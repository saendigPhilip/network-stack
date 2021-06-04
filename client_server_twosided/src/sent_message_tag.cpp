//
// Created by philip on 04.06.21.
//

#include "sent_message_tag.h"

sent_message_tag::sent_message_tag() : valid{false} {}

void sent_message_tag::validate(const void *tag,
    status_callback cb, size_t *value_size) {

    value_len = value_size;
    user_tag = tag;
    callback = cb;
    valid = true;
}

/**
 * Invalidates the message tag and calls the according callback if not null
 */
void sent_message_tag::invalidate(enum ret_val ret) {
    valid = false;
    if (callback)
        callback(ret, user_tag);
}
