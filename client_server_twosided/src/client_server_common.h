#ifndef RDMA_COMMON_METHODS
#define RDMA_COMMON_METHODS

#include <iostream>
using namespace std;


#define CIPHERTEXT_SIZE(payload_size) (MIN_MSG_LEN + (payload_size))
#define PAYLOAD_SIZE(ciphertext_size) ((ciphertext_size) - MIN_MSG_LEN)

/*
 * Macros for handling seq_op numbers. seq_op is 64 bit and looks like this:
 * +--------------------------+------------+------------+
 * | Sequence number (54 bit) | ID (8 bit) | OP (2 bit) |
 * +--------------------------+------------+------------+
 */
#define SEQ_MASK ~((uint64_t) 0x3ff)    // ~(11 1111 1111)
#define ID_MASK (uint64_t) 0x3fc        //   11 1111 1100
#define OP_MASK (uint64_t) 0x3          //   00 0000 0011

#define NEXT_SEQ(seq_number) ((seq_number) + (1 << 10))

#define SEQ_FROM_SEQ_OP(seq_number) (((seq_number) & SEQ_MASK) >> 10)
#define ID_FROM_SEQ_OP(seq_number) (((seq_number) & ID_MASK) >> 2)
#define OP_FROM_SEQ_OP(seq_number) ((seq_number) & OP_MASK)

#define SET_OP(seq_number, op) (((seq_number) & ~OP_MASK) | (op))
#define SET_ID(seq_number, id) \
    (((seq_number) & SEQ_MASK) | ((uint64_t) (id) << 2))


// #define DEBUG

static constexpr uint8_t DEFAULT_REQ_TYPE = 0;

static constexpr uint8_t RDMA_GET = 0b00;
static constexpr uint8_t RDMA_PUT = 0b01;
static constexpr uint8_t RDMA_DELETE = 0b10;
static constexpr uint8_t RDMA_ERR = 0b11;

struct rdma_msg_header {
    uint64_t seq_op;
    uint64_t key_len;
};

struct rdma_enc_payload {
    const unsigned char *key;
    const unsigned char *value;
    size_t value_len;
};

struct rdma_dec_payload {
    unsigned char *key;
    unsigned char *value;
    size_t value_len;
};

/* Request format:
 * +----------+--------------+-----------------+-----------+------------+
 * | IV (12B) | Seq, OP (8B) | key length (8B) | key/value | GMAC (16B) |
 * +----------+--------------+-----------------+-----------+------------+
 *            |---------encrypted and authenticated--------|
 */

/* Length of IV: 12 Byte
 * Length of GMAC Tag: 16 Byte
 * Length of Sequence number: 64 bit
 * Length of Data size to read/write: 64 bit
*/
static constexpr size_t IV_LEN = 12, MAC_LEN = 16, SEQ_LEN = 8, SIZE_LEN = 8;
static constexpr size_t ENC_KEY_LEN = 16;
static constexpr size_t MIN_MSG_LEN = IV_LEN + MAC_LEN + SEQ_LEN + SIZE_LEN;

static const std::string kServerHostname = "192.168.178.28";
static const std::string kClientHostname = "192.168.178.28";

static constexpr uint16_t kUDPPort = 31850;
static constexpr size_t kMsgSize = 4096;


int encrypt_message(const unsigned char *encryption_key, const struct rdma_msg_header *header,
        const struct rdma_enc_payload *payload, unsigned char **ciphertext);

int decrypt_message(const unsigned char *decryption_key, struct rdma_msg_header *header,
        struct rdma_dec_payload *payload, const unsigned char *ciphertext, size_t ciphertext_len);

#endif // RDMA_COMMON_METHODS
