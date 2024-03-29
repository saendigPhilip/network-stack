#include <atomic>
#include <cstdlib>
#include <cstring>
#include "Client.h"
#include "test_common.h"

#if NO_KV_OVERHEAD
std::atomic_size_t global_puts{0}, global_gets{0}, global_dels{0};

#else
#include "kv_bench.h"

std::atomic_size_t executed_ops{0};
#endif // NO_KV_OVERHEAD

std::atomic_uint8_t countdown;
uint16_t port = 31850;
struct timespec total_time_begin, total_time_end;

thread_local unsigned char *value_buf;
thread_local unsigned char *key_buf;
thread_local struct test_results *local_results;

struct test_params {
    uint16_t port;
    uint8_t id;
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



void evaluate_failed_op(ret_val status) {
    if (likely(status == TIMEOUT))
        local_results->timeouts++;
    else
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
#if NO_KV_OVERHEAD

#else
    auto *dst = (uint32_t *) key_buf;
    if (MAX_KEY) {
        dst[0] = static_cast<uint32_t>(rand()) % MAX_KEY;
    }
    else {
        // We don't care about the last bytes if key size is not dividable by 4
        for (size_t i = 1; i < KEY_SIZE / 4; i++)
            dst[i] = static_cast<uint32_t>(rand());
    }
#endif // NO_KV_OVERHEAD
}

#if NO_KV_OVERHEAD
void issue_requests(Client *client) {
    /* We have all start times in an array and pass the pointers
     * to the put/get functions as a tag that is returned in the callback
     */
#if MEASURE_LATENCY
    struct timespec times[MAX_ACCEPTED_RESPONSES];
#endif

    size_t puts_inc = TOTAL_PUTS;
    size_t gets_inc = TOTAL_GETS;
    size_t dels_inc = TOTAL_DELS;

    struct timespec begin_time;
    struct timespec end_time;
    (void) clock_gettime(CLOCK_MONOTONIC, &begin_time);

    for (size_t req = 0; ; req++) {
        get_random_key();
#if MEASURE_LATENCY
        struct timespec *time_now =
                times + (req % MAX_ACCEPTED_RESPONSES);
#else
        struct timespec *time_now = nullptr;
#endif
        if (++global_puts <= TOTAL_PUTS) {
#if MEASURE_LATENCY
            (void) clock_gettime(CLOCK_MONOTONIC, time_now);
#endif
            if (0 > client->put((void *) key_buf, KEY_SIZE,
                    (void *) value_buf, VAL_SIZE,
                    put_callback, time_now, LOOP_ITERATIONS /* *
                    (VAL_SIZE < 2048 ? 1 : VAL_SIZE / 1024)*/)) {

                cerr << "put() failed" << endl;
            }
        }
        else if (++global_gets <= TOTAL_GETS) {
#if MEASURE_LATENCY
            (void) clock_gettime(CLOCK_MONOTONIC, time_now);
#endif
            if (0 > client->get((void *) key_buf, KEY_SIZE,
                    (void *) value_buf, nullptr,
                    get_callback, time_now,
                    LOOP_ITERATIONS /* *
                        (VAL_SIZE < 2048 ? 1 : VAL_SIZE / 1024)*/)) {
                cerr << "get() failed" << endl;
            }
        }
        else if (++global_dels < TOTAL_DELS) {
#if MEASURE_LATENCY
            (void) clock_gettime(CLOCK_MONOTONIC, time_now);
#endif
            if (0 > client->del((void *) key_buf, KEY_SIZE,
                del_callback, time_now, LOOP_ITERATIONS)) {
                cerr << "delete() failed" << endl;
            }
        }
        else {
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            if (time_diff(&begin_time, &end_time) < MIN_TIME) {
                TOTAL_PUTS += puts_inc;
                TOTAL_GETS += gets_inc;
                TOTAL_DELS += dels_inc;
            }
            else
                break;
        }
    }

    size_t total_ops = global_puts + global_gets + global_dels;

    client->prepare_disconnect();
    // Run event loop if there are requests without responses
    for (size_t i = 0; i < 100'000 && (
        local_results->failed_puts + local_results->successful_puts
    + local_results->failed_gets + local_results->successful_gets
    + local_results->failed_deletes + local_results->successful_deletes
    + local_results->timeouts + local_results->invalid_responses < total_ops);
    i++) {
        client->run_event_loop_n_times(LOOP_ITERATIONS);
    }

