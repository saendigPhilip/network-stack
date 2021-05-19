#include <cstdint>
#include <cstring>
#include <cassert>
#include "test_common.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

size_t KEY_SIZE;
size_t VAL_SIZE;
size_t KV_SIZE;
uint8_t NUM_CLIENTS;
size_t LOOP_ITERATIONS;

void value_from_key(void *value, const void *key, size_t key_len) {
    size_t to_copy;
    for (size_t i = 0; i < MAX_VAL_SIZE; i += to_copy) {
        to_copy = min(key_len, MAX_VAL_SIZE - i);
        memcpy(value, key, to_copy);
        value = (void *) ((uint8_t *)value + to_copy);
    }
}

void fill_global_test_params(size_t test_num) {
    struct global_test_params param = TEST_PARAMS[test_num];
    assert(param.val_size <= MAX_VAL_SIZE);
    KEY_SIZE = param.key_size;
    VAL_SIZE = param.val_size;
    KV_SIZE = param.kv_size;
    NUM_CLIENTS = param.num_clients;
    LOOP_ITERATIONS = param.event_loop_iterations;
}