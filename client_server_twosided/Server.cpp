#include <openssl/rand.h>

#include "client_server_common.h"

#ifdef DEBUG
#include "../src/rpc.h"
#include "../src/nexus.h"
#else
#include "rpc.h"
#include "nexus.h"
#endif // DEBUG

#include "Server.h"

erpc::Rpc<erpc::CTransport> *rpc_host = nullptr;
erpc::Nexus *nexus = nullptr;
unsigned char *encryption_key = nullptr;

void req_handler(erpc::ReqHandle *, void *);
void req_handler_get(erpc::ReqHandle *req_handle, unsigned char *request_data, size_t request_data_size);
void req_handler_put(erpc::ReqHandle *req_handle, unsigned char *request_data, size_t request_data_size);
void req_handler_delete(erpc::ReqHandle *req_handle, unsigned char *request_data, size_t request_data_size);

get_function kv_get;
put_function kv_put;
delete_function kv_delete;

/* Hosts an RPC server */
int host_server(std::string hostname, uint16_t udp_port, size_t timeout_millis,
        get_function get, put_function put, delete_function del) {


    std::string server_uri = hostname + ":" + std::to_string(udp_port);
    if (!nexus)
        nexus = new erpc::Nexus(server_uri, 0, 0);

    if (RAND_status() != 1) {
        if (RAND_poll() != 1) {
            cerr << "Could not initialize RNG" << endl;
            return -1;
        }
    }

    if (nexus->register_req_func(0, req_handler)) {
        cerr << "Failed to host Server" << endl;
        delete nexus;
        return -1;
    }

    kv_get = get;
    kv_put = put;
    kv_delete = del;

    rpc_host = new erpc::Rpc<erpc::CTransport>(nexus, nullptr, 0, nullptr);
    if (!rpc_host) {
        cerr << "Failed to host Server" << endl;
        delete nexus;
        return -1;
    }

    /* TODO: replace this with an endless while-loop and run_once()/
     *          introduce variable for closing connections
     */
    rpc_host->run_event_loop(timeout_millis);
    return 0;
}

void close_connection() {
    delete rpc_host;
}

void send_encrypted_response(erpc::ReqHandle *req_handle,
        struct rdma_msg_header *header, struct rdma_enc_payload *payload) {

    unsigned char *ciphertext;
    erpc::MsgBuffer resp_buffer;
    /* The server never sends back a key, so the value length is sufficient */
    size_t ciphertext_size = CIPHERTEXT_SIZE(payload->value_len);
    try {
        resp_buffer = rpc_host->alloc_msg_buffer(ciphertext_size);
    } catch (const runtime_error &) {
        cerr << "Error allocating memory for response" << endl;
        return;
    }
    ciphertext = (unsigned char *) resp_buffer.buf;
    if (!ciphertext) {
        cerr << "Memory Allocation failure" << endl;
        return;
    }

    if (0 != encrypt_message(encryption_key, header, payload, &ciphertext)) {
        rpc_host->free_msg_buffer(resp_buffer);
        return;
    }

    rpc_host->enqueue_response(req_handle, &resp_buffer);

}


void send_empty_response(erpc::ReqHandle *req_handle, struct rdma_msg_header *header) {
    header->key_len = 0;
    struct rdma_enc_payload payload = { nullptr, nullptr, 0 };
    send_encrypted_response(req_handle, header, &payload);
}

/**
* Request handler for incoming receive requests
* Then, transfers data at the specified address to the client
* */
void send_response_get(erpc::ReqHandle *req_handle,
        struct rdma_msg_header *header, const void *key) {
    unsigned char *resp;
    size_t resp_len;

    /* Call KV-store: */
    resp = (unsigned char *) kv_get(key, header->key_len, &resp_len);
    if (!resp) {
        header->seq_op = SET_OP(NEXT_SEQ(header->seq_op), RDMA_ERR);
        send_empty_response(req_handle, header);
        return;
    }

    /* Reuse the request header for creating and enqueueing the response: */
    header->seq_op = NEXT_SEQ(header->seq_op);
    header->key_len = 0;
    struct rdma_enc_payload payload = { nullptr, resp, resp_len };

    send_encrypted_response(req_handle, header, &payload);

    free(resp);
}


/**
* Request handler for incoming put requests
* Checks freshness and checksum
* Then, writes data from the client to the specified address
* */
void send_response_put(erpc::ReqHandle *req_handle, struct rdma_msg_header *header,
        struct rdma_dec_payload *payload) {

    /* Call KV-store: */
    int resp = kv_put(payload->key, header->key_len, payload->value, payload->value_len);
    if (0 > resp) {
        header->seq_op = SET_OP(NEXT_SEQ(header->seq_op), RDMA_ERR);
    }
    else {
        header->seq_op = NEXT_SEQ(header->seq_op);
    }
    /* We only inform the client about whether the operation was successful or not */
    send_empty_response(req_handle, header);
}


/**
 * Handles a delete request, passes it to the KV-store and sends a response message
 * to the client
 * @param req_handle Handle of the request
 * @param header Header of the incoming request that is reused for the response
 * @param payload Payload structure of the incoming request
 */
void send_response_delete(erpc::ReqHandle *req_handle, struct rdma_msg_header *header,
        struct rdma_dec_payload *payload) {

    /* Call KV-store: */
    int resp = kv_delete(payload->key, header->key_len);
    if (0 > resp) {
        header->seq_op = SET_OP(NEXT_SEQ(header->seq_op), RDMA_ERR);
    }
    else {
        header->seq_op = NEXT_SEQ(header->seq_op);
    }
    /* We only inform the client about whether the operation was successful or not */
    send_empty_response(req_handle, header);
}

/* void req_handler(erpc::ReqHandle *req_handle, void *context) */
void req_handler(erpc::ReqHandle *req_handle, void *) {
    struct rdma_msg_header header;
    uint8_t op;
    const erpc::MsgBuffer *ciphertext_buf = req_handle->get_req_msgbuf();
    struct rdma_dec_payload payload = { nullptr, nullptr, 0 };
    unsigned char *ciphertext = (unsigned char *)ciphertext_buf->buf;
    if (!ciphertext) {
        cerr << "Could not get request message buffer" << endl;
        return;
    }
    size_t ciphertext_size = ciphertext_buf->get_data_size();
    if (ciphertext_size < MIN_MSG_LEN) {
        cerr << "Message not long enough" << endl;
        return;
    }

    if (0 != decrypt_message(encryption_key, &header, &payload, ciphertext, ciphertext_size))
        goto end_req_handler;

    /* Add sequence number to known sequence numbers and check for replays: */
    if (!add_sequence_number(header.seq_op)) {
        cerr << "Invalid sequence number" << endl;
        goto end_req_handler;
    }

    op = (uint8_t) (header.seq_op & 0b11);
    switch (op) {
        case RDMA_GET: send_response_get(req_handle, &header, payload.key); break;
        case RDMA_PUT:
        case RDMA_DELETE:
            cerr << "TODO: Implement" << endl; break;
        default:
            cerr << "Invalid operation: " << op << endl;
    }

end_req_handler:
    free(payload.key);
    free(payload.value);
}

/*
void req_handler(erpc::ReqHandle *req_handle, void *) {
    auto &resp = req_handle->pre_resp_msgbuf;
    rpc_host->resize_msg_buffer(&resp, kMsgSize);
    sprintf(reinterpret_cast<char *>(resp.buf), "hello");

    rpc_host->enqueue_response(req_handle, &resp);
}
*/

