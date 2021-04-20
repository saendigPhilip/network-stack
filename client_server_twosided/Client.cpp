#include <openssl/rand.h>

#include "Client.h"
#include "client_server_common.h"

#ifdef DEBUG
#include "../src/rpc.h"
#include "../src/nexus.h"
#else
#include "rpc.h"
#include "nexus.h"
#endif // DEBUG

erpc::Nexus *nexus;
erpc::Rpc<erpc::CTransport> *client_rpc = nullptr;

erpc::MsgBuffer req;
erpc::MsgBuffer resp;

uint64_t current_seq_number = 0;

unsigned char *encryption_key;

/* TODO: Map from Sequence numbers to buffers for multiple requests */
/* TODO: Use data structure for multiple sessions + context */
int session_nr = -1;

struct incoming_value{
    size_t expected_seq;
    void *value;
    void (*callback)(void *, void *);
    erpc::MsgBuffer *request;
    erpc::MsgBuffer *response;
};

/* Note: cont_func is the continuation callback.
 * A message can be re-identified with the help of a tag
 * -> Possibility to provide asynchronous and synchronous put and get calls
 * */
/* void cont_func(void *context, void *tag) { */
void verbose_cont_func(void *, void *tag) {
    /* TODO: Get request + parameters by tag and context */
    struct incoming_value *val = (struct incoming_value *)tag;
    if (!val)
        return;
    puts((char *)val->value);
}

/* sm = Session management. Is invoked if a session is created or destroyed */
/* void sm_handler(int session, erpc::SmEventType event, erpc::SmErrType error, void *context) { */
void verbose_sm_handler(int, erpc::SmEventType event, erpc::SmErrType error, void *) {
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
    switch(error) {
        case erpc::SmErrType::kNoError:
            break;
        default: cerr << "Error while initializing session" << endl;
    }
}

void empty_sm_handler(int, erpc::SmEventType, erpc::SmErrType, void *) {

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
    if (1 != RAND_bytes((unsigned char *) &current_seq_number, sizeof(current_seq_number))) {
        cerr << "Could not create initial random sequence number" << endl;
        return -1;
    }
    /* The last two bits are for indicating the operation */
    current_seq_number &= ~(uint64_t )0b11;
    return 0;
}

/**
 * Ends a session with a server
 * @return 0 on success, negative errno if the session can't be disconnected
 */
int disconnect() {
    int ret = client_rpc->destroy_session(session_nr);
    delete client_rpc;
    client_rpc = nullptr;
    return ret;
}

void cont_func(void *, void *) {

}


/**
 * Connects to a server at server_hostname:udp_port
 * Tries to connect with given number of iterations
 * @return negative value if an error occurs. Otherwise the eRPC session number is returned
 */
int connect(std::string server_hostname, unsigned int udp_port, size_t try_iterations) {
    if (initialize_client(kClientHostname, kUDPPort)) {
        cerr << "Failed to initialize client!" << endl;
        return -1;
    }
    std::string server_uri = server_hostname + ":" + std::to_string(udp_port);
    if (client_rpc) {
        cout << "Already connected. Disconnecting old connection" << endl;
        (void) disconnect();
    }
    client_rpc = new erpc::Rpc<erpc::CTransport>(
            nexus, nullptr, 0, empty_sm_handler, 0);

    session_nr = client_rpc->create_session(server_uri, 0);
    if (session_nr < 0) {
        cout << "Error: " << strerror(-session_nr) <<
            " Could not establish session with server at " << server_uri << endl;
        return session_nr;
    }
    for (size_t i = 0; i < try_iterations && !client_rpc->is_connected(session_nr); i++)
        client_rpc->run_event_loop_once();
    if (!client_rpc->is_connected(session_nr)) {
        cerr << "Could not reach the server within " << try_iterations << " tries" << endl;
        return -1;
    }
    else
        return session_nr;
}


/**
 * @param key Server address to read from
 * @param key_len Size of address
 * @return 0 on success, -1 if something went wrong, TODO: Return codes?
 */
int get_from_server(const char *key, size_t key_len, size_t expected_value_len,
        void *value, void (*callback)(void *, void *), unsigned int timeout=100) {
    if (!key)
        return -1;

    if (!client_rpc || session_nr < 0) {
        cerr << "Client not initialized yet" << endl;
        return -1;
    }

    int ret = -1;
    size_t req_ciphertext_len = CIPHERTEXT_SIZE(expected_value_len);
    struct rdma_msg_header header;
    erpc::MsgBuffer req;
    erpc::MsgBuffer resp;
    header.seq_op = current_seq_number | RDMA_GET;
    header.key_len = key_len;

    struct incoming_value *tag = (struct incoming_value *)malloc(sizeof(struct incoming_value));
    try {
         req = client_rpc->alloc_msg_buffer(req_ciphertext_len);
         resp = client_rpc->alloc_msg_buffer(CIPHERTEXT_SIZE(expected_value_len));
    } catch (const runtime_error &) {
        cerr << "Data size too big for allocating memory" << endl;
        goto err_get;
    }
    if (!(req.buf && resp.buf && tag)) {
        goto err_get;
    }

    if (0 != encrypt_message(encryption_key, &header, &req.buf, key, key_len)) {
        goto err_get;
    }

    /* Tag struct is passed to the callback function when the server responds */
    tag->expected_seq = NEXT_SEQ(current_seq_number);
    current_seq_number = NEXT_SEQ(tag->expected_seq);
    tag->value = value;
    tag->callback = callback;
    tag->request = &req;
    tag->response = &resp;

    client_rpc->enqueue_request(session_nr,
            DEFAULT_REQ_TYPE, &req, &resp, cont_func, (void *)tag);
    client_rpc->run_event_loop(timeout);
    return 0;

    /* req and resp belong to eRPC on success. On error, the buffers need to be freed manually */
err_get:
    client_rpc->free_msg_buffer(req);
    client_rpc->free_msg_buffer(resp);
    free(tag);

    return ret;
}

