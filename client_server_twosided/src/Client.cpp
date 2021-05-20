#include <openssl/rand.h>

#include "Client.h"
#include "client_server_common.h"
using namespace anchor_client;
erpc::Nexus *nexus = nullptr;
size_t nexus_ref = 0;

/* Because the last bit is always the same at client side, we shift the sequence
 * number to the right in order to use the whole accepted array */
#define ACCEPTED_INDEX(seq_op) \
        ((SEQ_FROM_SEQ_OP(seq_op) >> 1) % MAX_ACCEPTED_RESPONSES)

#define DEC_INDEX(index) index = \
        (((index) + MAX_ACCEPTED_RESPONSES - 1) % MAX_ACCEPTED_RESPONSES)

/* Some helper functions for dealing with sent_message_tag structs: */

static void init_message_tag(struct sent_message_tag *tag) {
    tag->request = new erpc::MsgBuffer();
    tag->response = new erpc::MsgBuffer();
    tag->valid = false;
}

/* Counterpart to init_message_tag: */
static void free_message_tag(struct sent_message_tag *tag) {
    delete tag->request;
    delete tag->response;
}

static void fill_message_tag(struct sent_message_tag *tag,
        const void *user_tag, status_callback cb,
        size_t *value_size = nullptr) {

    tag->value_len = value_size;
    tag->user_tag = user_tag;
    tag->callback = cb;
    tag->valid = true;
}

void Client::invalidate_message_tag(
        struct sent_message_tag *tag) {

    this->client_rpc->free_msg_buffer(*tag->request);
    this->client_rpc->free_msg_buffer(*tag->response);

    tag->valid = false;
}



void empty_sm_handler(int, erpc::SmEventType, erpc::SmErrType, void *) {

}

Client::Client(
        std::string& client_hostname, uint16_t udp_port, uint8_t id) {

    std::string client_uri = client_hostname + ":" + std::to_string(udp_port);
    if (RAND_status() != 1) {
        if (RAND_poll() != 1) {
            throw std::runtime_error("Couldn't initialize RNG");
        }
    }
    if (1 != RAND_bytes((unsigned char *) &current_seq_op,
            sizeof(current_seq_op))) {
        throw std::runtime_error("Couldn't generate initial sequence number");
    }
    if (!nexus)
        nexus = new erpc::Nexus(client_uri, 0, 0);
    nexus_ref++;
    current_seq_op = SET_ID(current_seq_op, id);
    this->erpc_id = id;
    for (size_t i = 0; i < MAX_ACCEPTED_RESPONSES; i++)
        init_message_tag(accepted + i);

    client_rpc = nullptr;
}

Client::~Client() {
    (void) disconnect();
    for (size_t i = 0; i < MAX_ACCEPTED_RESPONSES; i++)
        free_message_tag(accepted + i);
    if (nexus_ref == 0 && nexus) {
        delete nexus;
        nexus = nullptr;
    }
}
/**
 * Continuation function that is called when a server response arrives
 * @param client
 * @param message_tag Tag that was associated with the corresponding request
 *          Is a pointer to a struct sent_message tag
 */
void Client::decrypt_cont_func(void *context, void *message_tag) {
    if (!(context && message_tag))
        return;
    auto *client = static_cast<Client *>(context);

    enum ret_val ret = ret_val::INVALID_RESPONSE;
    auto *tag = static_cast<struct sent_message_tag *>(message_tag);
    struct rdma_msg_header incoming_header;
    struct rdma_dec_payload payload = { nullptr,
            (unsigned char *) tag->value, 0 };
    int expected_op, incoming_op;
    size_t ciphertext_size = tag->response->get_data_size();
    unsigned char *ciphertext = tag->response->buf;


    if (0 > decrypt_message(&incoming_header,
            &payload, ciphertext, ciphertext_size)) {
        goto end_decrypt_cont_func; // invalid response
    }

    expected_op = OP_FROM_SEQ_OP(tag->header.seq_op);
    incoming_op = OP_FROM_SEQ_OP(incoming_header.seq_op);
    // This is most likely a message that has already been processed (timeout)
    // If it's not, it's a replay or similar, so we don't care about it
    if ((incoming_header.seq_op & SEQ_MASK) !=
            (NEXT_SEQ(tag->header.seq_op) & SEQ_MASK)) {
        // cerr << "Message with old sequence number arrived" << endl;
        return;
    }

    if (expected_op != incoming_op) {
        ret = ret_val::OP_FAILED;
        goto end_decrypt_cont_func;
    }
    if (tag->value_len)
        *tag->value_len = payload.value_len;

    ret = ret_val::OP_SUCCESS;

end_decrypt_cont_func:
    client->message_arrived(ret, tag->header.seq_op);
}


