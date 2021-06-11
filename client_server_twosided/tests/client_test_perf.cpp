#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <clocale>
#include <random>
#include "Client.h"
#include "test_common.h"

uint16_t port = 31850;

thread_local unsigned char *value_buf;
thread_local unsigned char *key_buf;
thread_local struct test_params *local_params;
thread_local struct test_results *local_results;

struct test_params {
    uint16_t port;
    uint8_t id;
    size_t put_requests;
    size_t get_requests;
    size_t del_requests;
};

struct test_results {
    size_t successful_puts;
    size_t failed_puts;
    uint64_t put_time;
    size_t successful_gets;
    size_t failed_gets;
    uint64_t get_time;
    size_t successful_deletes;
    size_t failed_deletes;
    uint64_t delete_time;
    size_t timeouts;
    size_t invalid_responses;
    uint64_t time_all;
};


inline uint64_t time_diff(
        const struct timespec *start, const struct timespec *end){
    auto diff_sec = static_cast<uint64_t>(end->tv_sec - start->tv_sec);
    return diff_sec * 1'000'000'000ull + static_cast<uint64_t>(end->tv_nsec)
        - static_cast<uint64_t>(start->tv_nsec);
}


void evaluate_failed_op(ret_val status) {
    if (likely(status == TIMEOUT))
        // fprintf(stderr, "Thread %u: Timeout\n", local_params->id);
        local_results->timeouts++;
    else
        // fprintf(stderr, "Thread %u: Invalid resp.\n", local_params->id);
        local_results->invalid_responses++;
}

void put_callback(ret_val status, const void *tag) {
#if MEASURE_LATENCY
    struct timespec end_time;
    (void) clock_gettime(CLOCK_MONOTONIC, &end_time);
    const struct timespec *start_time = (struct timespec *) tag;
    uint64_t diff = time_diff(start_time, &end_time);
#endif // MEASURE_LATENCY

    if (status == OP_SUCCESS || status == OP_FAILED) {
        if (status == OP_SUCCESS)
            local_results->successful_puts++;
        else
            local_results->failed_puts++;

#if MEASURE_LATENCY
        local_results->put_time += diff;
#endif
    }
    else {
        evaluate_failed_op(status);
    }
}

void get_callback(ret_val status, const void *tag) {
#if MEASURE_LATENCY
    struct timespec end_time;
    (void) clock_gettime(CLOCK_MONOTONIC, &end_time);
    const struct timespec *start_time = (struct timespec *) tag;
    uint64_t diff = time_diff(start_time, &end_time);
#endif // MEASURE_LATENCY

    if (status == OP_SUCCESS || status == OP_FAILED) {
        if (OP_SUCCESS == status)
            local_results->successful_gets++;
        else
            local_results->failed_gets++;

#if MEASURE_LATENCY
        local_results->get_time += diff;
#endif
    }
    else {
        evaluate_failed_op(status);
    }
}

void del_callback(ret_val status, const void *tag) {
#if MEASURE_LATENCY
    struct timespec end_time;
    (void) clock_gettime(CLOCK_MONOTONIC, &end_time);
    const struct timespec *start_time = (struct timespec *) tag;
    uint64_t diff = time_diff(start_time, &end_time);
#endif // MEASURE_LATENCY

    if (status == OP_SUCCESS || status == OP_FAILED) {
        if (status == OP_SUCCESS)
            local_results->successful_deletes++;
        else
            local_results->failed_deletes++;

#if MEASURE_LATENCY
        local_results->delete_time += diff;
#endif

    }
    else {
        evaluate_failed_op(status);
    }
}

inline void get_random_key() {
    auto *dst = (int32_t *) key_buf;
    // We don't care about the last bytes if key size is not dividable by 4
    for (size_t i = 0; i < KEY_SIZE / 4; i++)
        dst[i] = rand();
}

