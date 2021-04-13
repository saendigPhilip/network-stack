//
// Created by philip on 05.04.21.
//

#ifndef CLIENT_SERVER_ONESIDED_CLIENT_H
#define CLIENT_SERVER_ONESIDED_CLIENT_H

#include "onesided_global.h"

#define DEFAULT_LOCAL_SIZE 4096


int rdma_client_connect(const char *ip, const char *port,
        const unsigned char *enc_key, size_t enc_key_length, size_t max_val_size);

void rdma_disconnect();

void *rdma_get(struct local_key_info *info, void *value);

int rdma_put(struct local_key_info *info, const void *new_value);

#endif //CLIENT_SERVER_ONESIDED_CLIENT_H
