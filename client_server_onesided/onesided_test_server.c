#include <openssl/rand.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "onesided_server.h"
#include "onesided_test_all.h"
#include "simple_unit_test.h"

#define HOST_SERVER_SUCCESS (void *)0
#define HOST_SERVER_FAILED (void *)1

unsigned char test_kv_plain[TEST_KV_NUM_ENTRIES][TEST_KV_MAX_VAL_SIZE];
unsigned char *test_kv_enc;
size_t test_kv_enc_size;
struct local_key_info test_key_infos[TEST_KV_NUM_ENTRIES];
struct timespec start_time;
struct timespec end_time;

/*
char *get_hex(uint8_t *bytes, size_t length) {
    char *out = (char *)malloc(length * 2 * sizeof(char) + 1);
    if (!out)
        return out;

    for (size_t i = 0; i < length; i++) {
        sprintf(out + 2 * i, "%02x", bytes[i]);
    }
    return out;
}
*/

size_t min(size_t a, size_t b) {
    return a < b ? a : b;
}

long timediff() {
    return (end_time.tv_sec - start_time.tv_sec) * 1000000000 +
        end_time.tv_nsec - start_time.tv_nsec;
}

void value_from_key(void *value, const void *key, size_t key_len) {
    size_t to_copy;
    for (size_t i = 0; i < TEST_KV_MAX_VAL_SIZE; i += to_copy) {
        to_copy = min(key_len, TEST_KV_MAX_VAL_SIZE - i);
        memcpy(value, key, to_copy);
        value = (void *) ((uint8_t *)value + to_copy);
    }
}

int setup_local_key_info(size_t value_entry_size) {
    int seq_set;
    void *buf;
    for (size_t i = 0; i < TEST_KV_NUM_ENTRIES; i++) {
        buf = malloc(sizeof(size_t));
        seq_set = RAND_bytes((unsigned char *)&(test_key_infos[i].sequence_number),
                sizeof(test_key_infos[i].sequence_number));
        if (!(buf && 1 == seq_set)) {
            PRINT_ERR("Could not initialize local_key_info structures");
            for (size_t j = 0; j < i; j++)
                free(test_key_infos[i].key);
            return -1;
        }
        /* We're using the index as key: */
        *((size_t *)buf) = i;
        test_key_infos[i].key = buf;
        test_key_infos[i].key_size = sizeof(size_t);
        test_key_infos[i].value_offset = i * value_entry_size;
    }

    return 0;
}

void free_local_key_info() {
    for (size_t i = 0; i < TEST_KV_NUM_ENTRIES; i++)
        free(test_key_infos[i].key);
}

/* Starts a Thread for the server and connects with a client in the current
 * Thread.
 * Joins the server thread and returns 0 on success or -1 otherwise
 */
int connect_client_server(unsigned char *enc_key) {
    const unsigned char *kv_store;
    size_t shared_size;
    if (enc_key) { /* -> KV-store is encrypted */
        kv_store = test_kv_enc;
        shared_size = test_kv_enc_size;
    }
    else { /* -> KV-store is not encrypted */
        kv_store = (unsigned char *)test_kv_plain;
        shared_size = sizeof(test_kv_plain);
    }

    if (0 > host_server(ip, port, (void *)kv_store, shared_size,
            TEST_KV_MAX_VAL_SIZE, (unsigned char *)enc_key, TEST_ENC_KEY_SIZE))
        return -1;

    return 0;
}

int test_init_all(int argc, char **argv) {
    if (0 > parseargs(argc, argv))
        return -1;

    /* Generate value entries from keys: */
    for (size_t i = 0; i < TEST_KV_NUM_ENTRIES; i++){
        value_from_key((void *)test_kv_plain[i], (void *)&(i), sizeof(size_t));
    }
    return 0;
}

int test_init_plain() {
    if (0 > connect_client_server(NULL))
        return -1;
    return setup_local_key_info(TEST_KV_MAX_VAL_SIZE);
}

void test_cleanup_plain() {
    free_local_key_info();
    shutdown_rdma_server();
}

