#include <libpmem.h>
#include <openssl/rand.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "onesided_server.h"
#include "onesided_test_all.h"
#include "simple_unit_test.h"

#define HOST_SERVER_SUCCESS (void *)0
#define HOST_SERVER_FAILED (void *)1

#define USE_PMEM

const char *kv_plain_path = ".test_kv_store_plain";
unsigned char *test_kv_plain = NULL;
size_t test_kv_plain_size;

const char *kv_enc_path = ".test_kv_store_enc";
unsigned char *test_kv_enc = NULL;
size_t test_kv_enc_size;


/* Starts a Thread for the server and connects with a client in the current
 * Thread.
 * Joins the server thread and returns 0 on success or -1 otherwise
 */
int connect_client_server(const unsigned char *enc_key) {
    const unsigned char *kv_store;
    size_t shared_size;
    if (enc_key) { /* -> KV-store is encrypted */
        kv_store = test_kv_enc;
        shared_size = TEST_KV_ENC_SIZE;
    }
    else { /* -> KV-store is not encrypted */
        kv_store = (unsigned char *)test_kv_plain;
        shared_size = TEST_KV_PLAIN_SIZE;
    }

    if (0 > host_server(ip, port, (void *)kv_store, shared_size,
            TEST_KV_MAX_VAL_SIZE, (unsigned char *)enc_key, TEST_ENC_KEY_SIZE))
        return -1;

    return 0;
}

/* Generates value entries and writes them to dest */
void generate_value_entries(void *dest) {
    for (size_t i = 0; i < TEST_KV_NUM_ENTRIES; i++){
        value_from_key(dest, (void *)&(i), sizeof(size_t));
        dest = ((uint8_t *)dest) + TEST_KV_MAX_VAL_SIZE;
    }
}

void unmap_plain() {
    if (test_kv_plain)
        (void)pmem_unmap(test_kv_plain, test_kv_plain_size);
}

void unmap_enc() {
    if (test_kv_enc)
        (void)pmem_unmap(test_kv_enc, test_kv_enc_size);
}

/*
 * Sets up a plaintext KV-store file. If there is one available and the size
 * matches, the existing file will be used. Otherwise, a new keystore will be
 * created along with an according file
 * returns 0 on success, -1 on failure
 */
int setup_kv_plain_file() {
    printf("Trying to read keystore file at %s\n", kv_plain_path);
    test_kv_plain = (unsigned char *)pmem_map_file(kv_plain_path, 0, 0, 0,
            &test_kv_plain_size, NULL);
    if (test_kv_plain && test_kv_plain_size >= TEST_KV_PLAIN_SIZE) {
        return 0;
    }

    printf("Could not read KV-store at %s. "
           "Creating new plain and encrypted KV-store\n", kv_plain_path);
    unmap_plain();
    test_kv_plain = (unsigned char *)pmem_map_file(kv_plain_path,
            TEST_KV_PLAIN_SIZE, PMEM_FILE_CREATE, 0666,
            &test_kv_plain_size, NULL);
    if (test_kv_plain && test_kv_plain_size >= TEST_KV_PLAIN_SIZE) {
        generate_value_entries(test_kv_plain);
        return 0;
    }

    unmap_plain();
    fprintf(stderr, "Failed to create file %s\n", kv_plain_path);
    return -1;
}

/*
 * Does the same as setup_kv_plain_file, only with an encrypted KV-store
 */
int setup_kv_enc_file() {
    printf("Trying to read encrypted keystore file at %s\n", kv_enc_path);
    test_kv_enc = (unsigned char *) pmem_map_file(kv_enc_path, 0, 0, 0,
            &test_kv_enc_size, NULL);
    if (test_kv_enc && test_kv_enc_size >= TEST_KV_ENC_SIZE)
        return 0;

    printf("Could not read encrypted KV-store at %s. "
           "Creating new encrypted KV-store\n", kv_enc_path);
    unmap_enc();
    test_kv_enc = (unsigned char *) pmem_map_file(kv_enc_path, TEST_KV_ENC_SIZE,
            PMEM_FILE_CREATE, 0666, &test_kv_enc_size, NULL);
    if (test_kv_enc && test_kv_enc_size >= TEST_KV_ENC_SIZE) {
        if (test_kv_enc ==
        setup_kv_store(test_enc_key, (size_t) TEST_ENC_KEY_SIZE,
                test_kv_enc, TEST_KV_ENC_SIZE, (void *) test_kv_plain,
                TEST_KV_NUM_ENTRIES, TEST_KV_MAX_VAL_SIZE,
                &test_kv_enc_size, test_key_infos)) {

            return 0;
        }
    }
    unmap_enc();
    fprintf(stderr, "Failed to set up encrypted KV-store at %s\n", kv_enc_path);
    return -1;
}

