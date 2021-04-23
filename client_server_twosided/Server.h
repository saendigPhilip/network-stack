#ifndef RDMA_SERVER
#define RDMA_SERVER

#include <iostream>
using namespace std;

typedef void *(*get_function) (const void *key, size_t key_len, size_t *data_len);
typedef int (*put_function)(const void *key, size_t key_len, const void *value, size_t value_len);
typedef int (*delete_function)(const void *key, size_t key_len);


int host_server(std::string hostname, uint16_t udp_port, size_t timeout_milliseconds,
        const unsigned char *encryption_key, uint8_t number_clients,
        get_function get, put_function put, delete_function del);


void close_connection();



#endif // RDMA_SERVER