int test_init_enc() {
    /* Generate random key: */
    if (1 != RAND_bytes((unsigned char *)test_enc_key, TEST_ENC_KEY_SIZE)) {
        PRINT_ERR("Could not generate random key");
        return -1;
    }

    /* Set up the local_key_info structures: */
    if (0 > setup_local_key_info(VALUE_ENTRY_SIZE(TEST_KV_MAX_VAL_SIZE)))
        return -1;

    /* Set up the KV store by encrypting the structures: */
    test_kv_enc = setup_kv_store(test_enc_key, (size_t)TEST_ENC_KEY_SIZE,
            (void *)test_kv_plain, TEST_KV_NUM_ENTRIES, TEST_KV_MAX_VAL_SIZE,
            &test_kv_enc_size, test_key_infos);

    if (!test_kv_enc) {
        PRINT_ERR("Failed to set up KV store");
        goto err_test_init_enc;
    }

    if (0 > connect_client_server(test_enc_key))
        goto err_test_init_enc;

    return 0;

err_test_init_enc:
    free_local_key_info();
    return -1;
}

void test_cleanup_enc() {
    free_local_key_info();
    shutdown_rdma_server();
}


/*
 * Performs num_access local get-operations iterations times and prints a summary
 * Returns 0 on succes, -1 on failure
 */
int perform_test_local(int random_access, uint32_t iterations,
        uint64_t num_accesses) {
    unsigned char result_buf[TEST_KV_MAX_VAL_SIZE];
    unsigned char correct_val_buf[TEST_KV_MAX_VAL_SIZE];
    void *get_dest;
    size_t index;
    long total_time = 0;
    struct local_key_info *info;
    for (unsigned int i = 0; i < iterations; i++) {
        for (uint64_t j = 0; j < num_accesses; j++) {
            index = (random_access ? (size_t) rand() : j) % TEST_KV_NUM_ENTRIES;
            info = (struct local_key_info *) test_key_infos + index;

            (void) clock_gettime(CLOCK_MONOTONIC, &start_time);
            get_dest = server_get(info, (void *) result_buf);
            (void) clock_gettime(CLOCK_MONOTONIC, &end_time);

            if (!get_dest || get_dest != result_buf)
                return -1;
            value_from_key(correct_val_buf, info->key, info->key_size);

            if (0 != memcmp((void *) get_dest,
                    (void *) correct_val_buf, TEST_KV_MAX_VAL_SIZE))
                return -1;

            total_time += timediff();
        }
    }
    double average_time = (double)total_time /
            ((double)iterations * (double)num_accesses);
    printf("\nTotal time: %ld ns\nAverage time per access: %f ns\n",
            total_time, average_time);
    return 0;
}

int main(int argc, char **argv) {
    int ret = -1;

    if (0 > test_init_all(argc, argv)) {
        exit(-1);
    }
    if (0 > test_init_plain()) {
        exit(-1);
    }
    /*
     * The server and the client are connected now and we should be able to
     * perform operations normally on both sides as server and client
     * communicate asynchronously
     */
    uint64_t num_accesses = 0x40 * TEST_KV_NUM_ENTRIES;
    uint32_t iterations = 16;
    BEGIN_TEST_DELIMITER("Server serial access time without encryption");
    EXPECT_EQUAL(0, perform_test_local(0, iterations, num_accesses));
    END_TEST_DELIMITER();

    BEGIN_TEST_DELIMITER("Server random access time without encryption");
    EXPECT_EQUAL(0, perform_test_local(1, iterations, num_accesses));
    END_TEST_DELIMITER();

    test_cleanup_plain();


    if (0 > test_init_enc())
        return -1;

    BEGIN_TEST_DELIMITER("Server serial access time with encryption");
    EXPECT_EQUAL(0, perform_test_local(0, iterations, num_accesses));
    END_TEST_DELIMITER();

    BEGIN_TEST_DELIMITER("Server random access time with encryption");
    EXPECT_EQUAL(0, perform_test_local(1, iterations, num_accesses));
    END_TEST_DELIMITER();


    /*
     * if (!connect_client_server(NULL)) ...
     */

    PRINT_TEST_SUMMARY();
    test_cleanup_plain();
    exit(ret);
}
