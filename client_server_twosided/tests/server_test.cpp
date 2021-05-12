#include <cstring>
#include <cstdlib>

#include "Server.h"
#include "simple_unit_test.h"
#include "test_common.h"

char *test_kv_store[TEST_KV_SIZE] = {nullptr};

long int get_index(const char *key) {
    char *err;
    long int index = strtol(key, &err, 0);
    if (*key != '\0' && *err == '\0' &&
            index >= 0 && (size_t) index < TEST_KV_SIZE) {
        return index;
    }
    return -1;
}

void *kv_get(const void *key, size_t, size_t *data_len) {
    long int index = get_index((char *) key);
    if (index >= 0 && test_kv_store[index]) {
        *data_len = strlen(test_kv_store[index]) + 1;
        return (void*) (test_kv_store + index);
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
    test_kv_store[index] = static_cast<char *>(malloc(value_size));
    if (!test_kv_store[index]) {
        cerr << "Memory allocation failure" << endl;
        return -1;
    }
    strncpy(static_cast<char *>(test_kv_store[index]),
            static_cast<char *>(value), value_size);
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
            TEST_MAX_KEY_SIZE + TEST_MAX_VAL_SIZE,
            kv_get, kv_put, kv_delete)) {
        cerr << "Failed to host server" << endl;
        return ret;
    }
    anchor_server::run_event_loop(1000000);
    cout << "Shutting down server" << endl;

    anchor_server::close_connection();

    for(size_t i = 0; i < TEST_KV_SIZE; i++)
        free(test_kv_store[i]);

    PRINT_TEST_SUMMARY();
    return 0;
}
