#include <cstring>
#include <cstdlib>

#include "client_server_common.h"
#include "Server.h"
#include "test_common.h"

#define MAX_TEST_SIZE (1 << 16)

void *test_kv_store[TEST_KV_SIZE] = { nullptr };

const void *kv_get(const void *key, size_t, size_t *data_len) {
    size_t index = *(size_t *) key;
    if (index < TEST_KV_SIZE) {
        *data_len = TEST_MAX_VAL_SIZE;
        return static_cast<void *>(test_kv_store + index);
    }
    else 
        return nullptr;
}


int kv_put(const void *key, size_t, void *value, size_t) {
    auto index = *(size_t *) key;
    if (index < TEST_KV_SIZE) {
        free(test_kv_store[index]);
        test_kv_store[index] = value;
        return 0;
    }
    else 
        return -1;
}

int kv_delete(const void *key, size_t) {
    auto index = *(size_t *) key;
    if (index < TEST_KV_SIZE) {
        free(test_kv_store[index]);
        test_kv_store[index] = nullptr;
        return 0;
    }
    else 
        return -1;
}


/* Fills the KV-store with initial data by calling value_from_key()
 * for each entry
 */
int initialize_kv_store() {
    for (size_t i = 0; i < TEST_KV_SIZE; i++) {
        test_kv_store[i] = malloc(TEST_MAX_VAL_SIZE);
        if (!test_kv_store[i]) {
            cerr << "Memory allocation failure" << endl;
            for (size_t j = 0; j < i; j++)
                free(test_kv_store[i]);
            return -1;
        }
        value_from_key(test_kv_store[i], (void *) &i, sizeof(size_t));
    }
    return 0;
}

void free_kv_store() {
    for (auto& entry : test_kv_store) {
        free(entry);
        entry = nullptr;
    }
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <ip-address>" << endl;
        return -1;
    }

    if (0 > initialize_kv_store()) {
        cerr << "Failed initializing KV-store. Aborting" << endl;
        return -1;
    }

    std::string ip(argv[1]);
    int ret = -1;
    const uint16_t standard_udp_port = 31850;
    if (anchor_server::host_server(ip, standard_udp_port,
            key_do_not_use, TOTAL_CLIENTS, 0,
            TEST_MAX_KEY_SIZE + TEST_MAX_VAL_SIZE, false,
            kv_get, kv_put, kv_delete)) {
        cerr << "Failed to host server" << endl;
        return ret;
    }

    anchor_server::close_connection(false);
    return 0;
}
