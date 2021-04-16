#include "client_server_common.h"
#include <endian.h>
#include <openssl/crypto.h>
#include <set>
#include <string.h>

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

/* TODO: For en- and decryption: simplify if possible (e.g. iv_len = 12, aad = NULL) */
/**
 * Encrypts plaintext_len bytes of plaintext using AES-128-GCM
 * Additionally authenticates aad_len bytes of data
 * Generates an IV of length iv_len and writes it to iv (has to provide iv_len bytes of space)
 * Uses 16-byte key for encryption and writes the encrypted data to ciphertext
 * Ciphertext has to provide plaintext_len + 16 bytes of space
 *
 * Returns 0 on success and a negative value if something went wrong
 */
int encrypt(
        const unsigned char *plaintext, size_t plaintext_len,
        unsigned char *iv, size_t iv_len,
        const unsigned char *key,
        unsigned char *tag, size_t tag_len,
        const unsigned char *aad, size_t aad_len,
        unsigned char *ciphertext){

    int ret = -1;
    int length;
    size_t block_size = 16;
    size_t encrypted_bytes = 0; /* TODO: Can be removed; only for checking */
    if (!(plaintext && key && ciphertext)){
        cerr << ERR_ENC << endl;
        return -1;
    }

    if (1 != RAND_bytes(iv, block_size)) {
        cerr << ERR_ENC << endl;
        return -1;
    }

    EVP_CIPHER_CTX *aes_ctx = EVP_CIPHER_CTX_new();
    if (!aes_ctx){
        cerr << ERR_ENC << endl;
        return -1;
    }

    if (1 != EVP_EncryptInit_ex(aes_ctx, EVP_aes_128_gcm(), NULL, key, iv)){
        ERR_print_errors_fp(stderr);
        cerr << ERR_ENC << endl;
        goto end_encrypt;
    }

    /* Set IV length: */
    if (1 != EVP_CIPHER_CTX_ctrl(aes_ctx, EVP_CTRL_AEAD_SET_IVLEN, iv_len, nullptr)){
        ERR_print_errors_fp(stderr);
        cerr << ERR_ENC << endl;
        goto end_encrypt;
    }

    /* Encrypt data: */
    if (1 != EVP_EncryptUpdate(
            aes_ctx, ciphertext, &length, plaintext, (int) plaintext_len)){
        cerr << ERR_ENC << endl;
        ERR_print_errors_fp(stderr);
        goto end_encrypt;
    } else
        encrypted_bytes = (size_t)length;

    /* Support additional authenticated data: */
    if (aad) {
        if (1 != EVP_EncryptUpdate(
                aes_ctx, NULL, NULL, aad, (int) aad_len)) {
            cerr << ERR_ENC << endl;
            ERR_print_errors_fp(stderr);
            goto end_encrypt;
        }
    }

    /* Write final encrypted data: */
    if (1 != EVP_EncryptFinal_ex(aes_ctx, ciphertext + encrypted_bytes, &length)){
        cerr << ERR_ENC << endl;
        ERR_print_errors_fp(stderr);
        goto end_encrypt;
    } else {
        encrypted_bytes += (size_t)length;
        /* TODO: Debug Output. Remove after test */
        cout << "Plaintext length: " << plaintext_len <<
             ", Ciphertext length (encrypted): " << encrypted_bytes << endl;
    }

    /* Write tag: */
    if (1 != EVP_CIPHER_CTX_ctrl(aes_ctx, EVP_CTRL_AEAD_GET_TAG, tag_len, tag)) {
        cerr << ERR_ENC << endl;
        ERR_print_errors_fp(stderr);
    }
    else {
        ret = 0;
    }

end_encrypt:
    EVP_CIPHER_CTX_free(aes_ctx);

    return ret;
}


/**
 * Decrypts ciphertext_len bytes of ciphertext using AES-128-GCM
 * For AES-GCM, an iv of iv_len bytes and a 16 byte key is used
 * Additional authenticated data (aad) of size aad_len is supported
 * Authenticity is checked with a tag_len byte long GMAC tag
 * A pointer to the plaintext with ciphertext_len bytes has to be provided
 *
 * returns 0, if the plaintext could be decrypted successfully and a negative value otherwise
 */
int decrypt(
        const unsigned char *ciphertext, size_t ciphertext_len,
        const unsigned char *iv, size_t iv_len,
        const unsigned char *key,
        unsigned char *tag, size_t tag_len,
        const unsigned char *aad, size_t aad_len,
        unsigned char *plaintext) {

    int ret = -1;
    size_t decrypted_bytes = 0;
    if (!(ciphertext && iv && key && plaintext)) {
        cerr << ERR_DEC << endl;
        return -1;
    }

    EVP_CIPHER_CTX *aes_ctx = EVP_CIPHER_CTX_new();
    if (!aes_ctx){
        cerr << ERR_DEC << endl;
        return -1;
    }

    if (1 != EVP_DecryptInit_ex(aes_ctx, EVP_aes_128_gcm(), NULL, key, iv)){
        cerr << ERR_DEC << endl;
        ERR_print_errors_fp(stderr);
        goto end_decrypt;
    }

    /* Set the location of the authentication tag: */
    if (1 != EVP_CIPHER_CTX_ctrl(aes_ctx, EVP_CTRL_AEAD_SET_TAG, tag_len, tag)) {
        cerr << ERR_DEC << endl;
        ERR_print_errors_fp(stderr);
        goto end_decrypt;
    }

    /* Set the length of the IV: */
    if (1 != EVP_CIPHER_CTX_ctrl(aes_ctx, EVP_CTRL_AEAD_SET_IVLEN, iv_len, nullptr)){
        cerr << ERR_DEC << endl;
        ERR_print_errors_fp(stderr);
        goto end_decrypt;
    }

    /* Decrypt data: */
    int length;
    if (1 != EVP_DecryptUpdate(aes_ctx, plaintext, &length, ciphertext, ciphertext_len)){
        cerr << ERR_DEC << endl;
        ERR_print_errors_fp(stderr);
        goto end_decrypt;
    } else
        decrypted_bytes += length;

    /* Support additional authenticated data: */
    if (aad) {
        if (1 != EVP_DecryptUpdate(aes_ctx, nullptr, nullptr, aad, aad_len)) {
            cerr << ERR_DEC << endl;
            ERR_print_errors_fp(stderr);
            goto end_decrypt;
        }
    }

    /* Finish decryption: */
    if (1 != EVP_DecryptFinal_ex(aes_ctx, plaintext + decrypted_bytes, &length)){
        cerr << ERR_DEC << endl;
        ERR_print_errors_fp(stderr);
        goto end_decrypt;
    } else {
        decrypted_bytes += length;
        /* TODO: Debug Output. Remove after test */
        cout << "Ciphertext length: " << ciphertext_len <<
             ", Plaintext length (decrypted): " << decrypted_bytes << endl;
        ret = 0;
    }

end_decrypt:
    EVP_CIPHER_CTX_free(aes_ctx);
    return ret;
}

