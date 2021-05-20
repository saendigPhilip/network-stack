//
// Created by philip on 28.04.21.
//

#ifndef TWOSIDED_COMMUNICATION_TEST_COMMON_H
#define TWOSIDED_COMMUNICATION_TEST_COMMON_H

#include <cstdint>

extern size_t KEY_SIZE;
extern size_t VAL_SIZE;
extern size_t KV_SIZE;
extern uint8_t NUM_CLIENTS;
extern size_t LOOP_ITERATIONS;

struct global_test_params {
    const size_t key_size;
    const size_t val_size;
    const size_t kv_size;
    const uint8_t num_clients;
    const size_t event_loop_iterations;
};
static constexpr size_t NUMBER_TESTS = 8;

/* Variable Loop iteration size: */
static constexpr struct global_test_params TEST_PARAMS[NUMBER_TESTS] = {
        {sizeof(size_t), 256, 64, 8, 32},
        {sizeof(size_t), 256, 64, 8, 64},
        {sizeof(size_t), 256, 64, 8, 128},
        {sizeof(size_t), 256, 64, 8, 256},
        {sizeof(size_t), 256, 64, 8, 512},
        {sizeof(size_t), 256, 64, 8, 1024},
        {sizeof(size_t), 256, 64, 8, 2048},
        {sizeof(size_t), 256, 64, 8, 4096}
};


/* Variable Value size: */ /*
static constexpr struct global_test_params TEST_PARAMS[NUMBER_TESTS] = {
        {sizeof(size_t), 64, 64, 8, 1000},
        {sizeof(size_t), 128, 64, 8, 1000},
        {sizeof(size_t), 256, 64, 8, 1000},
        {sizeof(size_t), 512, 64, 8, 1000},
        {sizeof(size_t), 1024, 64, 8, 1000},
        {sizeof(size_t), 2048, 64, 8, 1000},
        {sizeof(size_t), 4096, 64, 8, 1000},
        {sizeof(size_t), 8192, 64, 8, 1000}
};
*/


/* Variable KV-store size: */ /*
static constexpr struct global_test_params TEST_PARAMS[NUMBER_TESTS] = {
        {sizeof(size_t), 256, 1, 8, 1000},
        {sizeof(size_t), 256, 4, 8, 1000},
        {sizeof(size_t), 256, 16, 8, 1000},
        {sizeof(size_t), 256, 64, 8, 1000},
        {sizeof(size_t), 256, 256, 8, 1000},
        {sizeof(size_t), 256, 1024, 8, 1000},
        {sizeof(size_t), 256, 4096, 8, 1000},
        {sizeof(size_t), 256, 16384, 8, 1000}
};
*/


/* Variable Number of Threads: */ /*
static constexpr struct global_test_params TEST_PARAMS[NUMBER_TESTS] = {
        {sizeof(size_t), 256, 64, 1, 1000},
        {sizeof(size_t), 256, 64, 2, 1000},
        {sizeof(size_t), 256, 64, 4, 1000},
        {sizeof(size_t), 256, 64, 8, 1000},
        {sizeof(size_t), 256, 64, 16, 1000}
};
 */


static constexpr size_t PUT_REQUESTS_PER_CLIENT = (1 << 10);
static constexpr size_t GET_REQUESTS_PER_CLIENT = (1 << 10);
static constexpr size_t DELETE_REQUESTS_PER_CLIENT = (1 << 10);

static constexpr size_t MAX_KEY_SIZE = 8192;
static constexpr size_t MAX_VAL_SIZE = 8192;

static const unsigned char key_do_not_use[16] =
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};


/* We define a function to retrieve a key from a certain value,
 * so the KV-store is kept consistent and we can eventually perform
 * checks on the correctness of the returned and put values
 */
void value_from_key(
        void *value, size_t value_len, const void *key, size_t key_len);

void fill_global_test_params(size_t test_num);


#endif //TWOSIDED_COMMUNICATION_TEST_COMMON_H
