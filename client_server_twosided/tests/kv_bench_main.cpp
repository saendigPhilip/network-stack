#include <atomic>
#include <ctime>
#include <cstdio>
#include <iostream>
#include <memory>
#include <thread>
#include "client_server_common.h"
#include "kv_bench.h"
#include "untrusted_kv.h"
#include "test_common.h"

std::atomic_size_t executed_ops{0};
constexpr size_t kTHREADS{4};
std::atomic_size_t countdown{kTHREADS};


struct timespec time_begin, time_end;

void test_thread() {
    std::unique_ptr<unsigned char> value_buf =
        std::make_unique<unsigned char>(VAL_SIZE);

    if (--countdown == 0) {
        puts("Starting time measurement");
        clock_gettime(CLOCK_MONOTONIC, &time_begin);
    }
    else {
        while(countdown > 0);
    }

    for (size_t i = executed_ops; i < total_ops; i = executed_ops++) {
        // if (i % 100 == 0) {
        //      printf("%ld\n", i);
        // }
        uint64_t key = wl_params.keys[i];
        anchor_tr_::Trace_cmd::operation op = wl_params.op[i];
        size_t len;
        switch (op) {
            case anchor_tr_::Trace_cmd::Get:
                (void)kv_get((void *)&key, key_size, &len);
                break;
            case anchor_tr_::Trace_cmd::Put:
                (void)kv_put((void *)&key, key_size, value_buf.get(), VAL_SIZE);
                break;
            default:
                break;
        }
    }
}


int main(int argc, const char *argv[]) {
    /* We have all start times in an array and pass the pointers
     * to the put/get functions as a tag that is returned in the callback
     */
    int ret = 0;

    if (0 != global_params.parse_args(argc - 1, argv + 1)) {
        global_test_params::print_options();
        exit(1);
    }

    printf("Value size: %zu\n", VAL_SIZE);
    initialize_kv_store();
    enc_key = (unsigned char *) "0123456789ABCDEF";

    if (workload_scan() != 0) {
            perror("workload scan failed");
            return -1;
    }

    std::vector<std::thread> threads;

    for (size_t i = 0; i < kTHREADS - 1; i++) {
        threads.emplace_back(std::thread(test_thread));
    }
    test_thread();

    for (auto &thread : threads)
        thread.join();

    clock_gettime(CLOCK_MONOTONIC, &time_end);
    puts("Ended time measurement");

    uint64_t total_time = time_diff(&time_begin, &time_end);

    printf("Number of operations: %zu, Total time: %lu, kOps/s: %f\n",
                static_cast<size_t>(executed_ops), total_time,
                static_cast<double>(executed_ops * 1'000'000) /
                static_cast<double>(total_time));

    release_workload();

    return ret;
}