void Client::message_arrived(
        enum ret_val ret, uint64_t seq_op) {

    size_t index = ACCEPTED_INDEX(seq_op);
    /* If this is an expired answer to a request or a replay, we're done */
    if (!(this->accepted[index].valid)) {
        cerr << "Expired message arrived" << endl;
        return;
    }

    /* Call the Client callback and invalidate */
    if (this->accepted[index].callback)
        this->accepted[index].callback(ret, this->accepted[index].user_tag);
    invalidate_message_tag(accepted + index);

    /* To make sure we're making progress, we invalidate all older requests: */
    invalidate_old_requests(seq_op);
}

/* Iterates over the accepted struct until a request is found that Deletes all requests that have a lower sequence number than seq and invokes
 * the according client callbacks. This way, we can always guarantee that for
 * each message the client callback is invoked
 * seq_op is the latest sequence number that should be accepted
 */
void Client::invalidate_old_requests(uint64_t seq_op) {
    seq_op = PREV_SEQ(PREV_SEQ(seq_op));
    uint64_t current_seq;
    uint64_t orig_seq = SEQ_FROM_SEQ_OP(seq_op);
    int64_t diff;
    size_t index = ACCEPTED_INDEX(seq_op);
    for (size_t i = 0; i < MAX_ACCEPTED_RESPONSES; DEC_INDEX(index), i++) {
        /*
         * Invariant:
         * All tags "before" invalid tags are either invalid or fresh
         */
        if (!(accepted[index].valid))
            break;
        current_seq = SEQ_FROM_SEQ_OP(accepted[index].header.seq_op);
        diff = (int64_t) (orig_seq - current_seq);
        if (diff < 0)
            break;
        if (accepted[index].callback)
            accepted[index].callback(
                    ret_val::TIMEOUT, accepted[index].user_tag);
        invalidate_message_tag(accepted + index);
    }
}


/**
 * Connects to a anchor server
 * @param server_hostname Hostname of the anchor server
 * @param udp_port Port on which the communication takes place
 * @param id Assumption: The client ID was agreed on in an initial handshake
 *          It is used in the sequence number
 * @return negative value if an error occurs. Otherwise the eRPC session number is returned
 */
int Client::connect(std::string& server_hostname,
        unsigned int udp_port, const unsigned char *encryption_key) {

    std::string server_uri = server_hostname + ":" + std::to_string(udp_port);
    enc_key = encryption_key;
    if (client_rpc) {
        cout << "Already connected. Disconnecting old connection" << endl;
        (void) disconnect();
    }
    client_rpc = new erpc::Rpc<erpc::CTransport>(
            nexus, this, this->erpc_id, empty_sm_handler, 0);

    session_nr = client_rpc->create_session(server_uri, this->erpc_id);
    if (session_nr < 0) {
        cout << "Error: " << strerror(-session_nr) <<
            " Could not establish session with server at " << server_uri << endl;
        return session_nr;
    }
    for (size_t i = 0; !client_rpc->is_connected(session_nr); i++)
        client_rpc->run_event_loop_once();
    
    return session_nr;
}


/**
 * Ends a session with a server
 * @return 0 on success, negative errno if the session can't be disconnected
 */
int Client::disconnect() {
    if (!client_rpc)
        return 0;
    send_disconnect_message();
    int ret = client_rpc->destroy_session(session_nr);
    invalidate_old_requests(this->current_seq_op);
    delete this->client_rpc;
    this->client_rpc = nullptr;
    nexus_ref--;
    return ret;
}


