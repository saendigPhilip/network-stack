//
// Created by philip on 16.05.21.
//

#ifndef CLIENT_SERVER_TWOSIDED_SERVERTHREAD_H
#define CLIENT_SERVER_TWOSIDED_SERVERTHREAD_H
#include <thread>
#include "client_server_common.h"
#include "rpc.h"
#include "Server.h"

class ServerThread {
private:
    erpc::Rpc<erpc::CTransport> *rpc_host;
    uint64_t next_seq;
    uint8_t client_id;
    enum anchor_server::connection_status connected;
    std::thread *running_thread;

    static void connect_and_work(ServerThread *st);



public:
    ServerThread(erpc::Nexus *nexus, int erpc_id, size_t max_msg_size);

    ~ServerThread();

    void set_connection_status(enum anchor_server::connection_status status);

    bool is_seq_valid(uint64_t sequence_number);

    uint64_t get_next_seq(uint64_t sequence_number, uint8_t operation);

    void enqueue_response(erpc::ReqHandle *handle, erpc::MsgBuffer *resp);

    void join();

    void terminate();

};



#endif //CLIENT_SERVER_TWOSIDED_SERVERTHREAD_H
