#include <openssl/rand.h>

#include "Client.h"
#include "client_server_common.h"

#ifdef DEBUG
#include "../../src/rpc.h"
#include "../../src/nexus.h"
#else
#include "rpc.h"
#include "nexus.h"
#endif // DEBUG

const size_t connection_tries = 64;

const unsigned char *enc_key;
thread_local int session_nr;
thread_local uint64_t current_seq_number = 0;

erpc::Nexus *nexus;
thread_local erpc::Rpc<erpc::CTransport> *client_rpc = nullptr;

struct sent_message_tag{
    anchor_client::status_callback *callback;
    struct rdma_msg_header header;
    void *value;
    const void *user_tag;
    erpc::MsgBuffer *request;
    erpc::MsgBuffer *response;
};


/* Frees a message tag internally and also the tag memory itself
 * The callback is the only parameter that is not freed
 */
void free_message_tag(struct sent_message_tag *tag) {
    if (!tag)
        return;

    delete tag->request;
    delete tag->response;
    free(tag);
}


struct sent_message_tag *new_message_tag(
        anchor_client::status_callback *callback, const void *user_tag) {

    struct sent_message_tag *ret =
            (struct sent_message_tag *) malloc(sizeof(struct sent_message_tag));
    if (!ret)
        return nullptr;
    ret->callback = callback;
    ret->user_tag = user_tag;

    ret->request = new erpc::MsgBuffer();
    ret->response = new erpc::MsgBuffer();

    if (!(ret->request && ret->response)) {
        goto err_new_tag;
    }

    return ret;

err_new_tag:
    free_message_tag(ret);
    return nullptr;
}



/* Note: cont_func is the continuation callback.
 * A message can be re-identified with the help of a tag
 * -> Possibility to provide asynchronous and synchronous put and get calls
 * */
