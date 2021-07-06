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

/**
 * Constructs a queue and initializes the sequence number
 * @param id ID for identification at the server side
 *          (will be included in the sequence number)
 */
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

/**
 * Allocate all request buffers that are needed throughout the session
 * @param rpc rpc-object that is needed to allocate the buffer
 * @param req_size Maximum request size -> Buffer size for requests
 * @param resp_size Maximum response size -> Buffer size for responses
 */
void PendingRequestQueue::allocate_req_buffers(
        erpc::Rpc<erpc::CTransport> &rpc, size_t req_size, size_t resp_size) {

    for (auto& buf : this->queue) {
        buf.valid = false;
        buf.request = rpc.alloc_msg_buffer_or_die(req_size);
        buf.response = rpc.alloc_msg_buffer_or_die(resp_size);
    }
}

/**
 * Frees all request buffers
 * @param rpc rpc-object that was used to allocate the buffers
 */
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
    erpc::Rpc<erpc::CTransport>& client_rpc,
    uint8_t op, const void *user_tag, status_callback cb, size_t *value_size) {

    size_t index = ACCEPTED_INDEX(this->current_seq_op);
    msg_tag_t *ret = this->queue + index;
    while (unlikely(ret->valid)) {
        client_rpc.run_event_loop_once();
    }

    // Fill the struct with the provided values:
    ret->validate(user_tag, cb, value_size);
    ret->header.seq_op = SET_OP(this->current_seq_op, op);

    return ret;
}


/**
 * Needs to be called when a server response arrives.
 * Looks up whether the according
 * @param ret
 * @param seq_op
 */
void PendingRequestQueue::message_arrived(enum ret_val ret, uint64_t seq_op) {
    size_t index = ACCEPTED_INDEX(PREV_SEQ(seq_op));
    msg_tag_t& tag = this->queue[index];
    /* If this is an expired answer to a request or a replay, we're done */
    if (unlikely(!tag.valid)) {
        cerr << "Expired message arrived" << endl;
        return;
    }

    /* Call the Client callback and invalidate */
    tag.invalidate(ret);
}


void PendingRequestQueue::invalidate_all_requests() {
    size_t index = ACCEPTED_INDEX(this->current_seq_op);
    for (size_t i = 0; i < MAX_ACCEPTED_RESPONSES; i++) {
        if (this->queue[index].valid) {
            this->queue[index].invalidate(ret_val::TIMEOUT);
        }
        index++;
        index %= MAX_ACCEPTED_RESPONSES;
    }
}


bool PendingRequestQueue::queue_full() {
    // If the request at the current sequence number index is valid,
    // we know that the queue is full
    return this->queue[ACCEPTED_INDEX(this->current_seq_op)].valid;
}
