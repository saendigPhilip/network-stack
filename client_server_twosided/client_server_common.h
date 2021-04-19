#ifndef RDMA_COMMON_METHODS
#define RDMA_COMMON_METHODS

#define CIPHERTEXT_SIZE(payload_size) MIN_MSG_LEN + (payload_size)
#define PAYLOAD_SIZE(ciphertext_size) (ciphertext_size) - MIN_MSG_LEN

#include <iostream>
using namespace std;


// #define DEBUG

/* Request format:
 * +----------+--------------+-----------------+---------------+------------+
 * | IV (12B) | Seq, OP (8B) | key length (8B) | key [+ value] | GMAC (16B) |
 * +----------+--------------+-----------------+---------------+------------+
 *            |-----------encrypted and authenticated----------|
 */

static constexpr uint8_t RDMA_GET = 0b00;
static constexpr uint8_t RDMA_PUT = 0b01;
static constexpr uint8_t RDMA_DELETE = 0b10;
static constexpr uint8_t RDMA_ERR = 0b11;


struct rdma_msg {
    uint64_t seq_op;
    uint64_t key_len;
    void *payload;
};

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



// int calculate_hash(const void *msg, size_t len, unsigned char *dest);
// int check_hash(const void *msg, size_t len);
int add_sequence_number(uint64_t sequence_number);

int encrypt_message(const unsigned char *encryption_key,
        const struct rdma_msg *msg, unsigned char *ciphertext, size_t payload_len);

int decrypt_message(const unsigned char *decryption_key,
        struct rdma_msg *msg, const unsigned char *ciphertext, size_t ciphertext_len);

#endif // RDMA_COMMON_METHODS
