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

#define MIN(a,b) ((a) < (b) ? (a) : (b))

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

/* En-/decrypts data using EVP_CipherUpdate
 * Supports data lengths over (2^31 - 1) bytes
 * On success (i.e. if in_size bytes have been en-/decrypted without error), 0 is returned,
 * otherwise -1
 * */
int cipher_update(EVP_CIPHER_CTX *aes_ctx,
        const unsigned char *in, size_t in_size, unsigned char *out) {

    ssize_t total_processed_bytes = 0;
    int processed_bytes, to_process;

    while ((size_t) total_processed_bytes < in_size) {
        to_process = (int) MIN(INT32_MAX, in_size - total_processed_bytes);
        if (1 != EVP_CipherUpdate(aes_ctx, out, &processed_bytes, in, to_process)) {
            cerr << "Could not en-/decrypt payload" << endl;
            return -1;
        }
        if (processed_bytes < 0) {
            cerr << "Something went wrong while en-/decrypting" << endl;
            return -1;
        }
        total_processed_bytes += (size_t) processed_bytes;
        out += (size_t) processed_bytes;
        in += (size_t) processed_bytes;
    }
    if (total_processed_bytes < 0 || (size_t) total_processed_bytes != in_size) {
        cerr << "Number of en-/decrypted bytes doesn't match expected number" << endl;
        return -1;
    }
    return 0;
}


/**
 * Encrypts the header and the key/value that have to be placed in the corresponding
 * struct by the caller
 * @param encryption_key AES_GCM key to use for encryption
 * @param header Header data to encrypt
 * @param payload Payload data to encrypt
 * @param ciphertext Pointer to pointer where ciphertext is placed
 *          If NULL, memory for the ciphertext is allocated that has to be freed
 *          by the caller. In this case, on error, the memory is freed by this method
 * @return 0 on success, -1 on error
 */
int encrypt_message(const unsigned char *encryption_key, const struct rdma_msg_header *header,
        const struct rdma_enc_payload *payload, unsigned char **ciphertext) {
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

    size_t payload_len = header->key_len + payload->value_len;
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

    /* Encrypt key: */
    if (header->key_len > 0 && payload->key) {
        if (0 > cipher_update(aes_ctx, payload->key, header->key_len, ciphertext_pos))
            goto end_encrypt;

        ciphertext_pos += (size_t) header->key_len;
    }

    /* Encrypt value: */
    if (payload->value_len > 0 && payload->value) {
        if (0 > cipher_update(aes_ctx, payload->value, payload->value_len, ciphertext_pos))
            goto end_encrypt;

        ciphertext_pos += (size_t) payload->value_len;
    }

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

int allocate_and_decrypt(EVP_CIPHER_CTX *aes_ctx, unsigned char **payload,
        const unsigned char *ciphertext_pos, size_t expected_length, bool *to_free) {

    if (!*payload) {
        *payload = (unsigned char *) malloc((size_t) expected_length);
        if (!*payload) {
            cerr << "Memory allocation failure" << endl;
            return -1;
        }
        *to_free = true;
    }
    else
        *to_free = false;

    if (0 > cipher_update(aes_ctx, ciphertext_pos, expected_length, *payload))
        goto err_allocate_and_decrypt;

    return 0;

err_allocate_and_decrypt:
    if (*to_free) {
        free(*payload);
        *payload = NULL;
        *to_free = false;
    }
    return -1;
}

/**
 * Decrypts a message and stores the results of the decryption in a
 * rdma_msg_header and a rdma_dec_payload struct.
 * The pointers in the rdma_dec_payload struct are allocated by this method and
 * have to be freed by the caller. On error, this method frees the pointers itself
 * @param decryption_key AES-GCM Key to use for decryption
 * @param header Header struct to store the header information
 * @param payload Payload struct where key and value information are stored
 *          in newly allocated pointers
 * @param ciphertext Ciphertext to decrypt
 * @param ciphertext_len Length of ciphertext to decrypt
 * @return 0 on success, -1 on error
 */
int decrypt_message(const unsigned char *decryption_key, struct rdma_msg_header *header,
        struct rdma_dec_payload *payload, const unsigned char *ciphertext, size_t ciphertext_len) {

    int ret = -1;
    if (!(decryption_key && header && ciphertext && payload && ciphertext_len >= MIN_MSG_LEN)) {
        cerr << "decrypt_message: Invalid parameters" << endl;
        return -1;
    }
    size_t expected_payload_len = PAYLOAD_SIZE(ciphertext_len);
    int64_t expected_value_len;
    size_t bytes_decrypted = 0;
    bool free_key = false, free_value = false;
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

    if (header->key_len > expected_payload_len) {
        cerr << "Invalid key length" << endl;
        goto end_decrypt;
    }
    /* Decrypt key: */
    if (header->key_len > 0) {
        if (0 > allocate_and_decrypt(aes_ctx, &(payload->key),
                ciphertext + bytes_decrypted, header->key_len, &free_key))
            goto end_decrypt;

        bytes_decrypted += header->key_len;
    }

    /* Decrypt value: */
    expected_value_len = expected_payload_len - header->key_len;
    if (expected_value_len > 0) {
        if (0 > allocate_and_decrypt(aes_ctx, &(payload->value),
                ciphertext + bytes_decrypted, expected_value_len, &free_value))
            goto end_decrypt;
    }

    /* Finish decryption: */
    if (1 != EVP_DecryptFinal_ex(aes_ctx, nullptr, &length)){
        cerr << "Could not finish decryption" << endl;
        goto end_decrypt;
    }
    payload->value_len = expected_value_len;
    ret = 0;

end_decrypt:
    if (ret) {
        if (free_key) {
            free(payload->key);
            payload->key = nullptr;
        }
        if (free_value) {
            free(payload->value);
            payload->value = nullptr;
        }
    }
    EVP_CIPHER_CTX_free(aes_ctx);
    return ret;
}