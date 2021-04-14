//
// Created by philip on 06.04.21.
//

#ifndef CLIENT_SERVER_ONESIDED_ONESIDED_GLOBAL_H
#define CLIENT_SERVER_ONESIDED_ONESIDED_GLOBAL_H
#include <inttypes.h>
#include <stddef.h>


/* Message format:
 * +----+-------------+-------+-----+
 * | IV | Seq. Number | Value | MAC |
 * +----+-------------+-------+-----+
 * ------------+
 *  Value Size |
 * ------------+
 */
#define IV_SIZE 12
#define SEQ_SIZE 8
#define MAC_SIZE 16
#define MIN_VALUE_SIZE (IV_SIZE + SEQ_SIZE + MAC_SIZE)

#define VALUE_ENTRY_SIZE(value_size) (MIN_VALUE_SIZE + (value_size))


/**
 * Every server and client has to store one such struct for each key
 * Having the address and the sequence number of each value is essential for
 * onesided RDMA
 */
struct local_key_info {
    uint64_t sequence_number;
    void *key;
    size_t key_size;
    size_t value_offset;
};

void *malloc_aligned(size_t size);

#endif //CLIENT_SERVER_ONESIDED_ONESIDED_GLOBAL_H
