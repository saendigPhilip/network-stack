#include "common.h"
#ifndef RDMA_CLIENT
#define RDMA_CLIENT



int connect(std::string server_hostname, unsigned int udp_port, size_t try_iterations);
int disconnect();
void free_client();
int get_from_server(const char *key, size_t key_len, size_t expected_value_length, void* value, void *callback(),
        unsigned int timeout);


void cont_func(void *, void *);
void get_response_handler(int, erpc::SmEventType, erpc::SmErrType, void *);

#endif //RDMA_CLIENT


