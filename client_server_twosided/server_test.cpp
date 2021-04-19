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
    unsigned char *ciphertext = nullptr;
    void *decrypted_payload = nullptr;
    int header_equal, payload_equal;
    struct rdma_msg_header header = {
            40 | RDMA_GET,
            (uint64_t) plaintext_size,
    };
    struct rdma_msg_header arrived_header;

    if (0 != encrypt_message(key_do_not_use, &header,
            &ciphertext, (void *) plaintext, plaintext_size)) {
        cerr << "Encryption failed" << endl;
        goto end_test_encryption;
    }
    if (0 != decrypt_message(key_do_not_use, &arrived_header, ciphertext,
            &decrypted_payload, ciphertext_size)) {
        cerr << "Decryption failed" << endl;
        goto end_test_encryption;
    }

    header_equal = memcmp((void *)&header, (void *)&arrived_header, sizeof(struct rdma_msg_header));
    payload_equal = memcmp((void *) plaintext, decrypted_payload, plaintext_size);
    if (header_equal || payload_equal) {
        cerr << "En- or decryption doesn't work correctly yet..." << endl;
    }
    else {
        cout << "Seems like the encryption and decryption works" << endl;
        cout << "Original Plaintext: " << plaintext << endl;
        cout << "Plaintext after en- and decryption: " << (char *) decrypted_payload << endl;
        ret = 0;
    }

end_test_encryption:
    free(ciphertext);
    free(decrypted_payload);
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
