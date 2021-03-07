#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdio.h>
#include "rpc.h"
#include "nexus.h"

/* Request format:
 * +--------------+---------------------+------------------------+-------------------+
 * | Seq, OP (8B) | payload length (8B) | payload (address/data) | SHA256-Hash (32B) |
 * +--------------+---------------------+------------------------+-------------------+
 */
/* Struct is not (yet) for sending directly */
struct rdma_message{
    uint64_t seq_op;
    size_t length;
    char* payload;
    unsigned char* hash;
};

static constexpr uint8_t RDMA_RECV = 1;
static constexpr uint8_t RDMA_SEND = 2;

/* 	
Length of Hash: 256 bit (SHA256) 
Length of Sequence number: 64 bit
Length of PMEM Address: 64 bit
Length of Data to read/write: 64 bit
*/
static constexpr size_t HASH_LEN = 32, SEQ_LEN = 8, SIZE_LEN = 8;
static constexpr size_t MIN_MSG_LEN = HASH_LEN + SEQ_LEN + SIZE_LEN;

static const std::string kServerHostname = "192.168.178.28";
static const std::string kClientHostname = "192.168.178.28";

static constexpr uint16_t kUDPPort = 31850;
static constexpr size_t kMsgSize = 4096;

bool calculate_hash(const void *msg, size_t len, unsigned char *dest);
bool check_hash(const void *msg, size_t len);
bool add_sequence_number(uint64_t sequence_number);
uint8_t *read_data(const char *address, size_t address_size, size_t *data_length);

