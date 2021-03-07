#include "common.h"

erpc::Rpc<erpc::CTransport> *rpc_host = nullptr;
/**
 * Request handler for incoming send requests
 * Checks freshness and checksum
 * Then, writes data from the client to the specified address
 * TODO: implement
 * */
void req_handler_send(erpc::ReqHandle *req_handle, void *) {
}

/**
 * Request handler for incoming receive requests
 * Checks freshness and checksum
 * Then, transfers data at the specified address to the client
 * Not finished yet: No encryption, no authenticity control
 * */
void req_handler_recv(erpc::ReqHandle *req_handle, void *) {
    const erpc::MsgBuffer *incoming_buffer = req_handle->get_req_msgbuf();
    struct rdma_message request;
    uint8_t *request_data;

    size_t response_size;
    uint8_t *outgoing_data;
    size_t outgoing_data_size;

    if (!incoming_buffer) {
        fprintf(stderr, "Invalid incoming request. Buffer is null\n");
        return;
    }
    size_t request_data_size = incoming_buffer->get_data_size();
    if (request_data_size < MIN_MSG_LEN) {
        fprintf(stderr, "Invalid incoming request. Too short\n");
        return;
    }
    request_data = incoming_buffer->buf;

    /* Check checksum */
    if (!check_hash(request_data, request_data_size)) {
        fprintf(stderr, "Could not verify hash\n");
        return;
    }

    request.seq_op = be64toh(((uint64_t*) request_data)[0]);
    request.length = be64toh(((uint64_t*) request_data)[1]);
    request.payload = ((char *) request_data) + 16;

    unsigned int op = (unsigned int) (request.seq_op & 0x3);
    uint64_t seq = request.seq_op & ~(uint64_t) 0x3;

    if (op != RDMA_RECV) {
        fprintf(stderr, "Wrong operation\n");
        return;
    }
    /* Add sequence number to known sequence numbers and check for replays: */
    if (!add_sequence_number(seq)) {
        fprintf(stderr, "Invalid sequence number\n");
        return;
    }

    /* TODO: We're reading the data to outgoing_data and then copying it to the response buffer.
     *  More efficient solution?
     *   - Returning a message with payload of the requested size, indicate size at length field
     *      Overwrite remaining message with zeros (to avoid unauthorized memory read)
     * */
    /* Get the requested data: */
    size_t address_size = request_data_size - MIN_MSG_LEN;
    outgoing_data_size = request.length;
    outgoing_data = (uint8_t *) malloc(request.length);
    if (!outgoing_data) {
        fprintf(stderr, "Memory allocation failure\n");
        return;
    }
    auto &response = req_handle->pre_resp_msgbuf;
    if (!read_data(request.payload, address_size, &outgoing_data_size)) {
        fprintf(stderr, "Could not read data\n");
        goto end_req_handler_recv;
    }

    /* Create and enqueue the response: */
    response_size = MIN_MSG_LEN + request.length;
    rpc_host->resize_msg_buffer(&response, response_size);
    ((uint64_t *) response.buf)[0] = htobe64((seq + 4) | RDMA_RECV);
    ((uint64_t *) response.buf)[1] = htobe64(outgoing_data_size);
    (void *) memcpy(((uint8_t *) response.buf) + 16, outgoing_data, outgoing_data_size);
    if (calculate_hash(response.buf, outgoing_data_size + 16, response.buf + outgoing_data_size + 16)) {
        rpc_host->enqueue_response(req_handle, &response);
    }

end_req_handler_recv:
    free(outgoing_data);
}

void req_handler(erpc::ReqHandle *req_handle, void *) {
    auto &resp = req_handle->pre_resp_msgbuf;
    rpc_host->resize_msg_buffer(&resp, kMsgSize);
    sprintf(reinterpret_cast<char *>(resp.buf), "hello");

    rpc_host->enqueue_response(req_handle, &resp);
}


erpc::Rpc<erpc::CTransport> *host_server(std::string hostname, unsigned int port) {
    std::string server_uri = kServerHostname + ":" + std::to_string(kUDPPort);
    erpc::Nexus *nexus = nullptr;
    nexus = new erpc::Nexus(server_uri, 0, 0);
    if (!nexus) {
        fprintf(stderr, "Failed to host server\n");
        return nullptr;
    }
    nexus->register_req_func(RDMA_RECV, req_handler_recv);
    nexus->register_req_func(RDMA_SEND, req_handler_send);

    rpc_host = new erpc::Rpc<erpc::CTransport>(nexus, nullptr, 0, nullptr);
    if (!rpc_host)
        fprintf(stderr, "Failed to host server\n");
    return rpc_host;
}

void shutdown_server(erpc::Rpc<erpc::CTransport> *server){
    // delete server->nexus;
    delete server;
}

int main() {
    std::string server_uri = kServerHostname + ":" + std::to_string(kUDPPort);
    erpc::Nexus nexus(server_uri, 0, 0);
    nexus.register_req_func(RDMA_RECV, req_handler_recv);
    nexus.register_req_func(RDMA_SEND, req_handler_send);

    rpc_host = new erpc::Rpc<erpc::CTransport>(&nexus, nullptr, 0, nullptr);
    rpc_host->run_event_loop(100000);
}
