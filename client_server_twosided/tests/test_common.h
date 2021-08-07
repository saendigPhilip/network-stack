//
// Created by philip on 28.04.21.
//

#ifndef TWOSIDED_COMMUNICATION_TEST_COMMON_H
#define TWOSIDED_COMMUNICATION_TEST_COMMON_H

#include <cstdint>

#define KEY_SIZE global_params.key_size
#define VAL_SIZE global_params.val_size
#define MAX_KEY global_params.max_key_size
#define NUM_CLIENTS global_params.num_clients
#define LOOP_ITERATIONS global_params.event_loop_iterations
#define TOTAL_PUTS global_params.total_puts
#define TOTAL_GETS global_params.total_gets
#define TOTAL_DELS global_params.total_dels
#define MIN_TIME global_params.minimum_time
#define PATH_CSV global_params.path_csv


struct global_test_params {
    size_t key_size{8};
    size_t val_size{256};
    uint32_t max_key_size{0};
    uint8_t num_clients{1};
    size_t event_loop_iterations{1};
    size_t total_puts{1 << 12};
    size_t total_gets{1 << 12};
    size_t total_dels{1 << 12};
    size_t minimum_time{0};
    const char *path_csv{nullptr};

    int parse_args(int argc, const char *argv[]);
    static void print_options();

};

extern struct global_test_params global_params;

static const unsigned char key_do_not_use[16] =
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};


/* We define a function to retrieve a key from a certain value,
 * so the KV-store is kept consistent and we can eventually perform
 * checks on the correctness of the returned and put values
 */
void value_from_key(
        void *value, size_t value_len, const void *key, size_t key_len);


inline uint64_t time_diff(
    const struct timespec *start, const struct timespec *end){
    auto diff_sec = static_cast<uint64_t>(end->tv_sec - start->tv_sec);
    return diff_sec * 1'000'000'000ull + static_cast<uint64_t>(end->tv_nsec)
           - static_cast<uint64_t>(start->tv_nsec);
}


#endif //TWOSIDED_COMMUNICATION_TEST_COMMON_H
