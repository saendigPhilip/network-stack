#include <openssl/rand.h>

#include "client_server_common.h"

#include "Server.h"
#include "ServerThread.h"

erpc::Nexus *nexus = nullptr;
std::vector<ServerThread *> *threads = nullptr;
size_t max_msg_size;

anchor_server::get_function kv_get;
anchor_server::put_function kv_put;
anchor_server::delete_function kv_delete;

void req_handler(erpc::ReqHandle *req_handle, void *context);


/**
 * Creates an eRPC Nexus object which is needed for initialization and opening
 * connections
 * @param hostname Hostname (e.g. IP-address) of the server
 * @param udp_port Port for communication for initialization for the connection
 */
int anchor_server::init(string &hostname, uint16_t udp_port) {
    std::string server_uri = hostname + ":" + std::to_string(udp_port);
    nexus = new erpc::Nexus(server_uri, 0, 0);
    if (nexus->register_req_func(DEFAULT_REQ_TYPE, req_handler)) {
        cerr << "Failed to initialize Server" << endl;
        terminate();
        return -1;
    }
    return 0;
}


/**
 * Deletes the nexus object, new connections can't be initialized after calling
 */
void anchor_server::terminate() {
    if (nexus) {
        delete nexus;
        nexus = nullptr;
    }
}

/**
 * Hosts a server that answers client put/get/delete requests
 *
 * @param encryption_key Network key to use for en-/decryption of the messages
 * @param number_threads Number of threads that are spawned at the beginning
 *          Limits the number of clients
 * @param max_entry_size Size of biggest key-value-pair in the KV-store
 * @param asynchronous If true, the method terminates when the last spawned
 *          thread has terminated. Other threads may still run after termination
 *          of this method.
 *          close_connection() still needs to be called afterwards
 * @param get Function of the KV-Store that is called on client get-requests
 * @param put Function of the KV-Store that is called on client put-requests
 * @param del Function of the KV-Store that is called on client delete-requests
 * @return 0 if Server was hosted successfully, -1 on error
 */
int anchor_server::host_server(
        const unsigned char *encryption_key,
        uint8_t number_threads,
        size_t max_entry_size, bool asynchronous,
        get_function get, put_function put, delete_function del) {

    max_msg_size = CIPHERTEXT_SIZE(max_entry_size);
    if (max_msg_size > erpc::Rpc<erpc::CTransport>::kMaxMsgSize) {
        cerr << "Maximum entry size is too big. Not supported (yet)" << endl;
        cerr << "Maximum supported entry size: ";
        cerr << PAYLOAD_SIZE(erpc::Rpc<erpc::CTransport>::kMaxMsgSize) << endl;
        return -1;
    }

    if (RAND_status() != 1) {
        if (RAND_poll() != 1) {
            cerr << "Could not initialize RNG" << endl;
            return -1;
        }
    }

    enc_key = encryption_key;
    threads = new std::vector<ServerThread *>();

    kv_get = get;
    kv_put = put;
    kv_delete = del;

    if (!asynchronous)
        number_threads--;
    for (uint8_t id = 0; id < number_threads; id++) {
        threads->push_back(new ServerThread(nexus, id, max_msg_size));
    }
    if (!asynchronous)
        ServerThread thread(nexus, number_threads, max_msg_size, false);

    return 0;
}


/**
 * Closes the connection that was before opened by a call to host_server
 * @param force If true, forces each server thread to disconnect from the client
 *          itself. Otherwise waits for the
 */
void anchor_server::close_connection(bool force) {
    for (auto thread : *threads) {
        if (force)
            thread->terminate();
        thread->join();
        delete thread;
    }
    delete threads;
}


/**
 * Internal function for sending an encrypted response to a client.
 * Is called whenever any response is sent
 *
 * @param req_handle Handle that came with the request
 * @param st ServerThread for the according client
 * @param header struct for the header of the sent message
 * @param payload Payload struct
 */
void send_encrypted_response(erpc::ReqHandle *req_handle, ServerThread *st,
        struct rdma_msg_header *header, struct rdma_enc_payload *payload) {

    unsigned char *ciphertext;
    erpc::MsgBuffer *resp_buffer;
    /* The server never sends back a key, so the value length is sufficient */
    size_t ciphertext_size = CIPHERTEXT_SIZE(payload->value_len);
    if (unlikely(ciphertext_size > max_msg_size)) {
        cerr << "Answer too long for pre-allocated message buffer" << endl;
        return;
    }
    resp_buffer = &(req_handle->pre_resp_msgbuf);
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(resp_buffer, ciphertext_size);
    ciphertext = (unsigned char *) resp_buffer->buf;

    if (unlikely(0 != encrypt_message(header, payload, &ciphertext))) {
        cerr << "Failed to encrypt message" << endl;
        return;
    }

    st->enqueue_response(req_handle, resp_buffer);
}


/**
 * Sends a response with no payload (e.g. error message, put/delete response)
 * @param st ServerThread for the according client
 */
