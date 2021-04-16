#ifndef RDMA_SERVER
#define RDMA_SERVER

#include <iostream>
using namespace std;

int host_server(std::string hostname, uint16_t udp_port, size_t timeout_milliseconds);

void close_connection();



#endif // RDMA_SERVER