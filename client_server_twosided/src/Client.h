#ifndef RDMA_CLIENT
#define RDMA_CLIENT

#include <iostream>
using namespace std;

namespace anchor_client {
    typedef void status_callback(int, const void *);

    int connect(std::string server_hostname, unsigned int udp_port, 
                uint8_t id, const unsigned char *encryption_key);

    int disconnect();

    int get(const void *key, size_t key_len,
            void *value, size_t max_value_len, size_t *value_len,
            status_callback *callback, const void *user_tag, size_t loop_iterations=100);

    int put(const void *key, size_t key_len, const void *value, size_t value_len,
            status_callback *callback, const void *user_tag, size_t loop_iterations=100);

    int del(const void *key, size_t key_len,
            status_callback *callback, const void *user_tag, size_t loop_iterations=100);
}


#endif //RDMA_CLIENT


