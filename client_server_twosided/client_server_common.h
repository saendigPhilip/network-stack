#ifndef RDMA_COMMON_METHODS
#define RDMA_COMMON_METHODS

#include <iostream>
using namespace std;


// #define DEBUG

const char ERR_ENC[] = "Error: Something went wrong while encrypting";
const char ERR_DEC[] = "Error: Something went wrong while decrypting";

/* Request format:
 * +----------+--------------+---------------------+---------------+------------+
 * | IV (12B) | Seq, OP (8B) | payload length (8B) | key [+ value] | GMAC (16B) |
 * +----------+--------------+---------------------+---------------+------------+
 *            |-------------encrypted and authenticated------------|
 */

static constexpr uint8_t RDMA_GET = 0;
static constexpr uint8_t RDMA_PUT = 1;
static constexpr uint8_t RDMA_DELETE = 2;

static const unsigned char key_do_not_use[16] =
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
static const unsigned char iv_do_not_use[12] =
        {11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

struct rdma_req {
    uint64_t seq;
    uint8_t op;
    size_t payload_len;
    unsigned char *payload;
};

/* Length of IV: 12 Byte
 * Length of GMAC Tag: 16 Byte
 * Length of Sequence number: 64 bit
 * Length of Data size to read/write: 64 bit
*/
static constexpr size_t IV_LEN = 12, MAC_LEN = 16, SEQ_LEN = 8, SIZE_LEN = 8;
static constexpr size_t MIN_MSG_LEN = IV_LEN + MAC_LEN + SEQ_LEN + SIZE_LEN;

static const std::string kServerHostname = "192.168.178.28";
static const std::string kClientHostname = "192.168.178.28";

static constexpr uint16_t kUDPPort = 31850;
static constexpr size_t kMsgSize = 4096;

// int calculate_hash(const void *msg, size_t len, unsigned char *dest);
// int check_hash(const void *msg, size_t len);
int add_sequence_number(uint64_t sequence_number);

int encrypt(
        const unsigned char *plaintext, size_t plaintext_len,
        unsigned char *iv, size_t iv_len,
        const unsigned char *key,
        unsigned char *tag, size_t tag_len,
        const unsigned char *aad, size_t aad_len,
        unsigned char *ciphertext);

int decrypt(
        const unsigned char *ciphertext, size_t ciphertext_len,
        const unsigned char *iv, size_t iv_len,
        const unsigned char *key,
        unsigned char *tag, size_t tag_len,
        const unsigned char *aad, size_t aad_len,
        unsigned char *plaintext);

#endif // RDMA_COMMON_METHODS
