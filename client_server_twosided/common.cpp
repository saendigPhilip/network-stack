#include "common.h"
#include <endian.h>
#include <openssl/crypto.h>
#include <set>
#include <string.h>

std::set<uint64_t> *sequence_numbers;

/* *
 * Calculates a SHA-256 Hash of msg of length len and stores it in dest
 * Dest should have enough space for the hash i.e. should be 32 bytes long
 * */
bool calculate_hash(const void* msg, size_t len, unsigned char *dest) {
    if (!dest || !msg)
        return false;

    bool ret = false;
    EVP_MD_CTX *sha_ctx = EVP_MD_CTX_new();
    if (1 != EVP_DigestInit_ex(sha_ctx, EVP_sha256(), NULL)) {
        goto end_calculate_hash;
    }
    if (1 != EVP_DigestUpdate(sha_ctx, msg, len)) {
        goto end_calculate_hash;
    }
    if (1 == EVP_DigestFinal_ex(sha_ctx, dest, NULL)) {
        ret = true;
    }

end_calculate_hash:
    EVP_MD_CTX_free(sha_ctx);
    return ret;
}


/* *
 * Checks, if the SHA-256 hash of the first (len - 32) bytes of msg equals its last 32 bytes
 * */
bool check_hash(const void *msg, size_t len) {
    if (len < SHA256_DIGEST_LENGTH || !msg)
        return false;

    bool ret = false;
    unsigned char msg_hash[SHA256_DIGEST_LENGTH];
    if (!calculate_hash(msg, len - SHA256_DIGEST_LENGTH, msg_hash))
        return ret;

    /* Compare the calculated hash to the last 32 bytes of the message: */
    if (0 == CRYPTO_memcmp((void *) msg_hash, (void *) ((uint8_t*) msg + len - SHA256_DIGEST_LENGTH), SHA256_DIGEST_LENGTH)) {
        ret = true;
    }

    return ret;
}

/* *
 * Checks if a sequence number was already encountered by maintaining a set with
 * sequence numbers. Adds sequence number if not already contained
 * Returns true, if the sequence number is valid, and false if it was already encountered
 * */
bool add_sequence_number(uint64_t sequence_number) {
    if (sequence_number == 0)
        return false;
    if (!sequence_numbers) {
        sequence_numbers = new std::set<uint64_t>;
        sequence_numbers->insert(sequence_number);
        return true;
    }
    if (sequence_numbers->find(sequence_number) == sequence_numbers->end()) {
        sequence_numbers->insert(sequence_number);
        return true;
    }
    return false;
}

/* Dummy method for testing: We just interpret the address as a filename and read from the according file */
uint8_t *read_data(const char* address, size_t address_size, size_t *data_length) {
    if (!(address && data_length && address_size > 0 && *data_length > 0)) {
        return nullptr;
    }
    uint8_t *data = nullptr;
    FILE* file = fopen(address, "r");
    if (!file)
        return nullptr;

    data = (uint8_t *) malloc(*data_length);
    if (!data)
        goto end_read_data;

    *data_length = fread(data, 1, *data_length, file);


end_read_data:
    fclose(file);

    return data;
}