void send_empty_response(erpc::ReqHandle *req_handle, ServerThread *st,
        struct rdma_msg_header *header) {
    header->key_len = 0;
    struct rdma_enc_payload payload = { nullptr, nullptr, 0 };
    send_encrypted_response(req_handle, st, header, &payload);
}

/**
* Request handler for incoming receive requests
* Then, transfers data at the specified address to the client
* */
void send_response_get(erpc::ReqHandle *req_handle, ServerThread *st,
        struct rdma_msg_header *header, const void *key) {

    size_t resp_len;
    /* Call KV-store: */
    auto resp = static_cast<const unsigned char *>(
            kv_get(key, header->key_len, &resp_len));

    if (!resp) {
        header->seq_op = st->get_next_seq(header->seq_op, RDMA_ERR);
        send_empty_response(req_handle, st, header);
        return;
    }

    /* Reuse the request header for creating and enqueueing the response: */
    header->seq_op = st->get_next_seq(header->seq_op, RDMA_GET);
    header->key_len = 0;
    struct rdma_enc_payload payload = { nullptr, resp, resp_len };

    send_encrypted_response(req_handle, st, header, &payload);

}


/**
* Request handler for incoming put requests
* Checks freshness and checksum
* Then, writes data from the client to the specified address
* */
void send_response_put(erpc::ReqHandle *req_handle, ServerThread *st,
        struct rdma_msg_header *header, struct rdma_dec_payload *payload) {

    /* Call KV-store: */
    int resp = kv_put(payload->key, header->key_len, payload->value, payload->value_len);
    if (0 > resp) {
        header->seq_op = st->get_next_seq(header->seq_op, RDMA_ERR);
    }
    else {
        header->seq_op = st->get_next_seq(header->seq_op, RDMA_PUT);
    }
    /* We only inform the client about whether the operation was successful or not */
    send_empty_response(req_handle, st, header);
}


/**
 * Handles a delete request, passes it to the KV-store and sends a response message
 * to the client
 * @param req_handle Handle of the request
 * @param header Header of the incoming request that is reused for the response
 * @param payload Payload structure of the incoming request
 */
void send_response_delete(erpc::ReqHandle *req_handle, ServerThread *st,
        struct rdma_msg_header *header,
        const void *key) {

    /* Call KV-store: */
    int resp = kv_delete(key, header->key_len);
    if (0 > resp) {
        header->seq_op = st->get_next_seq(header->seq_op, RDMA_ERR);
    }
    else {
        header->seq_op = st->get_next_seq(header->seq_op, RDMA_DELETE);
    }
    /* We only inform the client about whether the operation was successful or not */
    send_empty_response(req_handle, st, header);
}


/**
 * Sends a response to a disconnect request
 */
void send_response_disconnect(erpc::ReqHandle *req_handle, ServerThread *st,
        struct rdma_msg_header *header) {

    /* Call KV-store: */
    header->seq_op = st->get_next_seq(header->seq_op, RDMA_ERR);
    /* We only inform the client about whether the operation was successful or not */
    send_empty_response(req_handle, st, header);
}


/**
 * The request handler that is invoked on every incoming request
 * @param req_handle Request Handle needed for Message Buffers and response
 * @param context Here: Pointer to according ServerThread that should handle the
 *          message
 */
void req_handler(erpc::ReqHandle *req_handle, void *context) {
    struct rdma_msg_header header;
    uint8_t op;
    auto st = static_cast<ServerThread *>(context);
    const erpc::MsgBuffer *ciphertext_buf = req_handle->get_req_msgbuf();
    struct rdma_dec_payload payload = { nullptr, nullptr, 0 };
    auto *ciphertext = static_cast<unsigned char *>(ciphertext_buf->buf);
    if (!ciphertext) {
        cerr << "Could not get request message buffer" << endl;
        return;
    }
    size_t ciphertext_size = ciphertext_buf->get_data_size();

    if (0 != decrypt_message(&header, &payload, ciphertext, ciphertext_size)) {
        cerr << "Failed to decrypt message" << endl;
        goto end_req_handler;
    }

    /* Always disconnect, if the client requests it: */
    op = OP_FROM_SEQ_OP(header.seq_op);
    if (unlikely(op == RDMA_ERR)) {
        send_response_disconnect(req_handle, st, &header);
        st->terminate();
        goto end_req_handler;
    }

    /* Check for replays by checking the sequence number: */
    if (unlikely(!st->is_seq_valid(header.seq_op))) {
        goto end_req_handler;
    }

    switch (op) {
        case RDMA_GET:
            send_response_get(req_handle, st, &header, payload.key);
            break;
        case RDMA_PUT:
            send_response_put(req_handle, st, &header, &payload);
            break;
        case RDMA_DELETE:
            send_response_delete(req_handle, st, &header, payload.key);
            break;
        default:
            cerr << "Invalid operation: " << op << endl;
    }

end_req_handler:
    free(payload.key);
    free(payload.value);
}