    (void) client->disconnect();
#if MEASURE_LATENCY
#else
    (void) clock_gettime(CLOCK_MONOTONIC, &end_time);
    local_results->time_all = time_diff(&begin_time, &end_time);
#endif // MEASURE_LATENCY
}

#else // NO_KV_OVERHEAD


void issue_requests(Client *client) {
    /* We have all start times in an array and pass the pointers
     * to the put/get functions as a tag that is returned in the callback
     */

    for (size_t i = executed_ops; i < total_ops; i = executed_ops++) {
        // if (i % 100 == 0) {
        //      printf("%ld\n", i);
        // }
        uint64_t key = wl_params.keys[i];
        anchor_tr_::Trace_cmd::operation op = wl_params.op[i];
        switch (op) {
            case anchor_tr_::Trace_cmd::Get:
                if (0 > client->get((void *) &key, KEY_SIZE,
                    (void *) value_buf, nullptr,
                    get_callback, nullptr,
                    LOOP_ITERATIONS)) {
                    cerr << "get() failed" << endl;
                }
                break;
            case anchor_tr_::Trace_cmd::Put:
                if (0 > client->put((void *) &key, KEY_SIZE,
                    (void *) value_buf, VAL_SIZE,
                    put_callback, nullptr, LOOP_ITERATIONS)) {
                    cerr << "put() failed" << endl;
                }
                break;
            default:
                break;
        }
    }

    client->prepare_disconnect();
    // Run event loop if there are requests without responses
    for (size_t i = 0; i < 100'000 && (
        local_results->failed_puts + local_results->successful_puts
    + local_results->failed_gets + local_results->successful_gets
    + local_results->failed_deletes + local_results->successful_deletes
    + local_results->timeouts + local_results->invalid_responses < total_ops);
    i++) {
        client->run_event_loop_n_times(LOOP_ITERATIONS);
    }

    (void) client->disconnect();
}


#endif // NO_KV_OVERHEAD


void test_thread(struct test_params *params, struct test_results *results,
        std::string *server_hostname) {

    key_buf = static_cast<unsigned char *>(calloc(1, KEY_SIZE));
    value_buf = static_cast<unsigned char *>(malloc(VAL_SIZE));
    {
        Client client{params->id, KEY_SIZE, VAL_SIZE};
        if (!(key_buf && value_buf))
            goto end_test_thread;

        local_results = results;
        if (0 > client.connect(
            *server_hostname, params->port, key_do_not_use)) {
            cerr << "Thread " << params->id
                 << ": Failed to connect to server" << endl;
            return;
        }
        srand(static_cast<unsigned int>(params->id));

        if (--countdown == 0) {
            (void) clock_gettime(CLOCK_MONOTONIC, &total_time_begin);
        } else {
            while (countdown > 0);
        }

        issue_requests(&client);
    }

end_test_thread:
    free(key_buf);
    free(value_buf);
}