/* void cont_func(void *context, void *tag) { */
void decrypt_cont_func(void *, void *message_tag) {
    int ret = -1;
    struct sent_message_tag *tag = (struct sent_message_tag *)message_tag;
    struct rdma_msg_header incoming_header;
    struct rdma_dec_payload payload = { nullptr, (unsigned char *) tag->value, 0 };
    int expected_op, incoming_op;
    if (!message_tag)
        goto end_decrypt_cont_func;


    if (0 > decrypt_message(enc_key, &incoming_header,
            &payload, tag->response->buf, tag->response->get_data_size()))
        goto end_decrypt_cont_func;

    expected_op = OP_FROM_SEQ_OP(tag->header.seq_op);
    incoming_op = OP_FROM_SEQ_OP(incoming_header.seq_op);
    if (incoming_header.seq_op != NEXT_SEQ(tag->header.seq_op)
            || expected_op != incoming_op)
        goto end_decrypt_cont_func;

    ret = 0;
end_decrypt_cont_func:
    tag->callback(ret, tag->user_tag);
    client_rpc->free_msg_buffer(*(tag->request));
    client_rpc->free_msg_buffer(*(tag->response));
    free_message_tag(tag);
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

int initialize_client(std::string client_hostname, uint16_t udp_port, uint8_t id) {
    std::string client_uri = client_hostname + ":" + std::to_string(udp_port);
    if (RAND_status() != 1) {
        if (RAND_poll() != 1) {
            cerr << "Couldn't initialize RNG" << endl;
            return -1;
        }
    }
    if (1 != RAND_bytes((unsigned char *) &current_seq_number, sizeof(current_seq_number))) {
        cerr << "Could not create initial random sequence number" << endl;
        return -1;
    }
    if (!nexus) {
        nexus = new erpc::Nexus(client_uri, 0, 0);
        if (!nexus)
            return -1;
    }
    current_seq_number = SET_ID(current_seq_number, id);
    
    return 0;
}


/**
 * Connects to a anchor server
 * @param server_hostname Hostname of the anchor server
 * @param udp_port Port on which the communication takes place
 * @param id Assumption: The client ID was agreed on in an initial handshake
 *          It is used in the sequence number
 * @return negative value if an error occurs. Otherwise the eRPC session number is returned
 */
int anchor_client::connect(std::string server_hostname,
        unsigned int udp_port, uint8_t id, const unsigned char *encryption_key) {
    if (0 > initialize_client(kClientHostname, kUDPPort, id)) {
        cerr << "Failed to initialize client!" << endl;
        return -1;
    }
    std::string server_uri = server_hostname + ":" + std::to_string(udp_port);
    enc_key = encryption_key;
    if (client_rpc) {
        cout << "Already connected. Disconnecting old connection" << endl;
        (void) anchor_client::disconnect();
    }
    client_rpc = new erpc::Rpc<erpc::CTransport>(
            nexus, nullptr, 0, empty_sm_handler, 0);

    session_nr = client_rpc->create_session(server_uri, 0);
    if (session_nr < 0) {
        cout << "Error: " << strerror(-session_nr) <<
            " Could not establish session with server at " << server_uri << endl;
        return session_nr;
    }
    for (size_t i = 0; i < connection_tries && !client_rpc->is_connected(session_nr); i++)
        client_rpc->run_event_loop_once();
    if (!client_rpc->is_connected(session_nr)) {
        cerr << "Could not reach the server" << endl;
        return -1;
    }
    else
        return session_nr;
}


/**
 * Ends a session with a server
 * @return 0 on success, negative errno if the session can't be disconnected
 */
int anchor_client::disconnect() {
    int ret = client_rpc->destroy_session(session_nr);
    delete client_rpc;
    client_rpc = nullptr;
    return ret;
}


int allocate_req_buffers(
        erpc::MsgBuffer *req, size_t req_size,
        erpc::MsgBuffer *resp, size_t resp_size) {
    try {
        *req = client_rpc->alloc_msg_buffer(req_size);
        *resp = client_rpc->alloc_msg_buffer(resp_size);
    } catch (const runtime_error &) {
        cerr << "Fatal error while allocating memory for request" << endl;
        goto err_allocate_req_buffers;
    }
    if (!(req->buf && resp->buf)) {
        cerr << "Error allocating buffers for request" << endl;
        goto err_allocate_req_buffers;
    }
    return 0;

err_allocate_req_buffers:
    client_rpc->free_msg_buffer(*req);
    client_rpc->free_msg_buffer(*resp);
    return -1;
}

void send_message(struct sent_message_tag *tag, size_t loop_iterations) {

    /* Skip the sequence number for the server response */
    current_seq_number = NEXT_SEQ(NEXT_SEQ(current_seq_number));

    client_rpc->enqueue_request(session_nr, DEFAULT_REQ_TYPE,
            tag->request, tag->response, decrypt_cont_func, (void *)tag);
    for (size_t i = 0; i < loop_iterations; i++)
        client_rpc->run_event_loop_once();
}

/**
 * @param key Server address to read from
 * @param key_len Size of address
 * @param value Value whose key we want to get
 * @param max_value_len Maximum length the returned value can have
 * @param callback Callback that is called if a server response is received
 * @param user_tag Arbitrary tag a user can specify to re-identify his request
 * @param timeout Maximum time to wait for the server's response
 * @return 0 on success, -1 on error
 */
int anchor_client::get(const void *key, size_t key_len,
        void *value, size_t max_value_len,
        anchor_client::status_callback *callback, const void *user_tag,
        size_t loop_iterations) {

    if (!key)
        return -1;

    if (!client_rpc || session_nr < 0) {
        cerr << "Client not initialized yet" << endl;
        return -1;
    }

    struct sent_message_tag *tag = new_message_tag(callback, user_tag);
    if (!tag)
        return -1;

    int ret = -1;
    uint64_t get_seq_number = SET_OP(current_seq_number, RDMA_GET);
    bool free_buffers = true;
    struct rdma_enc_payload enc_payload;

    if (0 != allocate_req_buffers(tag->request, CIPHERTEXT_SIZE(key_len),
            tag->response, CIPHERTEXT_SIZE(max_value_len))) {
        free_buffers = false;
        goto err_get;
    }

    tag->header = { get_seq_number, key_len };
    tag->value = value;
    enc_payload = { (unsigned char *) key, nullptr, 0 };

    if (0 != encrypt_message(enc_key, &(tag->header),
            &enc_payload, (unsigned char **) &(tag->request->buf)))
        goto err_get;


    send_message(tag, loop_iterations);

    return 0;

    /* req and resp belong to eRPC on success. On error, the buffers need to be freed manually */
err_get:
    if (free_buffers) {
        client_rpc->free_msg_buffer(*(tag->request));
        client_rpc->free_msg_buffer(*(tag->response));
    }
    free_message_tag(tag);
    return ret;
}

/**
 * Method that is called by clients to put a value for a certain key to the
 * server KV-store
 * @param key Key whose value is updated/inserted
 * @param key_len Length of key
 * @param value Value that is put to the KV-store
 * @param value_len Length of value
 * @param callback Callback that is called if the server responds to the request
 * @param user_tag Arbitrary tag a user can specify to re-identify his request
 * @param timeout Maximum time to wait for the server response
 * @return 0 on success, -1 on error
 */
int anchor_client::put(const void *key, size_t key_len,
        const void *value, size_t value_len,
        anchor_client::status_callback *callback, const void *user_tag,
        size_t loop_iterations) {

    if (!(key && value)) {
        return -1;
    }
    if (!client_rpc || session_nr < 0) {
        cerr << "Client not initialized yet" << endl;
        return -1;
    }

    struct sent_message_tag *tag = new_message_tag(callback, user_tag);
    if (!tag)
        return -1;

    bool free_buffers = true;
    uint64_t put_seq_number = SET_OP(current_seq_number, RDMA_PUT);
    struct rdma_enc_payload enc_payload;

    if (0 > allocate_req_buffers(tag->request, CIPHERTEXT_SIZE(key_len + value_len),
            tag->response, CIPHERTEXT_SIZE(0))) {
        free_buffers = false;
        goto err_put;
    }

    tag->header = { put_seq_number, key_len };
    tag->value = nullptr;
    tag->user_tag = user_tag;
    enc_payload = { (unsigned char *) key, (unsigned char *) value, value_len };

    if (0 > encrypt_message(enc_key, &(tag->header),
            &enc_payload, (unsigned char **) &(tag->request->buf)))
        goto err_put;

    send_message(tag, loop_iterations);

    return 0;

err_put:
    if (free_buffers) {
        client_rpc->free_msg_buffer(*(tag->request));
        client_rpc->free_msg_buffer(*(tag->response));
    }
    free_message_tag(tag);
    return -1;
}


/**
 * Method that can be called by clients to delete a key-value pair from the
 * server KV-store
 * @param key Key to be deleted
 * @param key_len Length of key
 * @param callback Callback that is called if we get a response from the server
 * @param user_tag Arbitrary tag a user can specify to re-identify his request
 * @param timeout Maximum time to wait for the server response
 * @return 0 on success, -1 on error
 */
int anchor_client::del(const void *key, size_t key_len,
        anchor_client::status_callback *callback, const void *user_tag,
        size_t loop_iterations) {

    if (!key) {
        return -1;
    }
    if (!client_rpc || session_nr < 0) {
        cerr << "Client not initialized yet" << endl;
        return -1;
    }

    struct sent_message_tag *tag = new_message_tag(callback, user_tag);
    if (!tag)
        return -1;

    bool free_buffers = true;
    uint64_t delete_seq_number = SET_OP(current_seq_number, RDMA_DELETE);
    struct rdma_enc_payload payload;

    if (0 > allocate_req_buffers(tag->request, CIPHERTEXT_SIZE(key_len),
            tag->response, CIPHERTEXT_SIZE(0))) {
        free_buffers = false;
        goto err_delete;
    }

    tag->header = { delete_seq_number, key_len };
    tag->value = nullptr;
    payload ={ (unsigned char *) key, nullptr, 0 };

    if (0 > encrypt_message(enc_key,
            &(tag->header), &payload, (unsigned char **)tag->request->buf))
        goto err_delete;

    send_message(tag, loop_iterations);

    return 0;

err_delete:
    if (free_buffers) {
        client_rpc->free_msg_buffer(*(tag->request));
        client_rpc->free_msg_buffer(*(tag->response));
    }
    free_message_tag(tag);
    return -1;
}

