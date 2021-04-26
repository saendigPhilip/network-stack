#ifndef RDMA_CLIENT
#define RDMA_CLIENT

#include <iostream>
using namespace std;

typedef void status_callback(int, const void *);

int connect(std::string server_hostname, unsigned int udp_port, uint8_t id,
        size_t try_iterations);


int get_from_server(const void *key, size_t key_len,
        void *value, size_t max_value_len,
        status_callback *callback, const void *user_tag, unsigned int timeout);

int put_to_server(const void *key, size_t key_len, const void *value, size_t value_len,
        status_callback *callback, const void *user_tag, unsigned int timeout);

int delete_from_server(const char *key, size_t key_len,
        status_callback *callback, const void *user_tag, unsigned int timeout);

int disconnect();


#endif //RDMA_CLIENT


