#ifndef RDMA_CLIENT
#define RDMA_CLIENT

#include <iostream>
using namespace std;
#include <string>

#include "rpc.h"
#include "client_server_common.h"


namespace anchor_client {
    enum ret_val { OP_SUCCESS, OP_FAILED, TIMEOUT, INVALID_RESPONSE };

    static constexpr size_t MAX_ACCEPTED_RESPONSES = 32;
    typedef void (*status_callback)(enum ret_val, const void *);

    struct sent_message_tag{
        struct rdma_msg_header header;
        void *value;
        size_t *value_len;
        const void *user_tag;
        erpc::MsgBuffer *request;
        erpc::MsgBuffer *response;
        status_callback callback;
        bool valid;
    };

    class Client {
    private:

        /* eRPC session number */
        int session_nr;
        uint8_t erpc_id;
        /* This is always the next sequence number that the Client sends */
        uint64_t current_seq_op = 0;
        erpc::Rpc<erpc::CTransport> *client_rpc;
        struct sent_message_tag accepted[MAX_ACCEPTED_RESPONSES];

        void invalidate_message_tag(
                struct anchor_client::sent_message_tag *tag);

        void send_message(struct sent_message_tag *tag, size_t loop_iterations);

        int allocate_req_buffers(
                erpc::MsgBuffer *req, size_t req_size,
                erpc::MsgBuffer *resp, size_t resp_size);


        static void decrypt_cont_func(void *context, void *message_tag);

        void message_arrived(enum ret_val ret, uint64_t seq_op);

        void invalidate_old_requests(uint64_t seq_op);

        sent_message_tag *prepare_new_request();

        void send_disconnect_message();

    public:

        Client(std::string& client_hostname, uint16_t udp_port, uint8_t id);
        ~Client();

        int connect(std::string& server_hostname,
                unsigned int udp_port, const unsigned char *encryption_key);

        int get(const void *key, size_t key_len,
                void *value, size_t max_value_len, size_t *value_len,
                anchor_client::status_callback callback, const void *user_tag,
                size_t loop_iterations = 100);

        int put(const void *key, size_t key_len, const void *value, size_t value_len,
                anchor_client::status_callback callback, const void *user_tag,
                size_t loop_iterations = 100);

        int del(const void *key, size_t key_len,
                anchor_client::status_callback callback, const void *user_tag,
                size_t loop_iterations = 100);


        bool queue_full();

        int disconnect();
    };
}


#endif //RDMA_CLIENT


