//
// Created by philip on 05.04.21.
//

#ifndef CLIENT_SERVER_ONESIDED_SERVER_H
#define CLIENT_SERVER_ONESIDED_SERVER_H

#include "onesided_global.h"


unsigned char *setup_kv_store(
        const unsigned char *enc_key, size_t enc_key_length,
        void* kv_store_plain, size_t num_values, size_t value_size,
        size_t* kv_store_size, struct local_key_info* infos);

int host_server(const char *ip, const char *port,
        void *shared_region, size_t shared_region_size, size_t max_val_size,
        const unsigned char *enc_key, size_t enc_key_length);

int accept_client();

void* server_get(struct local_key_info *info, void *value);

int server_put(struct local_key_info* info, const void *new_value);

void shutdown_rdma_server(void);

#endif //CLIENT_SERVER_ONESIDED_SERVER_H
