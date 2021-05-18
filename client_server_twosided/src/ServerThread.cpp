//
// Created by philip on 16.05.21.
//

#include <thread>
#include "rpc.h"
#include "ServerThread.h"


ServerThread::ServerThread(erpc::Nexus *nexus,
        int erpc_id, size_t max_msg_size, bool asynchronous) {
    this->client_id = erpc_id; // TODO: This is not secure. Find better solution
    this->next_seq = 0;
    this->stay_connected = true;

    if (asynchronous)
        this->running_thread = std::thread(
                connect_and_work, this, nexus, erpc_id, max_msg_size);
    else
        connect_and_work(this, nexus, erpc_id, max_msg_size);
}

void ServerThread::connect_and_work(ServerThread *st,
        erpc::Nexus *nexus, uint8_t erpc_id, size_t max_msg_size) {
    st->rpc_host = new erpc::Rpc<erpc::CTransport>(
            nexus, st, erpc_id, nullptr);
    st->rpc_host->set_pre_resp_msgbuf_size(max_msg_size);

    while (st->stay_connected)
        st->rpc_host->run_event_loop_once();
    delete st->rpc_host;
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
    if (id != this->client_id) {
        cerr << "Invalid Client ID" << endl;
        return false;
    }
    if (this->next_seq == 0) {
        this->next_seq = sequence_number & SEQ_MASK;
        return true;
    }
    if(SEQ_FROM_SEQ_OP(sequence_number & SEQ_MASK) - this->next_seq <
            SEQ_THRESHOLD) {
        this->next_seq = sequence_number & SEQ_MASK;
        return true;
    }
    else
        return false;
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
    this->running_thread.join();
}

void ServerThread::terminate() {
    this->stay_connected = false;
}
