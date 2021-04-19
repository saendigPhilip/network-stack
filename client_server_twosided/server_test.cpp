#include <cstring>

#include "client_server_common.h"
#include "Server.h"

static const unsigned char key_do_not_use[16] =
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

int test_encryption() {
    int ret = -1;
    char plaintext[] = "The quick brown fox jumps over the lazy dog";
    size_t plaintext_size = sizeof(plaintext);
    size_t ciphertext_size = CIPHERTEXT_SIZE(plaintext_size);
    unsigned char *ciphertext = (unsigned char *) malloc(ciphertext_size);
    if (!ciphertext) {
        cerr << "Memory allocation failure" << endl;
        return -1;
    }
    struct rdma_msg msg = {
            40 | RDMA_GET,
            (uint64_t) plaintext_size,
            (void *) plaintext
    };
    struct rdma_msg arrived_msg;
    arrived_msg.payload = nullptr;

    if (0 != encrypt_message(key_do_not_use, &msg, ciphertext, plaintext_size)) {
        cerr << "Encryption failed" << endl;
        goto end_test_encryption;
    }
    if (0 != decrypt_message(key_do_not_use, &arrived_msg, ciphertext, ciphertext_size)) {
        cerr << "Decryption failed" << endl;
        goto end_test_encryption;
    }

    if (0 != memcmp((void *)&msg, (void *)&arrived_msg,
            sizeof(msg.seq_op) + sizeof(msg.key_len))) {
        cerr << "En- or decryption doesn't work correctly yet..." << endl;
    }
    else {
        cout << "Seems like the encryption and decryption works" << endl;
        cout << "Original Plaintext: " << plaintext << endl;
        cout << "Plaintext after en- and decryption: " << (char *) arrived_msg.payload << endl;
        ret = 0;
    }

end_test_encryption:
    free(ciphertext);
    free(arrived_msg.payload);
    return ret;
}


int main() {
    return test_encryption();
    /*
    int ret = -1;
    std::string ip = "192.168.2.113";
    const uint16_t standard_udp_port = 31850;
    if (host_server(ip, standard_udp_port, 100000)) {
        cerr << "Failed to host server" << endl;
        return ret;
    }
     */
}
