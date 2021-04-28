#include <cstring>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "client_server_common.h"
#include "Server.h"
#include "simple_unit_test.h"
#include "test_common.h"

#define MAX_TEST_SIZE (1 << 16)

char *test_kv_store[TEST_KV_SIZE];

int test_pre_alloc(size_t key_size, size_t value_size,
        struct rdma_dec_payload *dec_payload, unsigned char **ciphertext) {

    dec_payload->key = nullptr;
    dec_payload->value = nullptr;
    *ciphertext = nullptr;

    if (key_size > 0) {
        dec_payload->key = (unsigned char *) malloc(key_size);
        if (!(dec_payload->key))
            return -1;
    }

    if (value_size > 0) {
        dec_payload->value = (unsigned char *) malloc(value_size);
        if (!(dec_payload->value))
            goto err_pre_alloc;
    }

    *ciphertext = (unsigned char *) malloc(CIPHERTEXT_SIZE(key_size + value_size));
    if (!ciphertext)
        goto err_pre_alloc;

    return 0;

err_pre_alloc:
    free(dec_payload->key);
    dec_payload->key = nullptr;
    free(dec_payload->value);
    dec_payload->value = nullptr;
    free(*ciphertext);
    *ciphertext = nullptr;
    return -1;
}

int single_test_encryption(bool pre_alloc,
        const unsigned char *key, size_t key_size,
        const unsigned char *value, size_t value_size) {

    int ret = -1;
    struct rdma_msg_header enc_header = { 40 | RDMA_GET, key_size };
    struct rdma_msg_header dec_header;
    struct rdma_enc_payload enc_payload = { key, value, value_size };
    struct rdma_dec_payload dec_payload;
    unsigned char *ciphertext;
    size_t ciphertext_size = CIPHERTEXT_SIZE(key_size + value_size);

    if (pre_alloc) {
        if (test_pre_alloc(key_size, value_size, &dec_payload, &ciphertext)) {
            cerr << "Memory allocation failure" << endl;
            goto end_test_encryption;
        }
    }
    else {
        dec_payload = {nullptr, nullptr, 0};
        ciphertext = nullptr;
    }

    int header_cmp, key_cmp, value_cmp;

    if (0 != encrypt_message(key_do_not_use, &enc_header, &enc_payload, &ciphertext)) {
        cerr << "Encryption failed" << endl;
        goto end_test_encryption;
    }
    if (0 != decrypt_message(key_do_not_use, &dec_header,
            &dec_payload, ciphertext, ciphertext_size)) {
        cerr << "Decryption failed" << endl;
        goto end_test_encryption;
    }

    header_cmp = memcmp(&enc_header, &dec_header, sizeof(struct rdma_msg_header));
    key_cmp = memcmp(key, dec_payload.key, dec_header.key_len);
    value_cmp = memcmp(value, dec_payload.value, dec_payload.value_len);
    if (header_cmp || key_cmp || value_cmp ||
            dec_header.key_len != key_size || dec_payload.value_len != value_size) {
        cerr << "En- or decryption doesn't work correctly yet..." << endl;
    }
    else {
        ret = 0;
    }

end_test_encryption:
    free(ciphertext);
    free(dec_payload.key);
    free(dec_payload.value);
    return ret;
}

void execute_tests(unsigned char *key, size_t key_size, unsigned char *value, size_t value_size) {
    EXPECT_EQUAL(0, single_test_encryption(false, key, key_size, nullptr, 0));
    EXPECT_EQUAL(0, single_test_encryption(true, key, key_size, nullptr, 0));
    EXPECT_EQUAL(0, single_test_encryption(false, nullptr, 0, value, value_size));
    EXPECT_EQUAL(0, single_test_encryption(true, nullptr, 0, value, value_size));
    EXPECT_EQUAL(0, single_test_encryption(false, key, key_size, value, value_size));
    EXPECT_EQUAL(0, single_test_encryption(true, key, key_size, value, value_size));
}


void test_encryption() {
    BEGIN_TEST_DELIMITER("encryption and decryption with NULL values");
    EXPECT_EQUAL(0, single_test_encryption(false, nullptr, 0, nullptr, 0));
    EXPECT_EQUAL(0, single_test_encryption(true, nullptr, 0, nullptr, 0));
    END_TEST_DELIMITER();

    unsigned char test_key[MAX_TEST_SIZE];
    unsigned char test_value[MAX_TEST_SIZE];
    /* The stack values are random enough... */
    size_t i = 1;
    BEGIN_TEST_DELIMITER("encryption and decryption with small payload sizes");
    for (; i <= 128; i++) {
        execute_tests(test_key, i, test_value, i + 1);
    }
    END_TEST_DELIMITER();

    BEGIN_TEST_DELIMITER("encryption and decryption with high payload sizes");
    for (; i < MAX_TEST_SIZE; i += 4096) {
        execute_tests(test_key, i, test_value, i + 1);
    }
    END_TEST_DELIMITER();

    BEGIN_TEST_DELIMITER("encryption and decryption with payload sizes over 2^31 bytes");
    size_t huge_test_key_size = (size_t) INT32_MAX + 100;
    size_t huge_test_value_size = (size_t) INT32_MAX + 99;

    unsigned char *huge_test_key = (unsigned char *) malloc(huge_test_key_size);
    unsigned char *huge_test_value = (unsigned char *) malloc(huge_test_value_size);
    if (!(huge_test_key && huge_test_value)) {
        cerr << "Memory allocation failure while testing with huge payload sizes" << endl;
        goto free_huge;
    }
    execute_tests(huge_test_key, huge_test_key_size, huge_test_value, huge_test_value_size);
    END_TEST_DELIMITER();

free_huge:
    free(huge_test_key);
    free(huge_test_value);

    PRINT_TEST_SUMMARY();
}

long int get_index(const char *key) {
    char *err;
    long int index = (size_t) strtol(key, &err, 0);
    if (*key != '\0' && *err == '\0' && index >= 0 && (size_t) index < TEST_KV_SIZE) {
        return index;
    }
    return -1;
}

void *kv_get(const void *key, size_t, size_t *data_len) {
    long int index = get_index((char *) key);
    if (index >= 0) {
        *data_len = strlen(test_kv_store[index]) + 1;
        return (void*) (test_kv_store + index);
    }
    else 
        return nullptr;
}


/* Dummy for put function */
int kv_put(const void *key, size_t, void *value, size_t) {
    long int index = get_index((char *) key);
    if (index >= 0) {
        free(test_kv_store[index]);
        test_kv_store[index] = (char *) value;
        return 0;
    }
    else 
        return -1;
}

int kv_delete(const void *key, size_t) {
    long int index = get_index((char *) key);
    if (index >= 0) {
        free(test_kv_store[index]);
        test_kv_store[index] = nullptr;
        return 0;
    }
    else 
        return -1;
}

int main(int argc, char *argv[]) {
    // test_encryption();
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <ip-address>" << endl;
        return -1;
    }
    int ret = -1;
    std::string ip(argv[1]);
    const uint16_t standard_udp_port = 31850;
    const uint8_t num_clients = 1;
    if (anchor_server::host_server(ip, standard_udp_port,
            key_do_not_use, num_clients, 0,
            TEST_MAX_KEY_SIZE + TEST_MAX_VAL_SIZE,
            kv_get, kv_put, kv_delete)) {
        cerr << "Failed to host server" << endl;
        return ret;
    }
    anchor_server::run_event_loop(100000);
    cout << "Shutting down server" << endl;

    anchor_server::close_connection();
    return 0;
}
