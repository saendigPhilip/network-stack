#include <openssl/rand.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/unistd.h>

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

void req_handler(erpc::ReqHandle *, void *);
void req_handler_get(erpc::ReqHandle *req_handle, unsigned char *request_data, size_t request_data_size);
void req_handler_put(erpc::ReqHandle *req_handle, unsigned char *request_data, size_t request_data_size);

unsigned char *kv_get(const char *key, size_t *data_len);
// int kv_put(const unsigned char *key, size_t key_len, unsigned char *value, size_t value_len);
// int kv_delete(const unsigned char *key, size_t key_len);

/* Hosts an RPC server
 */
int host_server(std::string hostname, uint16_t udp_port, size_t timeout_millis) {
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

    rpc_host = new erpc::Rpc<erpc::CTransport>(nexus, nullptr, 0, nullptr);
    if (!rpc_host) {
        cerr << "Failed to host Server" << endl;
        delete nexus;
        return -1;
    }

    /* TODO: (probably) replace this with an endless while-loop/ introduce variable for closing connections */
    rpc_host->run_event_loop(timeout_millis);
    return 0;
}

void close_connection() {
    delete rpc_host;
}

/**
* Request handler for incoming put requests
* Checks freshness and checksum
* Then, writes data from the client to the specified address
* TODO: implement
* */
/* TODO: Do we even respond? */
/*
void req_handler_put(erpc::ReqHandle *req_handle, struct rdma_req *req) {
}
*/

/**
* Request handler for incoming receive requests
* Checks freshness and checksum
* Then, transfers data at the specified address to the client
* Not finished yet: No encryption, no authenticity control
* */
void send_response_get(erpc::ReqHandle *req_handle, struct rdma_req *req) {
    unsigned char *resp;
    size_t resp_len;
    erpc::MsgBuffer resp_buffer;

    /* Get the requested data, get also allocates the space for seq_op and length: */
    resp = kv_get((char *)req->payload, &resp_len);
    if (!resp) {
        goto end_req_handler_get;
    }

    /* Create and enqueue the response: */
    ((uint64_t *) resp)[0] = htobe64(req->seq + 4);
    ((uint64_t *) resp)[1] = htobe64(resp_len);

    /* TODO: This gives us flexibility for the message buffer size, but may waste resources */
    resp_buffer = rpc_host->alloc_msg_buffer(resp_len + MIN_MSG_LEN);
    if (!resp_buffer.buf) {
        cerr << "Could not allocate enough memory" << endl;
        goto end_req_handler_get;
    }

    /* TODO: Make encryption and decryption a bit prettier and less modifiable */
    if (0 != encrypt(resp, resp_len, (unsigned char *)resp_buffer.buf, IV_LEN,
                     key_do_not_use, resp_buffer.buf + resp_len + MIN_MSG_LEN - MAC_LEN,
                     MAC_LEN, NULL, 0, resp_buffer.buf + IV_LEN)) {

        rpc_host->free_msg_buffer(resp_buffer);
        goto end_req_handler_get;
    }

    rpc_host->enqueue_response(req_handle, &resp_buffer);

end_req_handler_get:
    free(resp);
}

/* void req_handler(erpc::ReqHandle *req_handle, void *context) */
void req_handler(erpc::ReqHandle *req_handle, void *) {
    unsigned char *plaintext = nullptr;
    struct rdma_req req;
    uint64_t seq_op = 0;
    const erpc::MsgBuffer *req_msg_buf = req_handle->get_req_msgbuf();
    if (!req_msg_buf->buf) {
        cerr << "Failed to handle request" << endl;
        return;
    }
    unsigned char *req_msg = (unsigned char *)req_msg_buf->buf;
    size_t req_msg_len = req_msg_buf->get_data_size();
    if (req_msg_len < MIN_MSG_LEN) {
        cerr << "Message not long enough" << endl;
        return;
    }

    /* The plaintext size should be the incoming size minus IV and MAC tag length */
    plaintext = (unsigned char *)malloc(req_msg_len - MAC_LEN - IV_LEN);
    if (!plaintext) {
        cerr << "Memory allocation failure" << endl;
        return;
    }
    if (0 != decrypt(req_msg, req_msg_len, iv_do_not_use, sizeof(iv_do_not_use),
                 key_do_not_use, req_msg + req_msg_len - 12, 12, NULL, 0, plaintext)) {
        goto end_req_handler;
    }

    /* Convert header fields to host byte order: */
    seq_op = be64toh(((uint64_t *) req_msg)[0]);
    req.op = (uint8_t) (seq_op & 0x3);
    req.seq = seq_op & ~(uint64_t )0x3;

    /* Add sequence number to known sequence numbers and check for replays: */
    if (!add_sequence_number(req.seq)) {
        cerr << "Invalid sequence number" << endl;
        return;
    }

    /* Check if indicated payload length matches message length: */
    req.payload_len = be64toh(((uint64_t *) req_msg)[1]);
    if (req.payload_len != req_msg_len - MIN_MSG_LEN) {
        cerr << "Indicated length " << req.payload_len << "does not match expected length "
            << req_msg_len - MIN_MSG_LEN << endl;
        return;
    }

    req.payload = ((unsigned char *) req_msg) + 16;

    /* TODO: Call handler according to op */
    if (req.op == RDMA_GET) {
        send_response_get(req_handle, &req);
    }
    else if (req.op == RDMA_PUT) {
        cerr << "TODO: Implement" << endl;
    }
    else if (req.op == RDMA_DELETE) {
        cerr << "TODO: Implement" << endl;
    }
    else {
        cerr << "Invalid operation: " << req.op << endl;
    }

end_req_handler:
    free(plaintext);
}

