//
// Created by philip on 14.04.21.
//

#ifndef CLIENT_SERVER_ONESIDED_ONESIDED_TEST_ALL_H
#define CLIENT_SERVER_ONESIDED_ONESIDED_TEST_ALL_H

#include <stdio.h>


#define PRINT_ERR(string) fprintf(stderr, "%s\n", (string))
#define PRINT_INFO(string) puts(string)

#define TEST_ENC_KEY_SIZE 16
#define TEST_KV_MAX_VAL_SIZE 0x400
#define TEST_KV_NUM_ENTRIES 0x8000


extern char *ip, *port;
unsigned char test_enc_key[TEST_ENC_KEY_SIZE];

int parseargs(int argc, char **argv);



#endif //CLIENT_SERVER_ONESIDED_ONESIDED_TEST_ALL_H
