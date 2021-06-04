//
// Created by philip on 04.06.21.
//

#include <openssl/rand.h>
#include "PendingRequestQueue.h"

/* Because the last bit is always the same at client side, we shift the sequence
 * number to the right in order to use the whole accepted array */
#define ACCEPTED_INDEX(seq_op) \
        ((SEQ_FROM_SEQ_OP(seq_op) >> 1) % MAX_ACCEPTED_RESPONSES)

#define DEC_INDEX(index) index = \
        (((index) + MAX_ACCEPTED_RESPONSES - 1) % MAX_ACCEPTED_RESPONSES)

PendingRequestQueue::PendingRequestQueue(uint8_t id) {
    if (RAND_status() != 1) {
        if (RAND_poll() != 1) {
            throw std::runtime_error("Couldn't initialize RNG");
        }
    }
    if (1 != RAND_bytes((unsigned char *) &current_seq_op,
        sizeof(current_seq_op))) {
        throw std::runtime_error("Couldn't generate initial sequence number");
    }
    current_seq_op = SET_ID(current_seq_op, id);
}

void PendingRequestQueue::allocate_req_buffers(
        erpc::Rpc<erpc::IBTransport> &rpc, size_t req_size, size_t resp_size) {

    for (auto& buf : this->queue) {
        buf.valid = false;
        buf.request = rpc.alloc_msg_buffer_or_die(req_size);
        buf.response = rpc.alloc_msg_buffer_or_die(resp_size);
    }
}

void PendingRequestQueue::free_req_buffers(erpc::Rpc<erpc::CTransport>& rpc) {
    for (auto& buf : this->queue) {
        rpc.free_msg_buffer(buf.request);
        rpc.free_msg_buffer(buf.response);
    }
}

/**
 * Finds and fills an according message tag for a new request
 * Also fills in the sequence number to the according header
 * @param op Operation to be performed (e.g. RDMA_PUT)
 * @param user_tag Tag of the caller
 * @param cb Callback of the caller
 * @param value_size In case that a value is received afterwards, a pointer can
 *      be specified in which the length of the incoming value is stored
 * @return A pointer to a message tag that was filled as described before
 */
msg_tag_t *PendingRequestQueue::prepare_new_request(
    uint8_t op, const void *user_tag, status_callback cb, size_t *value_size) {

    size_t index = ACCEPTED_INDEX(this->current_seq_op);
    msg_tag_t *ret = this->queue + index;
    if (ret->valid) {
        ret->invalidate(ret_val::TIMEOUT);
        this->invalidate_old_requests(ret->header.seq_op);
    }

    // Fill the struct with the provided values:
    ret->validate(user_tag, cb, value_size);
    ret->header.seq_op = SET_OP(this->current_seq_op, op);

    return ret;
}


void PendingRequestQueue::message_arrived(enum ret_val ret, uint64_t seq_op) {
    size_t index = ACCEPTED_INDEX(PREV_SEQ(seq_op));
    msg_tag_t& tag = this->queue[index];
    /* If this is an expired answer to a request or a replay, we're done */
    if (!tag.valid) {
        cerr << "Expired message arrived" << endl;
        return;
    }

    /* Call the Client callback and invalidate */
    tag.invalidate(ret);

    /* To make sure we're making progress, we invalidate all older requests: */
    invalidate_old_requests(seq_op);
}


/* Iterates over the queue until a request is found that Deletes all requests
 * that have a lower sequence number than seq and invokes the according client
 * callbacks.
 * This way, we can always guarantee that for each message the client callback
 * is invoked
 * seq_op is the latest sequence number that should be accepted
 */
void PendingRequestQueue::invalidate_old_requests(uint64_t seq_op) {
    seq_op = PREV_SEQ(PREV_SEQ(seq_op));
    uint64_t orig_seq = SEQ_FROM_SEQ_OP(seq_op);
    int64_t diff;
    size_t index = ACCEPTED_INDEX(seq_op);
    for (size_t i = 0; i < MAX_ACCEPTED_RESPONSES; DEC_INDEX(index), i++) {
        // Invariant:
        // All tags "before" invalid tags are either invalid or fresh

        // Thanks to the invariant we know that when we encounter a valid tag,
        // all tags before it have to be valid as well
        if (!(this->queue[index].valid))
            break;

        uint64_t current_seq =
            SEQ_FROM_SEQ_OP(this->queue[index].header.seq_op);
        diff = (int64_t) (orig_seq - current_seq);

        // diff < 0 indicates that the sequence number of the current tag is
        // greater than the reference tag
        if (diff < 0)
            break;

        this->queue[index].invalidate(ret_val::TIMEOUT);
    }
}

bool PendingRequestQueue::queue_full() {
    // If the request at the current sequence number index is valid,
    // we know that the queue is full
    return this->queue[ACCEPTED_INDEX(this->current_seq_op)].valid;
}
