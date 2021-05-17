//
// Created by philip on 16.05.21.
//

#include <thread>
#include "rpc.h"
#include "ServerThread.h"


void server_sm_handler(int, erpc::SmEventType type,
        erpc::SmErrType error, void *context);


ServerThread::ServerThread(
        erpc::Nexus *nexus, int erpc_id, size_t max_msg_size) {
    this->client_id = erpc_id; // TODO: This is not secure. Find better solution
    this->next_seq = 0;
    this->connected = anchor_server::DISCONNECTED;

    this->running_thread = new std::thread(
            connect_and_work, this, nexus, erpc_id, max_msg_size);
}

void ServerThread::connect_and_work(ServerThread *st,
        erpc::Nexus *nexus, uint8_t erpc_id, size_t max_msg_size) {
    st->rpc_host = new erpc::Rpc<erpc::CTransport>(
            nexus, st, erpc_id, server_sm_handler);
    st->rpc_host->set_pre_resp_msgbuf_size(max_msg_size);

    while (st->connected == anchor_server::DISCONNECTED)
        st->rpc_host->run_event_loop_once();
    if (st->connected == anchor_server::ERROR)
        throw std::runtime_error("Connection failure");
    while (st->connected)
        st->rpc_host->run_event_loop_once();
}


void server_sm_handler(int, erpc::SmEventType type,
        erpc::SmErrType error, void *context) {

    auto server_thread = static_cast<ServerThread *>(context);
    if (error != erpc::SmErrType::kNoError) {
        server_thread->set_connection_status(anchor_server::ERROR);
        return;
    }
    if (type == erpc::SmEventType::kConnected)
        server_thread->set_connection_status(anchor_server::CONNECTED);
    else if (type == erpc::SmEventType::kDisconnected)
        server_thread->set_connection_status(anchor_server::DISCONNECTED);
    else
        server_thread->set_connection_status(anchor_server::ERROR);
}


void ServerThread::set_connection_status(
        enum anchor_server::connection_status status) {
    this->connected = status;
}

ServerThread::~ServerThread() {
    delete this->running_thread;
    delete this->rpc_host;
}

void ServerThread::enqueue_response(erpc::ReqHandle *handle,
        erpc::MsgBuffer *resp) {
    this->rpc_host->enqueue_response(handle, resp);
}

/* *
 * Checks if sequence number is valid by checking whether we expect the
 * according sequence number from the according client
 * Returns 0, if the sequence number is valid, -1 otherwise
 * */
bool ServerThread::is_seq_valid(uint64_t sequence_number) {
    uint8_t id = ID_FROM_SEQ_OP(sequence_number);
    if ((size_t) id != this->client_id) {
        cerr << "Invalid Client ID" << endl;
        return false;
    }
    if (this->next_seq == 0) {
        this->next_seq = sequence_number & SEQ_MASK;
        return true;
    }
    return (sequence_number & SEQ_MASK) == this->next_seq;
}

/*
 * Returns the next sequence number and updates the next expected sequence
 * number for the according client
 * Should only be called with already checked sequence numbers
 */
uint64_t ServerThread::get_next_seq(uint64_t sequence_number, uint8_t operation) {
    uint64_t ret = NEXT_SEQ(sequence_number);
    this->next_seq = NEXT_SEQ(ret & SEQ_MASK);
    return SET_OP(ret, operation);
}

void ServerThread::join() {
    this->running_thread->join();
}

void ServerThread::terminate() {
    this->set_connection_status(anchor_server::PERFORM_DISCONNECT);
}
