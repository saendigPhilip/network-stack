#include <cstring>
#include <cstdlib>

#include "Server.h"
#include "simple_unit_test.h"
#include "test_common.h"

constexpr size_t TEST_KV_SIZE = 256;

char *test_kv_store[TEST_KV_SIZE] = {nullptr};

long int get_index(const char *key) {
    char *err;
    long int index = strtol(key, &err, 0);
    if (*key != '\0' && *err == '\0' &&
            index >= 0 && (size_t) index < KV_SIZE) {
        return index;
    }
    return -1;
}

const void *kv_get(const void *key, size_t, size_t *data_len) {
    long int index = get_index((char *) key);
    if (index >= 0 && test_kv_store[index]) {
        *data_len = MAX_VAL_SIZE;
        return static_cast<const void*>(test_kv_store[index]);
    }
    else 
        return nullptr;
}


/* Dummy for put function */
int kv_put(const void *key, size_t, void *value, size_t value_size) {
    long int index = get_index((char *) key);
    if (index < 0)
        return -1;

    free(test_kv_store[index]);
    if (value_size == 0) {
        test_kv_store[index] = nullptr;
        return 0;
    }
    test_kv_store[index] = static_cast<char *>(malloc(MAX_VAL_SIZE));
    if (!test_kv_store[index]) {
        cerr << "Memory allocation failure" << endl;
        return -1;
    }
    memcpy(test_kv_store[index], value, value_size);
    return 0;
}

int kv_delete(const void *key, size_t) {
    long int index = get_index((char *) key);
    if (index >= 0) {
        free(test_kv_store[index]);
        test_kv_store[index] = nullptr;
        return 0;
    }
    else 
        return -1;
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <ip-address>" << endl;
        return -1;
    }
    int ret = -1;
    std::string ip(argv[1]);
    const uint16_t standard_udp_port = 31850;
    const uint8_t num_clients = 1;
    if (anchor_server::host_server(ip, standard_udp_port,
            key_do_not_use, num_clients, 0,
            MAX_KEY_SIZE + MAX_VAL_SIZE, true,
            kv_get, kv_put, kv_delete)) {
        cerr << "Failed to host server" << endl;
        return ret;
    }

    anchor_server::close_connection(false);
    cout << "Shut down server" << endl;

    for(auto entry : test_kv_store)
        free(entry);

    PRINT_TEST_SUMMARY();
    return 0;
}
