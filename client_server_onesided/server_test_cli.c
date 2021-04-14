//
// Created by philip on 05.04.21.
//
#include <stdlib.h>
#include <string.h>

#include "onesided_test.h"
#include "onesided_server.h"


unsigned char* test_kv_store = NULL;
size_t test_kv_store_size;

char test_kv_store_init[TEST_NUM_KV_ENTRIES][TEST_MAX_VALUE_SIZE]= {
        "zero",
        "one",
        "two",
        "three",
        "four",
        "five",
        "six",
        "seven",
        "eight",
        "nine",
        "ten",
        "eleven",
        "twelve",
        "thirteen",
        "fourteen",
        "fifteen"
};

size_t test_indices[TEST_NUM_KV_ENTRIES];

/* A possible implementation for a get function with the help of server_get
 * The array at value has to provide TEST_MAX_VALUE_SIZE bytes of space
 */
int get_function(const char *key, char *value) {
    struct local_key_info *info = get_key_info(key);
    if (!info)
        return -1;

    if (!server_get(info, value))
        return -1;
    else
        return 0;
}

/* A possible implementation for a get function with the help of server_get
 * The array at value has to be at least TEST_MAX_VALUE_SIZE bytes
 */
int put_function(const char *key, const char *value) {
    struct local_key_info *info = get_key_info(key);
    if (!info)
        return -1;

    return server_put(info, value);
}



int main(int argc, char **argv) {
    int ret = -1;
    if (parseargs(argc, argv))
        return ret;

    /* Set up KV-Store: */
    unsigned char *test_kv_store = setup_kv_store(
            key_do_not_use, sizeof(key_do_not_use), test_kv_store_init,
            TEST_NUM_KV_ENTRIES, TEST_MAX_VALUE_SIZE, &test_kv_store_size,
            local_key_infos);
    if (!test_kv_store) {
        PRINT_ERR("Failed to set up KV store");
        return ret;
    }

    printf("Setting up server at %s:%s\n", ip, port);
    if (0 > host_server(ip, port, test_kv_store, test_kv_store_size,
            TEST_MAX_VALUE_SIZE, key_do_not_use, sizeof(key_do_not_use))) {
        PRINT_ERR("Could not set up server");
        goto end_server_main;
    }
    puts("Waiting for client...");

    if (0 > accept_client())
        PRINT_ERR("Error while accepting client");
    else
        PRINT_INFO("Client connected!");

    communicate(get_function, put_function);

    ret = 0;

    /* Structures that have been set up during the connection process have to be deleted */
end_server_main:
    puts("Disconnecting and shutting down server");
    shutdown_rdma_server();
    free(test_kv_store);

    return ret;
}
