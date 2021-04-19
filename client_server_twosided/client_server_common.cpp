#include <endian.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <set>
#include <stdio.h>
#include <string.h>

#include "client_server_common.h"


std::set<uint64_t> *sequence_numbers;

/* *
 * Checks if a sequence number was already encountered by maintaining a set with
 * sequence numbers. Adds sequence number if not already contained
 * Returns 0, if the sequence number is valid, and a negative value if it was already encountered
 * */
int add_sequence_number(uint64_t sequence_number) {
    if (!sequence_numbers) {
        sequence_numbers = new std::set<uint64_t>;
        sequence_numbers->insert(sequence_number);
        return 0;
    }
    if (sequence_numbers->find(sequence_number) == sequence_numbers->end()) {
        sequence_numbers->insert(sequence_number);
        return 0;
    }
    return -1;
}

int encrypt_message(const unsigned char *encryption_key, const struct rdma_msg_header *header,
        unsigned char **ciphertext, const void *payload, size_t payload_len) {
    int ret = -1;
    int length;
    bool to_free = false;

    if (!(encryption_key && header && ciphertext && payload)){
        cerr << "encrypt_message: invalid parameters" << endl;
        return -1;
    }
    EVP_CIPHER_CTX *aes_ctx = EVP_CIPHER_CTX_new();
    if (!aes_ctx){
        cerr << "Memory allocation failure" << endl;
        return -1;
    }

    if (!*ciphertext) {
        *ciphertext = (unsigned char *) malloc(CIPHERTEXT_SIZE(payload_len));
        if (!*ciphertext) {
            cerr << "Memory allocation failure" << endl;
            return -1;
        }
        to_free = true;
    }
    unsigned char *ciphertext_pos = *ciphertext;

    if (1 != RAND_bytes(ciphertext_pos, IV_LEN)) {
        cerr << "encrypt_message: Could not generate IV" << endl;
        goto end_encrypt;
    }

    /* Initialize Encryption, set position of IV: */
    if (1 != EVP_EncryptInit_ex(aes_ctx,
            EVP_aes_128_gcm(), nullptr, encryption_key, ciphertext_pos)){
        cerr << "encrypt_message: Could not initialize encryption of IV" << endl;
        goto end_encrypt;
    }

    /* Set IV length: */
    if (1 != EVP_CIPHER_CTX_ctrl(aes_ctx, EVP_CTRL_AEAD_SET_IVLEN, IV_LEN, nullptr)){
        cerr << "encrypt_message: Could not set IV length" << endl;
        goto end_encrypt;
    }
    ciphertext_pos += IV_LEN;

    /* Encrypt seq_op and length: */
    if (1 != EVP_EncryptUpdate(aes_ctx, ciphertext_pos, &length,
            (unsigned char *)header, sizeof(struct rdma_msg_header))) {
        cerr << "Could not encrypt seq_op/key_len" << endl;
        goto end_encrypt;
    }
    ciphertext_pos += (size_t)length;

    /* Encrypt payload: */
    if (1 != EVP_EncryptUpdate(aes_ctx, ciphertext_pos, &length,
            (unsigned char *)payload, payload_len)) {
        cerr << "Could not encrypt payload" << endl;
        goto end_encrypt;
    }
    ciphertext_pos += (size_t)length;

    /* Write final encrypted data: */
    if (1 != EVP_EncryptFinal_ex(aes_ctx, ciphertext_pos, &length)){
        cerr << "Could not write final encrypted data" << endl;
        goto end_encrypt;
    }
    ciphertext_pos += (size_t) length;

    /* Write tag: */
    if (1 != EVP_CIPHER_CTX_ctrl(aes_ctx, EVP_CTRL_AEAD_GET_TAG, MAC_LEN, ciphertext_pos)) {
        cerr << "Could not write authentication tag" << endl;
        goto end_encrypt;
    }
    ret = 0;

end_encrypt:
    if (to_free && ret)
        free(*ciphertext);

    EVP_CIPHER_CTX_free(aes_ctx);
    return ret;
}

int decrypt_message(const unsigned char *decryption_key, struct rdma_msg_header *header,
        const unsigned char *ciphertext, void **payload, size_t ciphertext_len) {

    int ret = -1;
    if (!(decryption_key && header && ciphertext && payload && ciphertext_len >= MIN_MSG_LEN)) {
        cerr << "decrypt_message: Invalid parameters" << endl;
        return -1;
    }
    int expected_payload_len = PAYLOAD_SIZE(ciphertext_len);
    size_t bytes_decrypted = 0;
    bool to_free = false;
    int length;

    EVP_CIPHER_CTX *aes_ctx = EVP_CIPHER_CTX_new();
    if (!aes_ctx){
        cerr << "Memory allocation failure" << endl;
        return -1;
    }

    if (1 != EVP_DecryptInit_ex(aes_ctx, EVP_aes_128_gcm(), NULL,
            decryption_key, ciphertext + bytes_decrypted)){
        cerr << "decrypt_message: failed to initialize decryption" << endl;
        goto end_decrypt;
    }

    /* Set the location of the authentication tag: */
    if (1 != EVP_CIPHER_CTX_ctrl(aes_ctx, EVP_CTRL_AEAD_SET_TAG,
            MAC_LEN, (void *) (ciphertext + ciphertext_len - MAC_LEN))) {
        cerr << "decrypt_message: Could not set Tag location" << endl;
        goto end_decrypt;
    }

    /* Set the length of the IV: */
    if (1 != EVP_CIPHER_CTX_ctrl(aes_ctx, EVP_CTRL_AEAD_SET_IVLEN, IV_LEN, nullptr)){
        cerr << "decrypt_message: Could not set IV length" << endl;
        goto end_decrypt;
    }
    bytes_decrypted += IV_LEN;

    /* Decrypt seq_op and length: */
    if (1 != EVP_DecryptUpdate(aes_ctx, (unsigned char *) header,
            &length, ciphertext + bytes_decrypted, sizeof(struct rdma_msg_header))) {
        cerr << "Could not decrypt seq/op/length" << endl;
        goto end_decrypt;
    }
    if (length != sizeof(struct rdma_msg_header)) {
        cerr << "Seq_Op and length were not completely written to struct" << endl;
        goto end_decrypt;
    }
    bytes_decrypted += length;

    /* Decrypt payload, support payloads with length 0: */
    length = 0;
    if (expected_payload_len >= 0) {
        if (!*payload) {
            *payload = malloc((size_t) expected_payload_len);
            if (!*payload) {
                cerr << "Memory allocation failure" << endl;
                goto end_decrypt;
            }
            to_free = true;
        }
        if (1 != EVP_DecryptUpdate(aes_ctx, (unsigned char *) *payload, &length,
                ciphertext + bytes_decrypted, expected_payload_len)) {
            cerr << "Could not decrypt payload" << endl;
            goto end_decrypt;
        }
    }
    /* Finish decryption: */
    if (1 != EVP_DecryptFinal_ex(aes_ctx, (unsigned char *) *payload + length, &length)){
        cerr << "Could not finish decryption" << endl;
        goto end_decrypt;
    }
    ret = 0;

end_decrypt:
    if (ret && to_free) {
        free(*payload);
        *payload = nullptr;
    }
    EVP_CIPHER_CTX_free(aes_ctx);
    return ret;
}
