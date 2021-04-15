//
// Created by philip on 14.04.21.
//
#include "unistd.h"

#include "onesided_client.h"
#include "onesided_test_all.h"
#include "simple_unit_test.h"

int test_init_all(int argc, char **argv) {
    return parseargs(argc, argv);
}

int test_init_plain() {
    if (0 > setup_local_key_info(TEST_KV_MAX_VAL_SIZE))
        return -1;
    return rdma_client_connect(ip, port, NULL, 0, TEST_KV_MAX_VAL_SIZE);
}

int test_init_enc() {
    if (0 > setup_local_key_info(VALUE_ENTRY_SIZE(TEST_KV_MAX_VAL_SIZE)))
        return -1;
    return rdma_client_connect(ip, port, test_enc_key, TEST_ENC_KEY_SIZE,
            TEST_KV_MAX_VAL_SIZE);
}

void test_cleanup_plain() {
    rdma_disconnect();
}

void test_cleanup_enc() {
    rdma_disconnect();
}

int main(int argc, char **argv) {
    if (0 > test_init_all(argc, argv))
        return -1;

    if (0 > test_init_plain())
        return -1;

    BEGIN_TEST_DELIMITER("Client random access time without encryption");
    EXPECT_EQUAL(0, perform_test_get(rdma_get, iterations, num_accesses));
    END_TEST_DELIMITER();

    rdma_disconnect();

    sleep(5);

    if (0 > test_init_enc())
        return -1;

    BEGIN_TEST_DELIMITER("Client random access time with encryption");
    EXPECT_EQUAL(0, perform_test_get(rdma_get, iterations, num_accesses));
    END_TEST_DELIMITER();

    test_cleanup_enc();

    PRINT_TEST_SUMMARY();

    return 0;
}

