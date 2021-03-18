#include "Client.h"

erpc::Rpc<erpc::CTransport> *client_rpc = nullptr;

erpc::MsgBuffer req;
erpc::MsgBuffer resp;

/* TODO: Map from Sequence numbers to buffers for multiple requests */
/* TODO: Use data structure for multiple sessions + context */
int session_nr = -1;

struct incoming_value{
    void *value;
    void *(*callback)();
    erpc::MsgBuffer *response;
};

/* TODO: There has to be a better solution for passing the client to cont_func */
/* Note: cont_func is the continuation callback.
 * A message can be re-identified with the help of a tag
 * -> Possibility to provide asynchronous and synchronous put and get calls
 * */
/* void cont_func(void *context, void *tag) { */
void cont_func(void *, void *tag) {
    /* TODO: Get request + parameters by tag and context */
    struct incoming_value *val = (struct incoming_value *)tag;
    if (!val)
        return;
}

/* sm = Session management. Is invoked if a session is created or destroyed */
/* void sm_handler(int session, erpc::SmEventType event, erpc::SmErrType error, void *context) { */
void sm_handler(int, erpc::SmEventType event, erpc::SmErrType, void *) {
    /* TODO: Handle cases, consider errors */
    switch(event) {
        case erpc::SmEventType::kConnected:
            cout << "Connected!" << endl; break;
        case erpc::SmEventType::kConnectFailed:
            cerr << "Connection failed" << endl; break;
        case erpc::SmEventType::kDisconnected:
            cout << "Disconnected" << endl; break;
        case erpc::SmEventType::kDisconnectFailed:
            cerr << "Could not disconnect!" << endl; break;
        default:
            cerr << "Unknown event type" << endl; break;
    }
}

int initialize_client(std::string client_hostname, uint16_t udp_port) {
    std::string client_uri = client_hostname + ":" + std::to_string(udp_port);
    erpc::Nexus nexus(client_uri, 0, 0);
    if (RAND_status() != 1) {
        if (RAND_poll() != 1) {
            /* TODO: Handle this error (with an exception?) */
            cerr << "Couldn't initialize RNG" << endl;
            return -1;
        }
    }
    /* Context is passed to user callbacks by the event loop */
    void *context = nullptr;
    /* TODO: Increase port_index for multiple clients? */
    uint8_t port_index = 0;
    client_rpc = new erpc::Rpc<erpc::CTransport>(&nexus, context, port_index, sm_handler);
    if (!client_rpc) {
        return -1;
    }
    return 0;
}

/**
 * Destroys a session with a server
 * @return 0 on success, negative errno if the session can't be disconnected
 */
int disconnect() {
    /* TODO: What about freeing Nexus and the message buffers? */
    return client_rpc->destroy_session(session_nr);
}


int connect(std::string server_hostname, unsigned int udp_port, size_t try_iterations) {
    std::string server_uri = server_hostname + ":" + std::to_string(udp_port);
    /* TODO: Can we use the id? */
    session_nr = client_rpc->create_session(server_uri, 0);
    if (session_nr < 0) {
        std::cout << "Error: " << strerror(-session_nr) <<
            " Could not establish session with server at " << server_uri << std::endl;
        return session_nr;
    }
    for (size_t i = 0; i < try_iterations && !client_rpc->is_connected(session_nr); i++)
        client_rpc->run_event_loop_once();
    if (!client_rpc->is_connected(session_nr)) {
        cerr << "Could not reach the server within " << try_iterations << " tries";
        return session_nr + 1;
    }
    else
        return session_nr;
}

void free_client() {
    /* TODO: What about nexus? */
    delete client_rpc;
}

/**
 * @param key Server address to read from
 * @param key_len Size of address
 * @return 0 on success, -1 if something went wrong, TODO: Return codes?
 */
int get(char *key, size_t key_len, size_t expected_value_len, void *value, void *callback(),
        unsigned int timeout=100) {
    if (!key)
        return -1;

    int ret = -1;
    unsigned char *req_plaintext;
    size_t req_plaintext_len = SEQ_LEN + SIZE_LEN + key_len;
    size_t req_ciphertext_len = MIN_MSG_LEN + key_len;

    if (!client_rpc || session_nr < 0) {
        cerr << "Client not initialized yet" << endl;
    }

    /* Generate new sequence number: */
    uint64_t seq;
    do {
        if (1 != RAND_bytes((unsigned char*)&seq, sizeof(seq))) {
            cerr << "Failed to generate random sequence number" << endl;
            return -1;
        }
        seq &= ~ (uint64_t )0x3;
    } while(!add_sequence_number(seq));

    seq |= (uint64_t )RDMA_GET;

    req_plaintext = (unsigned char *)malloc(req_plaintext_len);
    req = client_rpc->alloc_msg_buffer(req_ciphertext_len);
    resp = client_rpc->alloc_msg_buffer(MIN_MSG_LEN + expected_value_len);
    struct incoming_value *tag = (struct incoming_value *)malloc(sizeof(struct incoming_value));

    if (!(req_plaintext && req.buf && resp.buf && tag)) {
        goto end_get;
    }

    ((uint64_t *) seq)[0] = htobe64(seq);
    ((uint64_t *) seq)[1] = htobe64((uint64_t) key_len);
    (void) memcpy(req_plaintext + 16, key, key_len);

    if (0 != encrypt(req_plaintext, req_plaintext_len, req.buf, IV_LEN,
                key_do_not_use, req.buf + req_ciphertext_len - MAC_LEN, MAC_LEN,
                NULL, 0, req.buf + IV_LEN)) {
        goto end_get;
    }

    /* Fill the tag that is passed to the callback function: */
    tag->callback = callback;
    tag->value = value;
    tag->response = &resp;

    /* TODO: Can we use the session number as the sequence number?
     *  Would require a little extra effort to calculate
     *  the hash, but it would maybe simplify the protocol
     *
     * */
    free(req_plaintext);
    client_rpc->enqueue_request(session_nr, 0, &req, &resp, cont_func, (void *)tag);
    client_rpc->run_event_loop(timeout);
    return 0;

    /* We only get here on failure, because req and resp belong to eRPC on success */
end_get:
    free(req_plaintext);
    client_rpc->free_msg_buffer(req);
    client_rpc->free_msg_buffer(resp);

    return ret;
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