int setup_kv_plain() {
#ifdef USE_PMEM
    return setup_kv_plain_file();
#else
    test_kv_plain = malloc_aligned(TEST_KV_PLAIN_SIZE);
    if (!test_kv_plain)
        return -1;
    generate_value_entries(test_kv_plain);
    return 0;
#endif
}

int setup_kv_enc() {
#ifdef USE_PMEM
    return setup_kv_enc_file();
#else
    test_kv_enc = malloc_aligned(TEST_KV_ENC_SIZE);
    if (!test_kv_enc)
        return -1;
    if (test_kv_enc !=
            setup_kv_store(test_enc_key, (size_t) TEST_ENC_KEY_SIZE,
                    test_kv_enc, TEST_KV_ENC_SIZE, (void *) test_kv_plain,
                    TEST_KV_NUM_ENTRIES, TEST_KV_MAX_VAL_SIZE,
                    &test_kv_enc_size, test_key_infos)) {
        free(test_kv_enc);
        return -1;
    }
    return 0;
#endif
}


int test_init_all(int argc, char **argv) {
    return parseargs(argc, argv);
}


int test_init_plain() {
    if (0 > setup_local_key_info(TEST_KV_MAX_VAL_SIZE))
        return -1;
    if (0 > setup_kv_plain())
        goto err_test_init_plain;
    if (0 > connect_client_server(NULL))
        goto err_test_init_plain;
    return 0;

err_test_init_plain:
    free_local_key_info();
#ifdef USE_PMEM
    unmap_enc();
#else
    free(test_kv_plain);
    test_kv_plain = NULL;
#endif
    return -1;
}

void test_cleanup_plain() {
    free_local_key_info();
    shutdown_rdma_server();
}


int test_init_enc() {
    /* Set up the local_key_info structures: */
    if (0 > setup_local_key_info(VALUE_ENTRY_SIZE(TEST_KV_MAX_VAL_SIZE)))
        return -1;

    if (0 > setup_kv_enc()) {
        goto err_test_init_enc;
    }

    if (0 > connect_client_server(test_enc_key))
        goto err_test_init_enc;

    return 0;

err_test_init_enc:
    free_local_key_info();
#ifdef USE_PMEM
    unmap_plain();
    unmap_enc();
#else
    free(test_kv_plain);
    test_kv_plain = NULL;
    free(test_kv_enc);
    test_kv_enc = NULL;
#endif
    return -1;
}


void test_cleanup_enc() {
#ifdef USE_PMEM
    unmap_plain();
    unmap_enc();
#else
    free(test_kv_plain);
    test_kv_plain = NULL;
    free(test_kv_enc);
    test_kv_enc = NULL;
#endif
    free_local_key_info();
    shutdown_rdma_server();
}


int main(int argc, char **argv) {
    int ret = -1;

    if (0 > test_init_all(argc, argv)) {
        exit(-1);
    }
    if (0 > test_init_plain()) {
        exit(-1);
    }

    BEGIN_TEST_DELIMITER("Server random access time without encryption");
    EXPECT_EQUAL(0, perform_test_get(server_get, iterations,
            num_accesses));
    END_TEST_DELIMITER();

    PRINT_INFO("Client tests can be executed now. Client may connect");
    if (0 > accept_client()) {
        test_cleanup_plain();
        return -1;
    }
    PRINT_INFO("Client connected. Waiting for client to finish his tests");

    wait_for_disconnect_event();
    test_cleanup_plain();

    puts("Client disconnected. "
         "Assuming that client finished and continuing with further tests");


    if (0 > test_init_enc())
        return -1;

    BEGIN_TEST_DELIMITER("Server random access time with encryption");
    EXPECT_EQUAL(0, perform_test_get(server_get, iterations,
            num_accesses));
    END_TEST_DELIMITER();

    PRINT_INFO("Client tests can be executed now. Client may connect");
    if (0 > accept_client()) {
        test_cleanup_enc();
        return -1;
    }
    PRINT_INFO("Client connected. Waiting for client to finish his tests");

    wait_for_disconnect_event();

    PRINT_TEST_SUMMARY();
    test_cleanup_enc();
    exit(ret);
}
