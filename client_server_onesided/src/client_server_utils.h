//
// Created by philip on 02.02.21.
//
#ifndef NETWORK_STACK_UTILS_H
#define NETWORK_STACK_UTILS_H

#include <librpma.h>

#include "onesided_global.h"




#define DESCRIPTORS_MAX_SIZE 24

#define DEBUG

#ifdef DEBUG
#define PRINT_ERR(string) fprintf(stderr, "%s\n", (string))
#define PRINT_INFO(string) puts((string))
#else
#define PRINT_ERR(string)
#define PRINT_INFO(string)
#endif // DEBUG


extern const char *usage_string;
extern char *hostname;
extern char *port;

extern struct rpma_peer *peer;


/* persistent memory address structure from librpma:
 * https://github.com/pmem/rpma/blob/master/examples/common/common-conn.h
 */
struct common_data {
    uint16_t data_offset;   /* user data offset */
    uint8_t mr_desc_size;   /* size of mr_desc in descriptors[] */
    uint8_t pcfg_desc_size; /* size of pcfg_desc in descriptors[] */
    /* buffer containing mr_desc and pcfg_desc */
    char descriptors[DESCRIPTORS_MAX_SIZE];
};

struct endpoint_data {
    struct rpma_peer *peer;
    struct rpma_ep *endpoint;
    struct rpma_mr_local *memory_region;
    size_t memory_region_size;
    struct rpma_conn_cfg *config;
    struct rpma_conn *connection;
    size_t block_size;
    const unsigned char *enc_key;
    size_t enc_key_length;
};
extern struct endpoint_data ep_data;


struct key_value_data {
    uint64_t sequence_number;
    void *key;
    size_t key_size;
    void *value;
    size_t value_offset;
};


int initialize_peer(const char *ip, int server);

int setup_encryption(const unsigned char *enc_key, size_t key_length,
        size_t max_value_size);

void *alligned_malloc(size_t size);

int establish_connection(struct rpma_conn *connection);


void free_endpoint_data();

int get_sequence_number(uint64_t *seq);

void *onesided_get(
        unsigned char *(*read_function)(size_t),
        struct local_key_info *info,
        void *value
);

int onesided_put(
        unsigned char *ciphertext_buf,
        int (*write_function)(size_t),
        const struct local_key_info *info,
        const void *new_value
);


#endif //NETWORK_STACK_UTILS_H
