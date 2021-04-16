#include "client_server_common.h"
#ifndef RDMA_SERVER
#define RDMA_SERVER


int host_server(std::string hostname, uint16_t udp_port, size_t timeout_milliseconds);
void close_connection();



#endif // RDMA_SERVER