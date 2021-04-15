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

const char *kv_plain_path = ".test_kv_store";
unsigned char *test_kv_plain;
size_t test_kv_plain_size;

const char *kv_enc_path = ".test_kv_store_enc";
unsigned char *test_kv_enc;
size_t test_kv_enc_size;


void free_local_key_info() {
    for (size_t i = 0; i < TEST_KV_NUM_ENTRIES; i++)
        free(test_key_infos[i].key);
}

/* Starts a Thread for the server and connects with a client in the current
 * Thread.
 * Joins the server thread and returns 0 on success or -1 otherwise
 */
int connect_client_server(const unsigned char *enc_key) {
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

/* Generates value entries and writes them to dest */
void generate_value_entries(void *dest) {
    for (size_t i = 0; i < TEST_KV_NUM_ENTRIES; i++){
        value_from_key(dest, (void *)&(i), sizeof(size_t));
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

int create_kv_plain_file() {
    test_kv_plain = (unsigned char *)pmem_map_file(kv_plain_path,
            TEST_KV_PLAIN_SIZE, PMEM_FILE_CREATE, 0666,
            &test_kv_plain_size, NULL);
    if (!test_kv_plain || test_kv_plain_size < TEST_KV_PLAIN_SIZE) {
        unmap_plain();
        fprintf(stderr, "Failed to create file %s\n", kv_plain_path);
        return -1;
    }
    generate_value_entries(test_kv_plain);
    return 0;
}

int create_kv_enc_file() {
    test_kv_enc = (unsigned char *) pmem_map_file(kv_enc_path, TEST_KV_ENC_SIZE,
            PMEM_FILE_CREATE, 0666, &test_kv_enc_size, NULL);
    if (!test_kv_enc || test_kv_enc_size < TEST_KV_ENC_SIZE) {
        unmap_enc();
        fprintf(stderr, "Failed to create file %s\n", kv_enc_path);
        return -1;
    }
    if (!setup_kv_store(test_enc_key, (size_t) TEST_ENC_KEY_SIZE,
            test_kv_enc, TEST_KV_ENC_SIZE, (void *) test_kv_plain,
            TEST_KV_NUM_ENTRIES, TEST_KV_MAX_VAL_SIZE,
            &test_kv_enc_size, test_key_infos)) {

        unmap_enc();
        return -1;
    }
    return 0;
}

/* Creates the encrypted and unencrypted keystore file */
int create_kv_store_files() {
    if (0 > create_kv_plain_file())
        return -1;
    if (0 > create_kv_enc_file()) {
        unmap_plain();
        return -1;
    }
    return 0;
}

int setup_kv_stores() {
    printf("Trying to read keystore file at %s\n", kv_plain_path);
    test_kv_plain = (unsigned char *)pmem_map_file(kv_plain_path, 0, 0, 0,
            &test_kv_plain_size, NULL);
    if (!test_kv_plain || test_kv_plain_size < TEST_KV_PLAIN_SIZE) {
        printf("Could not read KV-store at %s. "
               "Creating new plain and encrypted KV-store\n", kv_plain_path);
        unmap_plain();
        if (0 > create_kv_store_files()) {
            PRINT_ERR("Failed to create KV-store files");
            return -1;
        }
        else
            return 0;
    }
    printf("Trying to read encrypted keystore file at %s\n", kv_enc_path);
    test_kv_enc = (unsigned char *) pmem_map_file(kv_enc_path, 0, 0, 0,
            &test_kv_enc_size, NULL);
    if (!test_kv_enc || test_kv_enc_size < TEST_KV_PLAIN_SIZE) {
        printf("Could not read encrypted KV-store at %s. "
               "Creating new encrypted KV-store\n", kv_enc_path);
        unmap_enc();
        return create_kv_enc_file();
    }
    else
        return 0;
}

int test_init_all(int argc, char **argv) {
    if (0 > parseargs(argc, argv))
        return -1;

    return setup_kv_stores();
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
    /* Set up the local_key_info structures: */
    if (0 > setup_local_key_info(VALUE_ENTRY_SIZE(TEST_KV_MAX_VAL_SIZE)))
        return -1;

    if (!test_kv_enc) {
        PRINT_ERR("No encrypted KV store set up");
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

    BEGIN_TEST_DELIMITER("Server random access time without encryption");
    EXPECT_EQUAL(0, perform_test_get(server_get, iterations,
            num_accesses));
    END_TEST_DELIMITER();

    PRINT_INFO("Client tests can be executed now. Client may connect");
    if (0 > accept_client()) {
        test_cleanup_plain();
        return -1;
    }

    wait_for_disconnect_event();
    test_cleanup_plain();


    if (0 > test_init_enc())
        return -1;

    if (0 > accept_client()) {
        test_cleanup_enc();
        return -1;
    }

    wait_for_disconnect_event();

    BEGIN_TEST_DELIMITER("Server random access time with encryption");
    EXPECT_EQUAL(0, perform_test_get(server_get, iterations,
            num_accesses));
    END_TEST_DELIMITER();

    PRINT_TEST_SUMMARY();
    test_cleanup_enc();
    exit(ret);
}
