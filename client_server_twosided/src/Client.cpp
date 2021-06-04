#include <openssl/rand.h>

#include "Client.h"
#include "client_server_common.h"
erpc::Nexus *nexus = nullptr;


void empty_sm_handler(int, erpc::SmEventType, erpc::SmErrType, void *) {

}

void Client::init(std::string &client_hostname, uint16_t udp_port) {
    std::string client_uri = client_hostname + ":" + std::to_string(udp_port);
    nexus = new erpc::Nexus(client_uri, 0, 0);
}

void Client::terminate() {
    delete nexus;
    nexus = nullptr;
}

Client::Client(uint8_t id,
    size_t max_key_size, size_t max_val_size) :
    session_nr(-1),
    erpc_id(id),
    client_rpc{nexus, this, id, empty_sm_handler, 0},
    queue{id},
    max_key_size{max_key_size},
    max_val_size{max_val_size}
{
    this->queue.allocate_req_buffers(this->client_rpc,
        CIPHERTEXT_SIZE(max_key_size),CIPHERTEXT_SIZE(max_val_size));
}

Client::~Client() {
    this->queue.free_req_buffers(this->client_rpc);
    (void) disconnect();
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

    session_nr = client_rpc.create_session(server_uri, this->erpc_id);
    if (session_nr < 0) {
        std::cout << "Error: " << strerror(-session_nr) <<
             " Could not establish session with server at " << server_uri << endl;
        return session_nr;
    }
    for (size_t i = 0; !client_rpc.is_connected(session_nr); i++)
        client_rpc.run_event_loop_once();

    return session_nr;
}


/**
 * A simple message with type RDMA_ERR signalises the server to shut down its
 * thread and eRPC object for this client
 */
void Client::send_disconnect_message() {
    msg_tag_t *tag = queue.prepare_new_request(RDMA_ERR, nullptr, nullptr);

    struct rdma_enc_payload payload = { nullptr, nullptr, 0 };

    if (0 > encrypt_message(&(tag->header), &payload,
        static_cast<unsigned char **>(&(tag->request.buf))))
        goto err_send_disconnect_message;

    this->send_message(tag, 10000);
    return;

err_send_disconnect_message:
    tag->valid = false;
}

/**
 * Ends a session with a server
 * @return 0 on success, negative errno if the session can't be disconnected
 */
int Client::disconnect() {
    send_disconnect_message();
    int ret = client_rpc.destroy_session(session_nr);
    this->queue.invalidate_all_requests();
    return ret;
}

/**
 * Method that is always called for enqueuing a request
 * The sequence number is incremented, the request is enqueued and the event
 * loop is run for sending
 * @param tag Tag that will be passed by the callback
 * @param loop_iterations Number of event loop iterations to perform
 */
void Client::send_message(
    msg_tag_t *tag, size_t loop_iterations) {

    this->queue.inc_seq();

    client_rpc.enqueue_request(session_nr, DEFAULT_REQ_TYPE,
        &(tag->request), &(tag->response), decrypt_cont_func, (void *)tag);

    for (size_t i = 0; i < loop_iterations; i++)
        client_rpc.run_event_loop_once();
}


/**
 * @param key Server address to read from
 * @param key_len Size of address
 * @param value Value whose key we want to get
 * @param value_len Pointer to size_t at which the length of the value is stored
 * @param callback Callback that is called if a server response is received
 * @param user_tag Arbitrary tag a user can specify to re-identify his request
 * @param timeout Maximum time to wait for the server's response
 * @return 0 on success, -1 on error
 */
int Client::get(const void *key, size_t key_len,
    void *value, size_t *value_len,
    status_callback callback, const void *user_tag,
    size_t loop_iterations) {

    if (!key)
        return -1;

    assert(this->session_nr >= 0);
    assert(key_len <= this->max_key_size);

    msg_tag_t *tag = this->queue.prepare_new_request(
        RDMA_GET, user_tag, callback, value_len);

    int ret = -1;
    struct rdma_enc_payload enc_payload;

    tag->header.key_len = key_len;
    tag->value = value;
    enc_payload = { (unsigned char *) key, nullptr, 0 };

    if (0 != encrypt_message(&(tag->header),
        &enc_payload, (unsigned char **) &(tag->request.buf)))
        goto err_get;

    send_message(tag, loop_iterations);

    return 0;

    /* req and resp belong to eRPC on success.
     * On error, the buffers need to be freed manually */
err_get:
    tag->valid = false;
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
    assert(this->session_nr >= 0);
    assert(key_len <= this->max_key_size);
    assert(value_len <= this->max_val_size);

    msg_tag_t *tag = this->queue.prepare_new_request(
        RDMA_PUT, user_tag, callback);

    struct rdma_enc_payload enc_payload;

    tag->header.key_len = key_len;
    tag->value = nullptr;
    tag->user_tag = user_tag;
    enc_payload = { (unsigned char *) key, (unsigned char *) value, value_len };

    if (0 > encrypt_message(&(tag->header),
        &enc_payload, (unsigned char **) &(tag->request.buf)))
        goto err_put;

    send_message(tag, loop_iterations);

    return 0;

err_put:
    tag->valid = false;
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
    assert(session_nr >= 0);
    assert(key_len <= max_key_size);

    msg_tag_t *tag = this->queue.prepare_new_request(
        RDMA_DELETE, user_tag, callback);

    struct rdma_enc_payload payload;
    tag->header.key_len = key_len;
    tag->value = nullptr;
    payload ={ (unsigned char *) key, nullptr, 0 };

    if (0 > encrypt_message(
        &(tag->header), &payload, (unsigned char **)&(tag->request.buf)))
        goto err_delete;

    send_message(tag, loop_iterations);

    return 0;

err_delete:
    tag->valid = false;
    return -1;
}

void Client::run_event_loop_n_times(size_t n) {
    for (size_t i = 0; i < n; i++)
        this->client_rpc.run_event_loop_once();
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
    auto *tag = static_cast<msg_tag_t *>(message_tag);
    struct rdma_msg_header incoming_header;
    struct rdma_dec_payload payload = { nullptr,
                                        (unsigned char *) tag->value, 0 };
    int expected_op, incoming_op;
    size_t ciphertext_size = tag->response.get_data_size();
    unsigned char *ciphertext = tag->response.buf;


    if (0 > decrypt_message(&incoming_header,
        &payload, ciphertext, ciphertext_size)) {
        goto end_decrypt_cont_func; // invalid response
    }
    // This is most likely a message that has already been processed (timeout)
    // If it's not, it's a replay or similar, so we drop it
    if ((incoming_header.seq_op & (SEQ_MASK | ID_MASK)) !=
        (NEXT_SEQ(tag->header.seq_op) & (ID_MASK | SEQ_MASK))) {
        // cerr << "Message with old sequence number arrived" << endl;
        return;
    }

    expected_op = OP_FROM_SEQ_OP(tag->header.seq_op);
    incoming_op = OP_FROM_SEQ_OP(incoming_header.seq_op);
    if (expected_op != incoming_op) {
        ret = ret_val::OP_FAILED;
        goto end_decrypt_cont_func;
    }
    if (tag->value_len)
        *tag->value_len = payload.value_len;

    ret = ret_val::OP_SUCCESS;

end_decrypt_cont_func:
    client->queue.message_arrived(ret, tag->header.seq_op);
}


bool Client::queue_full() {
    return this->queue.queue_full();
}