void fill_params_structs(struct test_params *params) {
    struct test_params default_params = {
        port,
        0
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
        if (MAX_KEY)
            printf("Maximum possible Key: %lu\n", MAX_KEY);
        printf("Number of Clients/Threads: %u\n"
               "Number of client loop iterations: %zu\n\n\n",
            NUM_CLIENTS, LOOP_ITERATIONS);
    }
    else
        printf("Thread %2i: \n", params->id);

    double buf = static_cast<double>(put_time) / static_cast<double>(valid_puts);
    printf("put:     %'zu/%'zu valid responses, %'zu of it failed\n"
           "Total time: %'lu ns, time/put: %f ns (%f us)\n\n",
        valid_puts, TOTAL_PUTS, result->failed_puts,
        put_time, buf, buf / 1000.0);

    buf = static_cast<double>(get_time) / static_cast<double>(valid_gets);
    printf("get:     %'zu/%'zu valid responses, %'zu of it failed\n"
           "Total time: %'lu ns, time/get: %f ns (%f us)\n\n",
        valid_gets, TOTAL_GETS, result->failed_gets,
        get_time, buf, buf / 1000.0);

    buf = static_cast<double>(del_time) / static_cast<double>(valid_dels);
    printf("delete:  %'zu/%'zu valid responses, %'zu of it failed\n"
           "Total time: %'lu ns, time/delete: %f ns (%f us)\n\n",
        valid_dels, TOTAL_DELS, result->failed_deletes,
        del_time, buf, buf / 1000.0);


    uint64_t total_time = put_time + get_time + del_time;
    size_t valid_total = valid_puts + valid_gets + valid_dels;
    printf("Total time: %'lu ns, time/operation: %f ns\n", total_time,
        static_cast<double>(total_time) / static_cast<double>(valid_total));
    printf("Operations per second: %f\n",
        static_cast<double>(1'000'000 * valid_total) /
        static_cast<double>(result->time_all));
    printf("Timeouts: %'lu, Invalid responses: %'lu\n\n\n",
        result->timeouts, result->invalid_responses);

    if (all) {
        printf("Total time needed for tests: %'lu ns (%f s)\n",
            result->time_all,
            static_cast<double>(result->time_all) / 1'000'000'000);
        printf("Time per valid operation in total: %f ns\n\n",
            static_cast<double>(result->time_all) /
            static_cast<double>(valid_total));
        size_t uplink_volume =
            (TOTAL_PUTS + TOTAL_GETS + TOTAL_DELS) * KEY_SIZE +
            TOTAL_PUTS * VAL_SIZE;
        size_t downlink_volume = suc_gets * VAL_SIZE;

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

bool csv_file_exists() {
    FILE *test = fopen(PATH_CSV, "r");
    if (!test) {
        return false;
    }
    else {
        fclose(test);
        return true;
    }
}

void print_summary_csv(struct test_params *params,
    struct test_results *result) {

    bool file_exists = csv_file_exists();
    FILE *csv = fopen(PATH_CSV, "a");
    if (!csv) {
        fprintf(stderr, "Could not open CSV file at %s\n", PATH_CSV);
        print_summary(true, params, result);
    }

    size_t suc_puts = result->successful_puts;
    size_t valid_puts = suc_puts + result->failed_puts;
    uint64_t put_time = result->put_time;
    size_t suc_gets = result->successful_gets;
    size_t valid_gets = suc_gets + result->failed_gets;
    uint64_t get_time = result->get_time;
    size_t suc_dels = result->successful_deletes;
    size_t valid_dels = suc_dels + result->failed_deletes;
    uint64_t del_time = result->delete_time;

    if (!file_exists) {
        fputs(
            "Threads,Maximum Key,Key Size,Value size,Total Operations,,"
            "Total Puts,Valid Puts,Ratio,Failed Puts,Put Latency(us),"
            "Total Gets,Valid Gets,Ratio,Failed Gets,Get Latency(us),"
            "Total Deletes,Valid Deletes,Ratio,Failed Deletes,Delete Latency(us),,"
            "Total Time (ns),Time/Valid Op (ns),kOps/s,,"
            "Throughput Uplink (Mbit/s),Throughput Downlink (Mbit/s)\n",
            csv);
    }


    size_t total_ops = TOTAL_PUTS + TOTAL_GETS + TOTAL_DELS;
    size_t valid_ops = valid_puts + valid_gets + valid_dels;

    // Threads, Maximum Key, Key Size, Value Size, Total Operations
    fprintf(csv, "%u,%u,%zu,%zu,%zu,,",
        NUM_CLIENTS, MAX_KEY, KEY_SIZE, VAL_SIZE, total_ops);

    double ratio =
        static_cast<double>(valid_puts) / static_cast<double>(TOTAL_PUTS);
    double latency =
        static_cast<double>(put_time) / static_cast<double>(valid_puts * 1000);
    // Total Puts, Valid Puts, Ratio, Failed Puts, Put Latency
    fprintf(csv, "%zu,%zu,%f,%zu,%f,",
        TOTAL_PUTS, valid_puts, ratio, result->failed_puts, latency);

    ratio =
        static_cast<double>(valid_gets) / static_cast<double>(TOTAL_GETS);
    latency =
        static_cast<double>(get_time) / static_cast<double>(valid_gets * 1000);
    // Total Gets, Valid Gets, Ratio, Failed Gets, Get Latency
    fprintf(csv, "%zu,%zu,%f,%zu,%f,",
        TOTAL_GETS, valid_gets, ratio, result->failed_gets, latency);

    ratio =
        static_cast<double>(valid_dels) / static_cast<double>(TOTAL_DELS);
    latency =
        static_cast<double>(del_time) / static_cast<double>(valid_dels * 1000);
    // Total Dels, Valid Dels, Ratio, Failed Dels, Del Latency
    fprintf(csv, "%zu,%zu,%f,%zu,%f,,",
        TOTAL_DELS, valid_dels, ratio, result->failed_deletes, latency);


    // Total Time, Time/Valid Op, kOps/s
    fprintf(csv, "%lu,%f,%f,,",
        result->time_all,
        static_cast<double>(result->time_all) / static_cast<double>(valid_ops),
        1'000'000 * static_cast<double>(valid_ops) /
        static_cast<double>(result->time_all));


    size_t uplink_volume =
        (TOTAL_PUTS + TOTAL_GETS + TOTAL_DELS) * KEY_SIZE +
        TOTAL_PUTS * VAL_SIZE;
    size_t downlink_volume = suc_gets * VAL_SIZE;

    double uplink_speed = static_cast<double>(8000 * uplink_volume) /
        static_cast<double>(result->time_all);
    double downlink_speed = static_cast<double>(8000 * downlink_volume) /
        static_cast<double>(result->time_all);

    // Throughput (Uplink), Throughput (Downlink)
    fprintf(csv, "%f,%f\n",
        uplink_speed, downlink_speed);

    puts("\n");
    fclose(csv);
}


void perform_tests(string& server_hostname) {
#if NO_KV_OVERHEAD
#else
    if (0 != workload_scan()) {
        puts("Failed to scan workload");
        return;
    }
#endif // NO_KV_OVERHEAD


    struct test_params total_params = {
        port,
        0
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
    countdown = NUM_CLIENTS;
    uint8_t i = 0;
    for (; i < NUM_CLIENTS - 1; i++) {
        threads.emplace_back(new thread(test_thread, params + i, results + i,
                &server_hostname));
    }
    test_thread(params + i, results + i, &server_hostname);


    /* Wait for requester threads to finish: */
    for (i = 0; i < NUM_CLIENTS - 1; i++) {
        threads[i]->join();
    }

    (void) clock_gettime(CLOCK_MONOTONIC, &total_time_end);
    std::cout << "Ended time measurement" << std::endl;

    for (i = 0; i < NUM_CLIENTS - 1; i++) {
        delete threads[i];
    }


    struct test_results final_results;
    memset(&final_results, 0, sizeof(final_results));
    struct test_results *result;
    for (i = 0; i < NUM_CLIENTS; i++) {
        result = results + i;
        // print_summary(false, param, result);
        add_result_to_final(&final_results, result);
    }
    final_results.time_all = time_diff(&total_time_begin, &total_time_end);
    if (PATH_CSV)
        print_summary_csv(&total_params, &final_results);
    print_summary(true, &total_params, &final_results);
    free(params);
    free(results);

#if NO_KV_OVERHEAD
#else
    release_workload();
#endif // NO_KV_OVERHEAD
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

    /*
    struct test_params params = {
        80, 0, 1100, 1100, 1120
    };
    struct test_results results = {
        1000ul, 100ul, 1000ul,
        1000ul, 100ul, 1000ul,
        1000ul, 100ul, 1000ul,
        10, 10, 100000ul
    };
    PATH_CSV = "test.csv";
    print_summary_csv(&params, &results);
     */
}
