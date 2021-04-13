//
// Created by philip on 05.04.21.
//

#ifndef CLIENT_SERVER_ONESIDED_CLIENT_H
#define CLIENT_SERVER_ONESIDED_CLIENT_H

#include "onesided_global.h"

#define DEFAULT_LOCAL_SIZE 4096


int rdma_client_connect(const char *ip, const char *port, size_t max_val_size);

void rdma_disconnect();

void *rdma_get(const unsigned char *decryption_key,
        struct local_key_info *info, void *value);

int rdma_put(const unsigned char *encryption_key,
        struct local_key_info* info, const void *new_value);

#endif //CLIENT_SERVER_ONESIDED_CLIENT_H
