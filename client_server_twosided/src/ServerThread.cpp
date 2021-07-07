//
// Created by philip on 16.05.21.
//

#include <thread>
#include "rpc.h"
#include "ServerThread.h"


/**
 * Constructs a ServerThread and starts to work
 * @param nexus Nexus needed for the eRPC connection
 * @param erpc_id ID for the Client to handle
 * @param max_msg_size Maximum Message possible request size
 * @param asynchronous If true, spawns a new Thread for working. Otherwise
 *      starts working in the current thread
 */
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


/**
 * Connects to a client and runs the event loop until a disconnect is requested
 * @param st ServerThread that should connect and work
 * @param nexus Public, shared Nexus object needed for eRPC connection
 * @param erpc_id ID for identification of the according client
 * @param max_msg_size Maximum possible incoming request size
 */
void ServerThread::connect_and_work(ServerThread *st,
        erpc::Nexus *nexus, uint8_t erpc_id, size_t max_msg_size) {

    st->rpc_host = new erpc::Rpc<erpc::CTransport>(
            nexus, st, erpc_id, nullptr);
    st->rpc_host->set_pre_resp_msgbuf_size(max_msg_size);

    while (likely(st->stay_connected))
        st->rpc_host->run_event_loop_once();
    for (size_t i = 0; i < ITER_BEFORE_TERMINATION; i++)
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
    if (unlikely(id != this->client_id)) {
        cerr << "Invalid Client ID" << endl;
        return false;
    }
    if (unlikely(this->next_seq == 0)) {
        this->next_seq = sequence_number & SEQ_MASK;
        return true;
    }
    auto seq_diff = std::abs(static_cast<ssize_t>(SEQ_FROM_SEQ_OP(
        sequence_number & SEQ_MASK) - SEQ_FROM_SEQ_OP(this->next_seq)));

    if (likely(seq_diff < (SEQ_THRESHOLD * 2))) {
        if (likely(seq_diff > 0))
            this->next_seq = sequence_number & SEQ_MASK;
        return true;
    }
    else {
        fprintf(stderr, "Expected: %lx, Got: %lx\n",
                this->next_seq, sequence_number & SEQ_MASK);
        return false;
    }
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
