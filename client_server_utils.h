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


/*
 *
 *
 * Limited by the maximum length of the private data
 * for rdma_connect() in case of RDMA_PS_TCP (56 bytes).
 */
#define DESCRIPTORS_MAX_SIZE 24

/* structure from librpma:
 * https://github.com/pmem/rpma/blob/master/examples/common/common-conn.h
 */
struct common_data {
    uint16_t data_offset;   /* user data offset */
    uint8_t mr_desc_size;   /* size of mr_desc in descriptors[] */
    uint8_t pcfg_desc_size; /* size of pcfg_desc in descriptors[] */
    /* buffer containing mr_desc and pcfg_desc */
    char descriptors[DESCRIPTORS_MAX_SIZE];
};


int parseargs(int argc, char** argv);

int initialize_peer();

int establish_connection(struct rpma_conn* connection);

int wait_for_disconnect_event(struct rpma_conn* connection, int verbose);

void* malloc_aligned(size_t size);

#endif //NETWORK_STACK_UTILS_H
