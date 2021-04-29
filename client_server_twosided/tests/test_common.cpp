#include <cstdint>
#include <cstring>
#include "test_common.h"

#define min(a, b) ((a) < (b) ? (a) : (b))


void value_from_key(void *value, const void *key, size_t key_len) {
    size_t to_copy;
    for (size_t i = 0; i < TEST_MAX_VAL_SIZE; i += to_copy) {
        to_copy = min(key_len, TEST_MAX_VAL_SIZE - i);
        memcpy(value, key, to_copy);
        value = (void *) ((uint8_t *)value + to_copy);
    }
}
