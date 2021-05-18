//
// Created by philip on 28.04.21.
//

#ifndef TWOSIDED_COMMUNICATION_TEST_COMMON_H
#define TWOSIDED_COMMUNICATION_TEST_COMMON_H

#include <cstdint>


static constexpr size_t TEST_MAX_KEY_SIZE = 256;
static constexpr size_t TEST_MAX_VAL_SIZE = 256;
static constexpr size_t TEST_KV_SIZE = 1024;

static constexpr size_t PUT_REQUESTS_PER_CLIENT = (1 << 16);
static constexpr size_t GET_REQUESTS_PER_CLIENT = (1 << 16);
static constexpr size_t DELETE_REQUESTS_PER_CLIENT = (1 << 16);
static constexpr uint8_t TOTAL_CLIENTS = 16;

static const unsigned char key_do_not_use[16] =
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};


/* We define a function to retrieve a key from a certain value,
 * so the KV-store is kept consistent and we can eventually perform
 * checks on the correctness of the returned and put values
 */
void value_from_key(void *value, const void *key, size_t key_len);

#endif //TWOSIDED_COMMUNICATION_TEST_COMMON_H
