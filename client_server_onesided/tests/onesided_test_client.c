//
// Created by philip on 14.04.21.
//
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "onesided_client.h"
#include "onesided_test_all.h"
#include "simple_unit_test.h"


/*
 * Performs num_access get-operations iterations times and prints a summary
 * Returns 0 on success, -1 on failure
 */
int perform_test_put() {
    unsigned char result_buf[TEST_KV_MAX_VAL_SIZE];
    unsigned char new_val_buf[TEST_KV_MAX_VAL_SIZE];
    long total_time = 0;
    int ret;
    struct local_key_info *info;
    for (uint32_t i = 0; i < iterations; i++) {
        printf("Iteration %u of %u\r", i + 1, iterations);
        fflush(stdout);
        for (uint64_t j = 0; j < num_accesses; j++) {
            info = (struct local_key_info *) test_key_infos +
                    ((size_t) rand() % TEST_KV_NUM_ENTRIES);
            /* Create a new random value: */
            for (size_t i = 0; i < TEST_KV_MAX_VAL_SIZE / 4; i++) {
                ((int32_t *)new_val_buf)[i] = rand();
            }

            (void) clock_gettime(CLOCK_MONOTONIC, &start_time);
            ret = rdma_put(info, (void *) new_val_buf);
            (void) clock_gettime(CLOCK_MONOTONIC, &end_time);

            if (0 > ret)
                return -1;

            /* Check if the value has indeed arrived at the server: */
            if (result_buf != rdma_get(info, result_buf))
                return -1;

            if (0 != memcmp((void *) result_buf,
                    (void *) new_val_buf, TEST_KV_MAX_VAL_SIZE)) {
                PRINT_ERR("Value at server is not the one we put there");
                return -1;
            }

            /* Put the old value back TODO: This might not be necessary */
            value_from_key(new_val_buf, info->key, info->key_size);
            if (0 > rdma_put(info, (void *) new_val_buf))
                return -1;

            total_time += timediff();
        }
    }
    double average_time = (double)total_time /
                          ((double)iterations * (double)num_accesses);
    printf("\n\nTotal time: %ld ns "
           "(Number of iterations: %u, Number of accesses per iteration: %lu)\n"
           "Average time per access: %f ns\n",
            total_time, iterations, num_accesses, average_time);
    return 0;
}


int test_init_all(int argc, char **argv) {
    return parseargs(argc, argv);
}

int test_init_plain() {
    if (0 > setup_local_key_info(TEST_KV_MAX_VAL_SIZE))
        return -1;
    return rdma_client_connect(ip, port, NULL, 0, TEST_KV_MAX_VAL_SIZE);
}

int test_init_enc() {
    return setup_local_key_info(VALUE_ENTRY_SIZE(TEST_KV_MAX_VAL_SIZE));
}

void test_cleanup_plain() {
    rdma_disconnect();
    free_local_key_info();
}

void test_cleanup_enc() {
    rdma_disconnect();
    free_local_key_info();
}

int main(int argc, char **argv) {
    if (0 > test_init_all(argc, argv))
        return -1;

    if (0 > test_init_plain())
        return -1;


    BEGIN_TEST_DELIMITER("Client put random access time without encryption");
    EXPECT_EQUAL(0, perform_test_put(rdma_get, iterations, num_accesses));
    END_TEST_DELIMITER();

    BEGIN_TEST_DELIMITER("Client get random access time without encryption");
    EXPECT_EQUAL(0, perform_test_get(rdma_get, iterations, num_accesses));
    END_TEST_DELIMITER();

    test_cleanup_plain();

    do {
        sleep(5);
    }
    while(0 > rdma_client_connect(ip, port, test_enc_key, TEST_ENC_KEY_SIZE,
            TEST_KV_MAX_VAL_SIZE));

    if (0 > test_init_enc())
        return -1;

    BEGIN_TEST_DELIMITER("Client put random access time with encryption");
    EXPECT_EQUAL(0, perform_test_put(rdma_get, iterations, num_accesses));
    END_TEST_DELIMITER();

    BEGIN_TEST_DELIMITER("Client get random access time with encryption");
    EXPECT_EQUAL(0, perform_test_get(rdma_get, iterations, num_accesses));
    END_TEST_DELIMITER();

    test_cleanup_enc();

    PRINT_TEST_SUMMARY();

    return 0;
}

