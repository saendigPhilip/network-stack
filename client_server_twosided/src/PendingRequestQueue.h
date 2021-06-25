//
// Created by philip on 04.06.21.
//

#ifndef CLIENT_SERVER_TWOSIDED_PENDINGREQUESTQUEUE_H
#define CLIENT_SERVER_TWOSIDED_PENDINGREQUESTQUEUE_H

#include "rpc.h"
#include "client_server_common.h"
#include "sent_message_tag.h"

static constexpr size_t MAX_ACCEPTED_RESPONSES = 256;


class PendingRequestQueue {
private:
    uint64_t current_seq_op = 0;
    msg_tag_t queue[MAX_ACCEPTED_RESPONSES];

public:

    explicit PendingRequestQueue(uint8_t id);

    void allocate_req_buffers(
        erpc::Rpc<erpc::CTransport>& rpc, size_t req_size, size_t resp_size);

    void free_req_buffers(erpc::Rpc<erpc::CTransport>& rpc);

    msg_tag_t *prepare_new_request(uint8_t op, const void *user_tag,
        status_callback cb, size_t *value_size=nullptr);

    void message_arrived(enum ret_val ret, uint64_t seq_op);

    void invalidate_all_requests();

    bool queue_full();

    inline void inc_seq() {
        /* Skip one sequence number for the server response */
        this->current_seq_op = NEXT_SEQ(NEXT_SEQ(this->current_seq_op));
    }

};


#endif //CLIENT_SERVER_TWOSIDED_PENDINGREQUESTQUEUE_H
