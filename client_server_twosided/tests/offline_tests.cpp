//
// Created by philip on 04.06.21.
//
#include <cstdint>

#include "simple_unit_test.h"
#include "PendingRequestQueue.h"

constexpr int64_t SEQ_THRESHOLD = 32;
uint8_t client_id = 0xff;
uint64_t next_seq;

bool server_is_seq_valid(uint64_t sequence_number) {
    uint8_t id = ID_FROM_SEQ_OP(sequence_number);
    if (id != client_id) {
        cerr << "Invalid Client ID" << endl;
        return false;
    }
    if (next_seq == 0) {
        next_seq = sequence_number & SEQ_MASK;
        return true;
    }
    auto seq_diff = static_cast<ssize_t>(SEQ_FROM_SEQ_OP(
        (sequence_number & SEQ_MASK) - next_seq));
    if (seq_diff < SEQ_THRESHOLD && seq_diff > -SEQ_THRESHOLD) {
        if (seq_diff > 0)
            next_seq = sequence_number & SEQ_MASK;
        return true;
    }
    else {
        fprintf(stderr, "Expected: %lx, Got: %lx\n",
            next_seq, sequence_number & SEQ_MASK);
        return false;
    }
}

/*
 * Returns the next sequence number and updates the next expected sequence
 * number for the according client
 * Should only be called with already checked sequence numbers
 */
uint64_t server_get_next_seq(uint64_t sequence_number, uint8_t operation) {
    uint64_t ret = NEXT_SEQ(sequence_number);
    next_seq = NEXT_SEQ(ret & SEQ_MASK);
    return SET_OP(ret, operation);
}

bool client_seq_check(uint64_t incoming_seq, uint64_t local_seq) {
    return (incoming_seq & (SEQ_MASK | ID_MASK)) ==
        (NEXT_SEQ(local_seq) & (ID_MASK | SEQ_MASK));
}

void test_callback(enum ret_val ret, const void *) {
    EXPECT_EQUAL(ret, OP_SUCCESS);
}

void seq_test(size_t batch_size) {
    PendingRequestQueue queue{client_id};
    uint64_t seq;
    uint8_t op = 0;
    next_seq = 0;
    std::vector<msg_tag_t *> tags;
    for (size_t i = 0; i < 2; i++) {
        for (size_t j = 0; j < batch_size; j++) {
            tags.emplace_back(queue.prepare_new_request(op, nullptr, nullptr));
            queue.inc_seq();
        }

        for (auto tag : tags) {
            EXPECT_TRUE(server_is_seq_valid(tag->header.seq_op));
            seq = server_get_next_seq(tag->header.seq_op, op);
            EXPECT_TRUE(client_seq_check(seq, tag->header.seq_op));
            queue.message_arrived(OP_SUCCESS, seq);
        }
        tags.clear();
    }
}


int main() {
    for (size_t i = 1; i <= MAX_ACCEPTED_RESPONSES; i *= 2)
        seq_test(i);
    PRINT_TEST_SUMMARY();
}

