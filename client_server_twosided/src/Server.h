#ifndef RDMA_SERVER
#define RDMA_SERVER

#include <iostream>
using namespace std;

namespace anchor_server {
    typedef const void *(*get_function)(const void *key, size_t key_len, size_t *data_len);
    typedef int (*put_function)(const void *key, size_t key_len, void *value, size_t value_len);
    typedef int (*delete_function)(const void *key, size_t key_len);

    int init(string& hostname, uint16_t udp_port);

    int host_server(
            const unsigned char *encryption_key,
            uint8_t number_threads, size_t number_clients,
            size_t max_entry_size, bool asynchronous,
            get_function get, put_function put, delete_function del);

    void close_connection(bool force);

    void terminate();
}



#endif // RDMA_SERVER