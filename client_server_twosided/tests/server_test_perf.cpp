#include <atomic>
#include <cstring>
#include <cstdlib>
#include <map>
#include <vector>
#include <unistd.h>

#include "client_server_common.h"
#include "Server.h"
#include "test_common.h"

#if NO_KV_OVERHEAD
thread_local unsigned char *default_value;
#else
#include "untrusted_kv.h"

#endif // NO_KV_OVERHEAD

#if MEASURE_THROUGHPUT
std::atomic_size_t request_count{0};
thread_local struct timespec start;
#endif // MEASURE_THROUGHPUT

#if NO_KV_OVERHEAD
const void *kv_get(const void *, size_t, size_t *data_len) {
    if (!default_value)
        default_value = static_cast<unsigned char *>(malloc(VAL_SIZE));
    *data_len = VAL_SIZE;
    return default_value;
}

int kv_put(const void *, size_t, void *, size_t) {
#if MEASURE_THROUGHPUT
    ++request_count;
#endif // MEASURE_THROUGHPUT
    return 0;
}

int kv_delete(const void *, size_t) {
    return 0;
}

#endif // NO_KV_OVERHEAD



void print_usage(const char *argv0) {
    cout << "Usage: " << argv0 << " <ip-address>" << endl;
    global_test_params::print_options();
}


int main(int argc, const char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    struct timespec times[2];

    std::string ip(argv[1]);
    int ret = 1;
    const uint16_t standard_udp_port = 31850;
    if (0 != anchor_server::init(ip, standard_udp_port))
        return 1;

    if (0 > global_params.parse_args(argc - 2, argv + 2)) {
        print_usage(argv[0]);
        return 1;
    }

#if NO_KV_OVERHEAD
#else
    initialize_kv_store();
#endif // NO_KV_OVERHEAD

    if (anchor_server::host_server(
            key_do_not_use, NUM_CLIENTS,
            KEY_SIZE + VAL_SIZE,
#if MEASURE_THROUGHPUT
            true,
#else
            false,
#endif
            kv_get, kv_put, kv_delete)) {
        cerr << "Failed to host server" << endl;
        return ret;
    }

#if MEASURE_THROUGHPUT
    std::vector<double> results;
    results.reserve(128);
    bool running = true;
    size_t requests_old = 0;
    clock_gettime(CLOCK_MONOTONIC, times + 1);
    for (int i = 0; running; i ^= 1) {
        sleep(10);
        size_t requests_interval = request_count - requests_old;
        requests_old += requests_interval;
        if (requests_old > 0) {
            clock_gettime(CLOCK_MONOTONIC, times + i);
            uint64_t interval_time = time_diff(times + (i ^ 1), times + i);
            double throughput =
                static_cast<double>(8 * requests_interval
                                    * (KEY_SIZE + VAL_SIZE)) /
                static_cast<double>(interval_time);

            printf("Requests: %zu, Time: %lu ns, Throughput: %f Gbit/s\n",
                requests_interval, interval_time, throughput);
            results.push_back(throughput);

            // Stop if there are no requests anymore
            running = request_count == 0 || requests_interval > 0;
        }
    }

    // Take the average of the data from the third until the third last
    // Throughput measurement
    double total_throughput = 0.0;
    for (size_t i = 2; i < results.size() - 2; i++) {
        total_throughput += results[i];
    }
    size_t result_count = results.size() - 4;
    total_throughput /= static_cast<double>(result_count);
    printf("Final result (taken from %zu data points): %f Gbit/s\n\n",
        result_count, total_throughput);

#endif

    anchor_server::close_connection(false);

#if NO_KV_OVERHEAD
    free(default_value);
#else
    cleanup_kv_store();
#endif

    anchor_server::terminate();
    return 0;
}
