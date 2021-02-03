//
// Created by philip on 02.02.21.
//
#include <librpma.h>

#ifndef NETWORK_STACK_UTILS_H
#define NETWORK_STACK_UTILS_H

extern const char* usage_string;
extern char* hostname;
extern char* port;

extern struct rpma_peer* peer;

int parseargs(int argc, char** argv);

int initialize_peer();

int establish_connection(struct rpma_conn* connection);

#endif //NETWORK_STACK_UTILS_H
