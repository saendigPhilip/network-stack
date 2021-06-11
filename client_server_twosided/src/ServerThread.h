//
// Created by philip on 16.05.21.
//

#ifndef CLIENT_SERVER_TWOSIDED_SERVERTHREAD_H
#define CLIENT_SERVER_TWOSIDED_SERVERTHREAD_H
#include <thread>
#include "client_server_common.h"
#include "rpc.h"
#include "Server.h"

static constexpr ssize_t SEQ_THRESHOLD = 64;

class ServerThread {
private:
    erpc::Rpc<erpc::CTransport> *rpc_host;
    uint64_t next_seq;
    uint8_t client_id;
    bool stay_connected;
    std::thread running_thread;

    static void connect_and_work(ServerThread *st, erpc::Nexus *nexus,
            uint8_t erpc_id, size_t max_msg_size);

public:
    ServerThread(erpc::Nexus *nexus, int erpc_id, size_t max_msg_size,
            bool asynchronous = true);

    bool is_seq_valid(uint64_t sequence_number);

    uint64_t get_next_seq(uint64_t sequence_number, uint8_t operation);

    void enqueue_response(erpc::ReqHandle *handle, erpc::MsgBuffer *resp);

    void join();

    void terminate();

};



#endif //CLIENT_SERVER_TWOSIDED_SERVERTHREAD_H
