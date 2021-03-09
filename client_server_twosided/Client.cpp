#include "Client.h"

Client *client = nullptr;

erpc::Rpc<erpc::CTransport> Client::*client_rpc = nullptr;
erpc::Nexus Client::*nexus = nullptr;

/* TODO: Map from Sequence numbers to buffers for multiple requests */
/* TODO: Use data structure for multiple sessions */
int session_nr;

/* TODO: There has to be a better solution for passing the client to cont_func */
void cont_func(void *, void *) { printf("%s\n", client->get_resp()->buf); }

void sm_handler(int, erpc::SmEventType, erpc::SmErrType, void *) {}

Client::Client(std::string client_hostname, uint16_t udp_port) {
    std::string client_uri = client_hostname + ":" + std::to_string(udp_port);
    nexus = new erpc::Nexus(client_uri, 0, 0);
    if (RAND_status() != 1) {
        if (RAND_poll() != 1) {
            /* TODO: Handle this error (exception?) */
            fprintf(stderr, "Couldn't initialize RNG\n");
        }
    }
}

Client::~Client() {
    /* TODO: What about freeing Nexus and the message buffers? */
    delete client_rpc;
}


int Client::connect(std::string server_hostname, unsigned int udp_port, size_t try_iterations) {
    std::string server_uri = server_hostname + ":" + std::to_string(udp_port);
    /* TODO: Can we use the id? */
    session_nr = client_rpc->create_session(server_uri, 0);
    if (session_nr < 0) {
        fprintf(stderr, "Error: %s. Could not establish session with server at %s",
                strerror(-session_nr), server_uri);
        return session_nr;
    }
    for (size_t i = 0; i < try_iterations && !client_rpc->is_connected(session_nr); i++)
        client_rpc->run_event_loop_once();
    if (!client_rpc->is_connected(session_nr)) {
        fprintf(stderr, "Could not reach the server within %lu tries", try_iterations);
        return session_nr + 1;
    }
    else
        return session_nr;
}

/**
 * @param address Server address to read from
 * @param address_size Size of address
 * @param length Number of bytes to read from server
 * @return 0 on success, -1 if something went wrong, TODO: Return codes?
 */
int Client::recv(char *address, size_t address_size, size_t length, unsigned int refresh_timeout=100) {
    if (!address)
        return -1;

    /* TODO: Perform some more checks */
    /*if (!req)
        return -1; */

    /* Generate new sequence number: */
    uint64_t seq;
    do {
        if (RAND_bytes((unsigned char*)&seq, sizeof(seq)) != 1)
            return -1;
    } while(!add_sequence_number(seq));

    seq &= ~ (uint64_t )0x3;
    seq |= (uint64_t )RDMA_RECV;

    req = client_rpc->alloc_msg_buffer_or_die(MIN_MSG_LEN + address_size);
    resp = client_rpc->alloc_msg_buffer_or_die(MIN_MSG_LEN + length);

    ((uint64_t *) req.buf)[0] = htobe64(seq);
    ((uint64_t *) req.buf)[1] = htobe64((uint64_t) length);
    (void) memcpy(req.buf + 16, address, address_size);
    if (!calculate_hash(req.buf, 16 + address_size, req.buf + 16 + address_size)) {
        fprintf(stderr, "Could not calculate hash of data\n");
        return -1;
    }

    /* TODO: Can we use the session number as the sequence number?
     *  Would require a little extra effort to calculate
     *  the hash, but it would maybe simplify the protocol
     *
     * Question: What parameters does cont_func take? It is called when we get a response, right?
     * */
    client_rpc->enqueue_request(session_nr, RDMA_RECV, &req, &resp, cont_func, nullptr);
    client_rpc->run_event_loop(refresh_timeout);
    return 0;
}

erpc::MsgBuffer *Client::get_resp() {
    return &resp;
}


int main() {
    /* erpc::Nexus nexus(client_uri, 0, 0);

    rpc = new erpc::Rpc<erpc::CTransport>(&nexus, nullptr, 0, sm_handler);

    std::string server_uri = kServerHostname + ":" + std::to_string(kUDPPort);
    int session_num = rpc->create_session(server_uri, 0);

    while (!rpc->is_connected(session_num)) rpc->run_event_loop_once();

    req = rpc->alloc_msg_buffer_or_die(kMsgSize);
    resp = rpc->alloc_msg_buffer_or_die(kMsgSize);

    rpc->enqueue_request(session_num, kReqType, &req, &resp, cont_func, nullptr);
    rpc->run_event_loop(100);

    delete rpc;
    */
}
