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
std::map<std::vector<unsigned char>, unsigned char *> test_kv_store;
std::atomic_flag kv_flag{false};

void lock_kv() {
    while (kv_flag.test_and_set())
        ;
}

void unlock_kv() {
    kv_flag.clear();
}

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

#else // NO_KV_OVERHEAD

const void *kv_get(const void *key, size_t key_size, size_t *data_len) {
    void *ret = nullptr;
    auto *key_uc = static_cast<const unsigned char *>(key);
    auto vec = std::vector<unsigned char>();
    vec.assign(key_uc, key_uc + key_size);

    lock_kv();
    auto iter = test_kv_store.find(vec);
    if (iter != test_kv_store.end()) {
        ret = iter->second;
        *data_len = VAL_SIZE;
    }
    else
        *data_len = 0;
    unlock_kv();

    return ret;
}


int kv_put(const void *key, size_t key_size, void *value, size_t value_size) {
    int ret = -1;
    auto *key_uc = static_cast<const unsigned char *>(key);
    auto vec = std::vector<unsigned char>();
    vec.assign(key_uc, key_uc + key_size);
    auto iter = test_kv_store.find(vec);
    unsigned char *val;

    lock_kv();
    if (iter == test_kv_store.end()) {
        val = static_cast<unsigned char *>(malloc(VAL_SIZE));
        if (!val)
            goto end_kv_put;
        test_kv_store.insert({vec, val});
    }
    else {
        val = iter->second;
    }
    memcpy(val, value, value_size);

    ret = 0;
end_kv_put:
    unlock_kv();
    return ret;
}

int kv_delete(const void *key, size_t key_size) {
    int ret = -1;
    auto *key_uc = static_cast<const unsigned char *>(key);
    auto vec = std::vector<unsigned char>();
    vec.assign(key_uc, key_uc + key_size);
    lock_kv();
    auto iter = test_kv_store.find(vec);
    if (iter != test_kv_store.end()) {
        test_kv_store.erase(iter);
        free(iter->second);
        ret = 0;
    }
    unlock_kv();
    return ret;
}
#endif // NO_KV_OVERHEAD


/* Fills the KV-store with initial data by calling value_from_key()
 * for each entry
 */
int initialize_kv_store() {
#if NO_KV_OVERHEAD
#else
    unlock_kv();
#endif // NO_KV_OVERHEAD
    return 0;
}

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

    if (0 > initialize_kv_store()) {
        cerr << "Failed initializing KV-store. Aborting" << endl;
        return 1;
    }

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
    for (auto& iter : test_kv_store) {
        free(iter.second);
        test_kv_store.erase(iter.first);
    }
#endif

    anchor_server::terminate();
    return 0;
}
