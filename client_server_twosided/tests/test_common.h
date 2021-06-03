//
// Created by philip on 28.04.21.
//

#ifndef TWOSIDED_COMMUNICATION_TEST_COMMON_H
#define TWOSIDED_COMMUNICATION_TEST_COMMON_H

#include <cstdint>

#define KEY_SIZE global_params.key_size
#define VAL_SIZE global_params.val_size
#define KV_SIZE global_params.kv_size
#define NUM_CLIENTS global_params.num_clients
#define LOOP_ITERATIONS global_params.event_loop_iterations
#define CLIENT_TIMEOUT global_params.client_timeout_us
#define PUTS_PER_CLIENT global_params.puts_per_client
#define GETS_PER_CLIENT global_params.gets_per_client
#define DELS_PER_CLIENT global_params.dels_per_client


struct global_test_params {
    size_t key_size{256};
    size_t val_size{1024};
    size_t kv_size{128};
    uint8_t num_clients{4};
    size_t event_loop_iterations{128};
    size_t client_timeout_us{1};
    size_t puts_per_client{1 << 12};
    size_t gets_per_client{1 << 12};
    size_t dels_per_client{1 << 12};

    int parse_args(int argc, const char *argv[]);
    static void print_options();

};

extern struct global_test_params global_params;

//static constexpr size_t MAX_KEY_SIZE = 8192;
//static constexpr size_t MAX_VAL_SIZE = 8192;

static const unsigned char key_do_not_use[16] =
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};


/* We define a function to retrieve a key from a certain value,
 * so the KV-store is kept consistent and we can eventually perform
 * checks on the correctness of the returned and put values
 */
void value_from_key(
        void *value, size_t value_len, const void *key, size_t key_len);

#endif //TWOSIDED_COMMUNICATION_TEST_COMMON_H
