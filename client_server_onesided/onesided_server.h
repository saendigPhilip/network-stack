//
// Created by philip on 05.04.21.
//

#ifndef CLIENT_SERVER_ONESIDED_SERVER_H
#define CLIENT_SERVER_ONESIDED_SERVER_H

#include "onesided_global.h"

int host_server(const char *ip, const char *port,
        void *shared_region, size_t shared_region_size, size_t block_size);

int accept_client();

void* server_get(const unsigned char *decryption_key,
        struct local_key_info *info, void *value);

int server_put(const unsigned char *encryption_key,
        struct local_key_info* info, const void *new_value);

unsigned char *setup_kv_store(const unsigned char *encryption_key,
        void* kv_store_plain,
        size_t num_values, size_t value_size,
        size_t *kv_store_size, struct local_key_info* infos);

void shutdown_rdma_server(void);

#endif //CLIENT_SERVER_ONESIDED_SERVER_H
