#include <cstring>
#include <cstdlib>

#include "client_server_common.h"
#include "Server.h"
#include "test_common.h"

#define TEST_KV_AT(index) (test_kv_store + (index) * VAL_SIZE)

char *test_kv_store;

const void *kv_get(const void *key, size_t, size_t *data_len) {
    size_t index = *(size_t *) key;
    if (index < KV_SIZE) {
        *data_len = VAL_SIZE;
        return static_cast<void *>(TEST_KV_AT(index));
    }
    else 
        return nullptr;
}


int kv_put(const void *key, size_t, void *value, size_t value_size) {
    auto index = *(size_t *) key;
    if (index < KV_SIZE) {
        memcpy(TEST_KV_AT(index), value, value_size);
        return 0;
    }
    else 
        return -1;
}

/* The delete operation is O(1) and can be used for simple latency tests */
int kv_delete(const void *key, size_t) {
    auto index = *(size_t *) key;
    if (index < KV_SIZE) {
        return 0;
    }
    else 
        return -1;
}


/* Fills the KV-store with initial data by calling value_from_key()
 * for each entry
 */
int initialize_kv_store() {
    test_kv_store = static_cast<char *>(malloc(KV_SIZE * VAL_SIZE));
    if (!test_kv_store)
        return -1;
    for (size_t i = 0; i < KV_SIZE; i++) {
        value_from_key(TEST_KV_AT(i), VAL_SIZE, (void *) &i, KEY_SIZE);
    }
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <ip-address>" << endl;
        return -1;
    }


    std::string ip(argv[1]);
    int ret = -1;
    const uint16_t standard_udp_port = 31850;
    if (0 != anchor_server::init(ip, standard_udp_port))
        return 1;
    for (size_t i = 0; i < NUMBER_TESTS; i++) {
        cout << "\n\n\n-----Starting test " << i << "-----\n" << endl;
        fill_global_test_params(i);
        if (0 > initialize_kv_store()) {
            cerr << "Failed initializing KV-store. Aborting" << endl;
            return -1;
        }
        if (anchor_server::host_server(
                key_do_not_use, NUM_CLIENTS,
                KEY_SIZE + VAL_SIZE, false,
                kv_get, kv_put, kv_delete)) {
            cerr << "Failed to host server" << endl;
            return ret;
        }

        anchor_server::close_connection(false);
        free(test_kv_store);
    }
    anchor_server::terminate();
    return 0;
}