int Client::allocate_req_buffers(
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


void Client::send_message(
        struct sent_message_tag *tag, size_t loop_iterations) {

    /* Skip one sequence number for the server response */
    this->current_seq_op = NEXT_SEQ(NEXT_SEQ(current_seq_op));


    client_rpc->enqueue_request(session_nr, DEFAULT_REQ_TYPE,
            tag->request, tag->response, decrypt_cont_func, (void *)tag);

    for (size_t i = 0; i < loop_iterations; i++)
        client_rpc->run_event_loop_once();
}

bool Client::queue_full() {
    /* If the request at the current sequence number index is valid,
     * we know that the queue is full
     */
    return accepted[ACCEPTED_INDEX(this->current_seq_op)].valid;
}

struct sent_message_tag *Client::prepare_new_request() {
    size_t index = ACCEPTED_INDEX(this->current_seq_op);
    struct sent_message_tag *ret = accepted + index;
    if (ret->valid) {
        ret->callback(TIMEOUT, ret->user_tag);
        invalidate_message_tag(ret);
        invalidate_old_requests(accepted[index].header.seq_op);
    }
    return ret;
}

void Client::send_disconnect_message() {
    struct sent_message_tag *tag = prepare_new_request();
    fill_message_tag(tag, nullptr, nullptr);
    tag->header = { SET_OP(current_seq_op, RDMA_ERR), 0 };
    struct rdma_enc_payload payload = { nullptr, nullptr, 0 };

    if (0 != allocate_req_buffers(
            tag->request, MIN_MSG_LEN, tag->response, MIN_MSG_LEN)) {
        goto err_send_disconnect_message;
    }

    if (0 > encrypt_message(&(tag->header), &payload,
            static_cast<unsigned char **>(&(tag->request->buf))))
        goto err_send_disconnect_message;

    this->send_message(tag, 10000);
    return;

err_send_disconnect_message:
    invalidate_message_tag(tag);
}

/**
 * @param key Server address to read from
 * @param key_len Size of address
 * @param value Value whose key we want to get
 * @param max_value_len Maximum length the returned value can have
 * @param value_len Pointer to size_t at which the length of the value is stored
 * @param callback Callback that is called if a server response is received
 * @param user_tag Arbitrary tag a user can specify to re-identify his request
 * @param timeout Maximum time to wait for the server's response
 * @return 0 on success, -1 on error
 */
int Client::get(const void *key, size_t key_len,
        void *value, size_t max_value_len, size_t *value_len,
        status_callback callback, const void *user_tag,
        size_t loop_iterations) {

    if (!key)
        return -1;

    if (!client_rpc || session_nr < 0) {
        cerr << "Client not initialized yet" << endl;
        return -1;
    }

    struct sent_message_tag *tag = prepare_new_request();
    fill_message_tag(tag, user_tag, callback, value_len);

    int ret = -1;
    uint64_t get_seq_number = SET_OP(current_seq_op, RDMA_GET);
    struct rdma_enc_payload enc_payload;

    if (0 != allocate_req_buffers(tag->request, CIPHERTEXT_SIZE(key_len),
            tag->response, CIPHERTEXT_SIZE(max_value_len))) {
        goto err_get;
    }

    tag->header = { get_seq_number, key_len };
    tag->value = value;
    enc_payload = { (unsigned char *) key, nullptr, 0 };

    if (0 != encrypt_message(&(tag->header),
            &enc_payload, (unsigned char **) &(tag->request->buf)))
        goto err_get;


    send_message(tag, loop_iterations);

    return 0;

    /* req and resp belong to eRPC on success.
     * On error, the buffers need to be freed manually */
err_get:
    invalidate_message_tag(tag);
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
int Client::put(const void *key, size_t key_len,
        const void *value, size_t value_len, status_callback callback,
        const void *user_tag, size_t loop_iterations) {

    if (!(key && value)) {
        return -1;
    }
    if (!client_rpc || session_nr < 0) {
        cerr << "Client not initialized yet" << endl;
        return -1;
    }

    struct sent_message_tag *tag = prepare_new_request();
    fill_message_tag(tag, user_tag, callback);

    uint64_t put_seq_number = SET_OP(current_seq_op, RDMA_PUT);
    struct rdma_enc_payload enc_payload;

    if (0 > allocate_req_buffers(tag->request, CIPHERTEXT_SIZE(key_len + value_len),
            tag->response, CIPHERTEXT_SIZE(0))) {
        goto err_put;
    }

    tag->header = { put_seq_number, key_len };
    tag->value = nullptr;
    tag->user_tag = user_tag;
    enc_payload = { (unsigned char *) key, (unsigned char *) value, value_len };

    if (0 > encrypt_message(&(tag->header),
            &enc_payload, (unsigned char **) &(tag->request->buf)))
        goto err_put;

    send_message(tag, loop_iterations);

    return 0;

err_put:
    invalidate_message_tag(tag);
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
int Client::del(const void *key, size_t key_len,
        status_callback callback, const void *user_tag,
        size_t loop_iterations) {

    if (!key) {
        return -1;
    }
    if (!client_rpc || session_nr < 0) {
        cerr << "Client not initialized yet" << endl;
        return -1;
    }

    struct sent_message_tag *tag = prepare_new_request();
    fill_message_tag(tag, user_tag, callback);

    uint64_t delete_seq_number = SET_OP(current_seq_op, RDMA_DELETE);
    struct rdma_enc_payload payload;

    if (0 > allocate_req_buffers(tag->request, CIPHERTEXT_SIZE(key_len),
            tag->response, CIPHERTEXT_SIZE(0))) {
        goto err_delete;
    }

    tag->header = { delete_seq_number, key_len };
    tag->value = nullptr;
    payload ={ (unsigned char *) key, nullptr, 0 };

    if (0 > encrypt_message(
            &(tag->header), &payload, (unsigned char **)&(tag->request->buf)))
        goto err_delete;

    send_message(tag, loop_iterations);

    return 0;

err_delete:
    invalidate_message_tag(tag);
    return -1;
}

void Client::run_event_loop_n_times(size_t n) {
    if (this->client_rpc) {
        for (size_t i = 0; i < n; i++)
            this->client_rpc->run_event_loop_once();
    }
}


