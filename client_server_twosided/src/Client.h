#ifndef RDMA_CLIENT
#define RDMA_CLIENT

#include <iostream>
#include <string>

#include "rpc.h"
#include "client_server_common.h"
#include "PendingRequestQueue.h"


class Client {
private:

    /* eRPC session number */
    int session_nr;
    uint8_t erpc_id;
    /* This is always the next sequence number that the Client sends */
    erpc::Rpc<erpc::CTransport> client_rpc;
    PendingRequestQueue queue;

    size_t max_key_size;
    size_t max_val_size;

    void send_message(msg_tag_t *tag, size_t loop_iterations);

    static void decrypt_cont_func(void *context, void *message_tag);

    void send_disconnect_message();

public:

    static void init(std::string& client_hostname, uint16_t udp_port);
    static void terminate();


    Client(uint8_t id, size_t max_key_size, size_t max_val_size);
    ~Client();

    int connect(std::string& server_hostname,
            unsigned int udp_port, const unsigned char *encryption_key);

    int disconnect();

    int get(const void *key, size_t key_len,
            void *value, size_t *value_len,
            status_callback callback, const void *user_tag,
            size_t loop_iterations = 1000);

    int put(const void *key, size_t key_len, const void *value, size_t value_len,
            status_callback callback, const void *user_tag,
            size_t loop_iterations = 1000);

    int del(const void *key, size_t key_len,
            status_callback callback, const void *user_tag,
            size_t loop_iterations = 1000);


    void run_event_loop_n_times(size_t n);

    bool queue_full();
};


#endif //RDMA_CLIENT


