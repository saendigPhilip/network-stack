//
// Created by philip on 05.04.21.
//

#ifndef CLIENT_SERVER_ONESIDED_ONESIDED_TEST_H
#define CLIENT_SERVER_ONESIDED_ONESIDED_TEST_H

#include <stdio.h>

#include "onesided_global.h"


#define PRINT_INFO(string) puts((string))
#define PRINT_ERR(string) fprintf(stderr, "%s\n", (string))

#define TEST_NUM_KV_ENTRIES 16
#define TEST_MAX_VALUE_SIZE 256
#define TEST_ENTRY_SIZE VALUE_ENTRY_SIZE(TEST_MAX_VALUE_SIZE)


typedef size_t TEST_KEY_TYPE;

extern char *ip, *port;
extern struct local_key_info local_key_infos[TEST_NUM_KV_ENTRIES];

static const unsigned char key_do_not_use[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};


int parseargs(int argc, char **argv);

struct local_key_info *get_key_info(const char *key);

void communicate(
        int (*get_function)(const char *key, char *value),
        int (*put_function)(const char *key, const char *value));


#endif //CLIENT_SERVER_ONESIDED_ONESIDED_TEST_H