void issue_requests(Client *client) {
    /* We have all start times in an array and pass the pointers
     * to the put/get functions as a tag that is returned in the callback
     */
#if MEASURE_LATENCY
    struct timespec times[MAX_ACCEPTED_RESPONSES];
#endif

    size_t gets_performed = 0, puts_performed = 0, deletes_performed = 0;
    for (size_t req = 0; ; req++) {
        get_random_key();
#if MEASURE_LATENCY
        struct timespec *time_now =
                times + (req % MAX_ACCEPTED_RESPONSES);
#else
        struct timespec *time_now = nullptr;
#endif
        // Go easy on the server:
        if (client->queue_full()) {
            std::this_thread::sleep_for(chrono::microseconds(CLIENT_TIMEOUT));
            client->run_event_loop_n_times(LOOP_ITERATIONS);
        }

        if (puts_performed < local_params->put_requests) {
            value_from_key(
                    (void *) value_buf, VAL_SIZE, (void *) key_buf, KEY_SIZE);
#if MEASURE_LATENCY
            (void) clock_gettime(CLOCK_MONOTONIC, time_now);
#endif
            if (0 > client->put((void *) key_buf, KEY_SIZE,
                    (void *) value_buf, VAL_SIZE,
                    put_callback, time_now, LOOP_ITERATIONS)) {

                cerr << "put() failed" << endl;
            } else
                puts_performed++;
        }
        else if (gets_performed < local_params->get_requests) {
#if MEASURE_LATENCY
            (void) clock_gettime(CLOCK_MONOTONIC, time_now);
#endif
            if (0 > client->get((void *) key_buf, KEY_SIZE,
                    (void *) value_buf, nullptr,
                    get_callback, time_now, LOOP_ITERATIONS)) {
                cerr << "get() failed" << endl;
            } else
                gets_performed++;
        }
        else if (deletes_performed < local_params->del_requests) {
#if MEASURE_LATENCY
            (void) clock_gettime(CLOCK_MONOTONIC, time_now);
#endif
            if (0 > client->del((void *) key_buf, KEY_SIZE,
                del_callback, time_now, LOOP_ITERATIONS)) {
                cerr << "delete() failed" << endl;
            } else
                deletes_performed++;
        }
        else
            break;
    }
    (void) client->disconnect();
}


void test_thread(struct test_params *params, struct test_results *results,
        std::string *server_hostname) {

    Client client{params->id, KEY_SIZE, VAL_SIZE};
    key_buf = static_cast<unsigned char *>(malloc(KEY_SIZE));
    value_buf = static_cast<unsigned char *>(malloc(VAL_SIZE));
    if (!(key_buf && value_buf))
        goto end_test_thread;

    local_params = params;
    local_results = results;
    if (0 > client.connect(
            *server_hostname, params->port, key_do_not_use)) {
        cerr << "Thread " << params->id
                << ": Failed to connect to server" << endl;
        return;
    }
    srand(static_cast<unsigned int>(params->id));

    issue_requests(&client);

end_test_thread:
    free(key_buf);
    free(value_buf);
}

void fill_params_structs(struct test_params *params) {
    struct test_params default_params = {
            port,
            0,
            PUTS_PER_CLIENT,
            GETS_PER_CLIENT,
            DELS_PER_CLIENT
    };
    for (uint8_t id = 0; id < NUM_CLIENTS; id++) {
        memcpy(static_cast<void *>(params + id), &default_params,
            sizeof(struct test_params));
        params[id].id = id;
    }
}

void add_result_to_final(
        struct test_results *final, struct test_results *result) {
    final->successful_puts += result->successful_puts;
    final->failed_puts += result->failed_puts;
    final->put_time += result->put_time;
    final->successful_gets += result->successful_gets;
    final->failed_gets += result->failed_gets;
    final->get_time += result->get_time;
    final->successful_deletes += result->successful_deletes;
    final->failed_deletes += result->failed_deletes;
    final->delete_time += result->delete_time;
    final->timeouts += result->timeouts;
    final->invalid_responses += result->invalid_responses;
}