/*
void req_handler(erpc::ReqHandle *req_handle, void *) {
    auto &resp = req_handle->pre_resp_msgbuf;
    rpc_host->resize_msg_buffer(&resp, kMsgSize);
    sprintf(reinterpret_cast<char *>(resp.buf), "hello");

    rpc_host->enqueue_response(req_handle, &resp);
}
*/

/* Dummy method for testing: We interpret the key as a filename and read from the according file */
unsigned char *kv_get(const char *key, size_t *data_length) {
    if (!(key && data_length)) {
        return nullptr;
    }

    unsigned char *data;
    FILE *file = fopen(key, "rb");
    if (!file) {
        cerr << "Could not open " << key << endl;
        return nullptr;
    }

    struct stat key_stat;
    if (!stat(key, &key_stat)) {
        cerr << "Could not get stats of " << key << endl;
        return nullptr;
    }

    /* Allocate enough memory for the whole message: */
    data = (unsigned char *) malloc(SEQ_LEN + SIZE_LEN + key_stat.st_size);
    if (!data) {
        cerr << "Memory allocation failure" << endl;
        goto end_get;
    }

    *data_length = fread(data + SIZE_LEN + SEQ_LEN, 1, key_stat.st_size, file);

end_get:
    fclose(file);
    return data;
}

int test_encryption() {
    int ret = -1;
    FILE *ciphertext_file = NULL;
    FILE *tag_file = NULL;
    unsigned char *ciphertext = NULL;
    unsigned char *plaintext2 = NULL;

    unsigned char tag[16];
    unsigned char iv[16];
    size_t tag_size = 16;
    size_t plaintext_size;
    char plaintext[] = "The quick brown fox jumps over the lazy dog";
    plaintext_size = strlen(plaintext) + 1;
    ciphertext_file = fopen("Ciphertext", "w");
    tag_file = fopen("Tag", "w");
    ciphertext = (unsigned char *)malloc(plaintext_size);
    plaintext2 = (unsigned char *)malloc(plaintext_size);
    if (!(ciphertext_file && tag_file && ciphertext && plaintext2)) {
        cerr << "Something went wrong" << endl;
        goto end_main;
    }

    encrypt((unsigned char *)plaintext, plaintext_size, iv, sizeof(iv_do_not_use),
            key_do_not_use, tag, tag_size, NULL, 0, ciphertext);
    fwrite((void *)ciphertext, 1, plaintext_size, ciphertext_file);
    fwrite((void *)tag, 1, tag_size, tag_file);

    decrypt(ciphertext, plaintext_size, iv, sizeof(iv_do_not_use),
            key_do_not_use, tag, tag_size, NULL, 0, plaintext2);

    if (0 != memcmp((void *)plaintext, (void *)plaintext2, plaintext_size)) {
        cerr << "En- or decryption doesn't work correctly yet..." << endl;
        cout << (char *) plaintext2 << endl;
    }
    else {
        cout << "Seems like the encryption and decryption works" << endl;
        cout << "Original Plaintext: " << plaintext << endl;
        cout << "Plaintext after en- and decryption: " << (char *) plaintext2 << endl;
        ret = 0;
    }

end_main:
    free(ciphertext);
    free(plaintext2);
    if (ciphertext_file) {
        fclose(ciphertext_file);
    }
    if (tag_file) {
        fclose(tag_file);
    }
    return ret;
}


