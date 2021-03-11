#include "common.h"
#ifndef RDMA_CLIENT
#define RDMA_CLIENT


class Client{
private:
    erpc::Rpc<erpc::CTransport> *client_rpc;
    erpc::Nexus *nexus;

    erpc::MsgBuffer req;
    erpc::MsgBuffer resp;

/* TODO: Use data structure for multiple sessions */
    int session_nr;

public:
    Client(std::string client_hostname, uint16_t udp_port);
    ~Client();
    int connect(std::string server_hostname, unsigned int udp_port, size_t try_iterations);
    int disconnect();
    int recv(char *address, size_t address_size, size_t length, unsigned int refresh_timeout);
    erpc::MsgBuffer *get_resp();

};

void cont_func(void *, void *);
void recv_response_handler(int, erpc::SmEventType, erpc::SmErrType, void *);

#endif //RDMA_CLIENT


