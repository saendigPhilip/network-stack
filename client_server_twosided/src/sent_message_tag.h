//
// Created by philip on 04.06.21.
//

#ifndef CLIENT_SERVER_TWOSIDED_SENT_MESSAGE_TAG_H
#define CLIENT_SERVER_TWOSIDED_SENT_MESSAGE_TAG_H
#include "client_server_common.h"
#include "rpc.h"

enum ret_val { OP_SUCCESS, OP_FAILED, TIMEOUT, INVALID_RESPONSE };
typedef void (*status_callback)(enum ret_val, const void *);

struct sent_message_tag {
    struct rdma_msg_header header;
    void *value;
    size_t *value_len;
    const void *user_tag;
    erpc::MsgBuffer request;
    erpc::MsgBuffer response;
    status_callback callback;
    bool valid;

    sent_message_tag();

    void validate(const void *user_tag,
        status_callback cb, size_t *value_size=nullptr);

    void invalidate(enum ret_val ret);

};
typedef struct sent_message_tag msg_tag_t;


#endif //CLIENT_SERVER_TWOSIDED_SENT_MESSAGE_TAG_H
