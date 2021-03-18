#include "common.h"
#ifndef RDMA_CLIENT
#define RDMA_CLIENT



int connect(std::string server_hostname, unsigned int udp_port, size_t try_iterations);
int disconnect();
void free_client();
int get(char *address, size_t address_size, size_t length, unsigned int refresh_timeout);


void cont_func(void *, void *);
void get_response_handler(int, erpc::SmEventType, erpc::SmErrType, void *);

#endif //RDMA_CLIENT