void print_summary(bool all, struct test_params *params, 
        struct test_results *result) {

    setlocale(LC_ALL, "");
    size_t total_puts = params->put_requests;
    size_t total_gets = params->get_requests;
    size_t total_dels = params->del_requests;
    size_t suc_puts = result->successful_puts;
    size_t valid_puts = suc_puts + result->failed_puts;
    uint64_t put_time = result->put_time;
    size_t suc_gets = result->successful_gets;
    size_t valid_gets = suc_gets + result->failed_gets;
    uint64_t get_time = result->get_time;
    size_t suc_dels = result->successful_deletes;
    size_t valid_dels = suc_dels + result->failed_deletes;
    uint64_t del_time = result->delete_time;

    if (all) {
        printf("\n\n--------------------Summary--------------------\n\n");
        printf("Key size: %lu, Value size: %lu\n", KEY_SIZE, VAL_SIZE);
        printf("Number of Clients/Threads: %u\n"
               "Number of client loop iterations: %zu\n\n\n",
                NUM_CLIENTS, LOOP_ITERATIONS);
    }
    else 
        printf("Thread %2i: \n", params->id);

    double buf = static_cast<double>(put_time) / static_cast<double>(valid_puts);
    printf("put:     %'zu/%'zu valid responses, %'zu of it failed\n"
           "Total time: %'lu ns, time/put: %f ns (%f us)\n\n",
            valid_puts, total_puts, result->failed_puts,
            put_time, buf, buf / 1000.0);

    buf = static_cast<double>(get_time) / static_cast<double>(valid_gets);
    printf("get:     %'zu/%'zu valid responses, %'zu of it failed\n"
           "Total time: %'lu ns, time/get: %f ns (%f us)\n\n",
            valid_gets, total_gets, result->failed_gets,
            get_time, buf, buf / 1000.0);

    buf = static_cast<double>(del_time) / static_cast<double>(valid_dels);
    printf("delete():  %'zu/%'zu valid responses, %'zu of it failed\n"
           "Total time: %'lu ns, time/delete: %f ns (%f us)\n\n",
            valid_dels, total_dels, result->failed_deletes,
            del_time, buf, buf / 1000.0);


    uint64_t total_time = put_time + get_time + del_time;
    size_t valid_total = valid_puts + valid_gets + valid_dels;
    printf("Total time: %'lu ns, time/operation: %f ns\n", total_time,
            static_cast<double>(total_time) / static_cast<double>(valid_total));
    printf("Timeouts: %'lu, Invalid responses: %'lu\n\n\n",
            result->timeouts, result->invalid_responses);

    if (all) {
        printf("Total time needed for tests: %f s\n",
            static_cast<double>(result->time_all) / 1'000'000'000);
        printf("Time per valid operation in total: %f\n\n",
                static_cast<double>(result->time_all) /
                static_cast<double>(valid_total));
        size_t uplink_volume =
                (total_puts + total_gets + total_dels) * KEY_SIZE +
                total_puts * VAL_SIZE;
        size_t downlink_volume =
                (valid_puts + valid_gets + valid_dels) * MIN_MSG_LEN
                + valid_gets * VAL_SIZE;

        buf = static_cast<double>(1000 * uplink_volume) /
            static_cast<double>(result->time_all);
            printf("Total uplink speed:    %f MB/s (%f Mbit/s)\n",
            buf, buf * 8);

        buf = static_cast<double>(1000 * downlink_volume) /
            static_cast<double>(result->time_all);
        printf("Total downlink speed:  %f MB/s (%f Mbit/s)\n",
            buf, buf * 8);

    }
    puts("\n");
}

void perform_tests(string& server_hostname) {

    struct test_params total_params = {
            port,
            0,
            PUTS_PER_CLIENT * NUM_CLIENTS,
            GETS_PER_CLIENT * NUM_CLIENTS,
            DELS_PER_CLIENT * NUM_CLIENTS
    };
    auto params = static_cast<struct test_params *>(
            malloc(NUM_CLIENTS * sizeof(struct test_params)));

    size_t results_size = NUM_CLIENTS * sizeof(struct test_results);
    auto results = static_cast<struct test_results *>(
            malloc(results_size));

    assert(params != nullptr && results != nullptr);
    fill_params_structs(params);
    memset(results, 0, results_size);

    vector<thread *> threads;
    /* Launch requester threads: */
    struct timespec total_time_begin, total_time_end;
    (void) clock_gettime(CLOCK_MONOTONIC, &total_time_begin);
    uint8_t i = 0;
    for (; i < NUM_CLIENTS - 1; i++) {
        threads.emplace_back(new thread(test_thread, params + i, results + i,
                &server_hostname));
    }
    test_thread(params + i, results + i, &server_hostname);


    /* Wait for requester threads to finish: */
    for (i = 0; i < NUM_CLIENTS - 1; i++) {
        threads[i]->join();
        delete threads[i];
    }
    (void) clock_gettime(CLOCK_MONOTONIC, &total_time_end);

    struct test_results final_results;
    memset(&final_results, 0, sizeof(final_results));
    struct test_results *result;
    for (i = 0; i < NUM_CLIENTS; i++) {
        result = results + i;
        // print_summary(false, param, result);
        add_result_to_final(&final_results, result);
    }
    final_results.time_all = time_diff(&total_time_begin, &total_time_end);
    print_summary(true, &total_params, &final_results);
    free(params);
    free(results);
}

void print_usage(const char *arg0) {
    cout << "Usage: " << arg0 <<
         " <client ip-address> <server ip-address>" << endl;
    global_test_params::print_options();
}

int main(int argc, const char *argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    string client_hostname(argv[1]);
    string server_hostname(argv[2]);
    Client::init(client_hostname, port);

    global_params.parse_args(argc - 3, argv + 3);

    perform_tests(server_hostname);
    // Wait for the server to prepare:
    this_thread::sleep_for(2s);
    Client::terminate();
    return 0;
}
