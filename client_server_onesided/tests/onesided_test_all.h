//
// Created by philip on 14.04.21.
//

#ifndef CLIENT_SERVER_ONESIDED_ONESIDED_TEST_ALL_H
#define CLIENT_SERVER_ONESIDED_ONESIDED_TEST_ALL_H

#include <stdio.h>

#include "onesided_global.h"


#define PRINT_ERR(string) fprintf(stderr, "%s\n", (string))
#define PRINT_INFO(string) puts(string)

#define TEST_ENC_KEY_SIZE 16
#define TEST_KV_MAX_VAL_SIZE (0x400 - MIN_VALUE_SIZE)
#define TEST_KV_NUM_ENTRIES 0x8000

#define TEST_KV_PLAIN_SIZE (TEST_KV_MAX_VAL_SIZE * TEST_KV_NUM_ENTRIES)
#define TEST_KV_ENC_SIZE \
    (VALUE_ENTRY_SIZE(TEST_KV_MAX_VAL_SIZE) * TEST_KV_NUM_ENTRIES)


extern char *ip, *port;
static const uint64_t num_accesses = 0x10 * TEST_KV_NUM_ENTRIES;
static const uint32_t iterations = 8;

static const unsigned char test_enc_key[TEST_ENC_KEY_SIZE] =
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
struct local_key_info test_key_infos[TEST_KV_NUM_ENTRIES];

int parseargs(int argc, char **argv);

void value_from_key(void *value, const void *key, size_t key_len);

int setup_local_key_info(size_t value_entry_size);

int perform_test_get(
        void *(get_function)(struct local_key_info *, void *),
        uint32_t iterations, uint64_t num_accesses);

#endif //CLIENT_SERVER_ONESIDED_ONESIDED_TEST_ALL_H
