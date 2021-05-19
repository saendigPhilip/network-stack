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
        {64, 256, 64, 8, 32},
        {64, 256, 64, 8, 64},
        {64, 256, 64, 8, 128},
        {64, 256, 64, 8, 256},
        {64, 256, 64, 8, 512},
        {64, 256, 64, 8, 1024},
        {64, 256, 64, 8, 2048},
        {64, 256, 64, 8, 4096}
};


/* Variable Value size: */ /*
static constexpr struct global_test_params TEST_PARAMS[NUMBER_TESTS] = {
        {64, 64, 64, 8, 1000},
        {64, 128, 64, 8, 1000},
        {64, 256, 64, 8, 1000},
        {64, 512, 64, 8, 1000},
        {64, 1024, 64, 8, 1000},
        {64, 2048, 64, 8, 1000},
        {64, 4096, 64, 8, 1000},
        {64, 8192, 64, 8, 1000}
};
*/


static constexpr size_t PUT_REQUESTS_PER_CLIENT = (1 << 20);
static constexpr size_t GET_REQUESTS_PER_CLIENT = (1 << 20);
static constexpr size_t DELETE_REQUESTS_PER_CLIENT = (1 << 20);

static constexpr size_t MAX_KEY_SIZE = 8192;
static constexpr size_t MAX_VAL_SIZE = 8192;

static const unsigned char key_do_not_use[16] =
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};


/* We define a function to retrieve a key from a certain value,
 * so the KV-store is kept consistent and we can eventually perform
 * checks on the correctness of the returned and put values
 */
void value_from_key(void *value, const void *key, size_t key_len);

void fill_global_test_params(size_t test_num);


#endif //TWOSIDED_COMMUNICATION_TEST_COMMON_H
