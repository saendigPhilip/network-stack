#ifndef RDMA_CLIENT
#define RDMA_CLIENT

#include <iostream>
using namespace std;


int connect(std::string server_hostname, unsigned int udp_port,
        size_t try_iterations);

int get_from_server(const char *key, size_t key_len,
        size_t max_value_length, void *value,
        void (callback)(void *, void *),
        unsigned int timeout);

int disconnect();
void free_client();

void verbose_cont_func(void *, void *);
// void get_response_handler(int, erpc::SmEventType, erpc::SmErrType, void *);

typedef void status_callback(int);

#endif //RDMA_CLIENT


