#include "common.h"
#ifndef RDMA_SERVER
#define RDMA_SERVER

class Server{
private:
    erpc::Rpc<erpc::CTransport> *rpc_host;
    erpc::Nexus *nexus;

public:
    Server(std::string hostname, uint16_t port);
    ~Server();

    erpc::Rpc<erpc::CTransport> *get_rpc_host();

    /* TODO: Write method to register memory region for sharing */
};

void req_handler_recv(erpc::ReqHandle *req_handle, void *);


#endif // RDMA_SERVER