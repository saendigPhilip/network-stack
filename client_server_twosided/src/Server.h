#ifndef RDMA_SERVER
#define RDMA_SERVER

#include <iostream>
using namespace std;

namespace anchor_server {
    typedef const void *(*get_function)(const void *key, size_t key_len, size_t *data_len);
    typedef int (*put_function)(const void *key, size_t key_len, void *value, size_t value_len);
    typedef int (*delete_function)(const void *key, size_t key_len);
    enum connection_status { CONNECTED, DISCONNECTED, ERROR };

    int host_server(
            std::string& hostname, uint16_t udp_port,
            const unsigned char *encryption_key,
            uint8_t number_threads, size_t num_bg_threads,
            size_t max_entry_size,
            get_function get, put_function put, delete_function del);

    void run_event_loop(size_t timeout);

    void run_event_loop_n_times(size_t n);

    void close_connection();
}



#endif // RDMA_SERVER