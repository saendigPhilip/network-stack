#include <chrono>
#include <cstring>
#include <ctime>
#include <random>
#include "Client.h"
#include "test_common.h"

char test_value[TEST_MAX_VAL_SIZE];
char incoming_test_value[TEST_MAX_VAL_SIZE];

thread_local unsigned char value_buf[TEST_MAX_VAL_SIZE];
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
    uint64_t put_time;
    size_t successful_gets;
    uint64_t get_time;
    size_t successful_deletes;
    uint64_t delete_time;
    size_t failed_operations;
    size_t timeouts;
    size_t invalid_responses;
};


inline uint64_t time_diff(
        const struct timespec *start, const struct timespec *end){
    auto diff_sec = static_cast<uint64_t>(end->tv_sec - start->tv_sec);
    return diff_sec * 1'000'000'000ull + static_cast<uint64_t>(end->tv_nsec)
        - static_cast<uint64_t>(start->tv_nsec);
}


void evaluate_status(anchor_client::ret_val status) {
    switch(status) {
        case anchor_client::OP_FAILED:
            local_results->failed_operations++;
            break;
        case anchor_client::TIMEOUT:
            local_results->timeouts++;
            break;
        case anchor_client::INVALID_RESPONSE:
            local_results->invalid_responses++;
            break;
        default:
            break;
    }
}

void put_callback(anchor_client::ret_val status, const void *tag) {
    struct timespec end_time;
    (void) clock_gettime(CLOCK_MONOTONIC, &end_time);
    const struct timespec *start_time = (struct timespec *) tag;
    uint64_t diff = time_diff(start_time, &end_time);

    if (status == anchor_client::OP_SUCCESS) {
        local_results->successful_puts++;
        local_results->put_time += diff;
    }
    else
        evaluate_status(status);
}

void get_callback(anchor_client::ret_val status, const void *tag) {
    struct timespec end_time;
    (void) clock_gettime(CLOCK_MONOTONIC, &end_time);
    const struct timespec *start_time = (struct timespec *) tag;
    uint64_t diff = time_diff(start_time, &end_time);

    if (anchor_client::OP_SUCCESS == status) {
        local_results->successful_gets++;
        local_results->get_time += diff;
    }
    else
        evaluate_status(status);
}

void del_callback(anchor_client::ret_val status, const void *tag) {
    struct timespec end_time;
    (void) clock_gettime(CLOCK_MONOTONIC, &end_time);
    const struct timespec *start_time = (struct timespec *) tag;
    uint64_t diff = time_diff(start_time, &end_time);

    if (status == anchor_client::OP_SUCCESS) {
        local_results->successful_deletes++;
        local_results->delete_time += diff;
    }
}

inline size_t get_random_key() {
    return static_cast<size_t>(rand()) % TEST_KV_SIZE;
}

void issue_requests(anchor_client::Client *client) {
    /* We have all start times in an array and pass the pointers
     * to the put/get functions as a tag that is returned in the callback
     */
    struct timespec times[anchor_client::MAX_ACCEPTED_RESPONSES];
    size_t gets_performed = 0, puts_performed = 0, deletes_performed = 0;
    size_t key;
    for (size_t req = 0; ; req++) {
        key = get_random_key();
        struct timespec *time_now =
                times + (req % anchor_client::MAX_ACCEPTED_RESPONSES);

        // Go easy on the server:
        if (client->queue_full()) {
            cerr << "Queue full. Waiting for server" << endl;
            std::this_thread::sleep_for(5ms);
            client->run_event_loop_n_times(20);
        }

        if (puts_performed < local_params->put_requests) {
            value_from_key((void *) value_buf, (void *) &key, sizeof(size_t));
            (void) clock_gettime(CLOCK_MONOTONIC, time_now);
            if (0 > client->put((void *) &key, sizeof(size_t),
                    (void *) value_buf, TEST_MAX_VAL_SIZE,
                    put_callback, time_now)) {

                cerr << "put() failed" << endl;
            } else
                puts_performed++;
        }
        else if (gets_performed < local_params->get_requests) {
            (void) clock_gettime(CLOCK_MONOTONIC, time_now);
            if (0 > client->get((void *) &key, sizeof(size_t),
                    (void *) value_buf, TEST_MAX_VAL_SIZE, nullptr,
                    get_callback, time_now)) {
                cerr << "get() failed" << endl;
            } else
                gets_performed++;
        }
        else if (deletes_performed < local_params->del_requests) {
            (void) clock_gettime(CLOCK_MONOTONIC, time_now);
            if (0 > client->del((void *) key,
                    sizeof(size_t), del_callback, time_now)) {
                cerr << "delete() failed" << endl;
            } else
                deletes_performed++;
        }
        else
            break;
    }
}

void test_thread(struct test_params *params, struct test_results *results,
        anchor_client::Client *client, std::string *server_hostname) {

    local_params = params;
    local_results = results;
    if (0 > client->connect(
            *server_hostname, params->port, key_do_not_use)) {
        cerr << "Thread " << params->id
                << ": Failed to connect to server" << endl;
        return;
    }
    srand(static_cast<unsigned int>(params->id));

    issue_requests(client);
    (void) client->disconnect();
}

void fill_params_structs(
        struct test_params *params, uint16_t port) {

    struct test_params default_params = {
            port,
            0,
            PUT_REQUESTS_PER_CLIENT,
            GET_REQUESTS_PER_CLIENT,
            DELETE_REQUESTS_PER_CLIENT
    };
    for (uint8_t id = 0; id < TOTAL_CLIENTS; id++) {
        memcpy(static_cast<void *>(params + id), &default_params,
            sizeof(struct test_params));
        params[id].id = id;
    }
}

void add_result_to_final(
        struct test_results *final, struct test_results *result) {
    final->successful_puts += result->successful_puts;
    final->put_time += result->put_time;
    final->successful_gets += result->successful_gets;
    final->get_time += result->get_time;
    final->successful_deletes += result->successful_deletes;
    final->delete_time += result->delete_time;
    final->failed_operations += result->failed_operations;
    final->timeouts += result->timeouts;
    final->invalid_responses += result->invalid_responses;
}

void print_summary(bool all, struct test_params *params, 
        struct test_results *result) {

    size_t total_puts = params->put_requests;
    size_t total_gets = params->get_requests;
    size_t total_dels = params->del_requests;
    size_t suc_puts = result->successful_puts;
    uint64_t put_time = result->put_time;
    size_t suc_gets = result->successful_gets;
    uint64_t get_time = result->get_time;
    size_t suc_dels = result->successful_deletes;
    uint64_t del_time = result->delete_time;

    if (all)
        printf("\n\n--------------------Summary--------------------\n\n\n");
    else 
        printf("Thread %2i: \n", params->id);

    printf("put(): %zu/%zu successful, Total time: %lu, time/put: %f\n",
            suc_puts, total_puts, put_time,
            static_cast<double>(put_time) / static_cast<double>(suc_puts));
    printf("get(): %zu/%zu successful, Total time: %lu, time/get: %f\n",
            suc_gets, total_gets, get_time,
            static_cast<double>(get_time) / static_cast<double>(suc_gets));
    printf("delete(): %zu/%zu successful, Total time: %lu, time/delete: %f\n",
            suc_dels, total_dels, del_time,
            static_cast<double>(del_time) / static_cast<double>(suc_dels));

    uint64_t total_time = put_time + get_time + del_time;
    size_t suc_total = suc_puts + suc_gets + suc_dels;
    printf("Total time: %lu, time/operation: %f\n", total_time,
            static_cast<double>(total_time) / static_cast<double>(suc_total));
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] <<
            " <client ip-address> <server ip-address>" << endl;
        return -1;
    }
    string client_hostname(argv[1]);
    string server_hostname(argv[2]);
    uint16_t port = 31850;
    struct test_params total_params = {
            port,
            0,
            PUT_REQUESTS_PER_CLIENT * TOTAL_CLIENTS,
            GET_REQUESTS_PER_CLIENT * TOTAL_CLIENTS,
            DELETE_REQUESTS_PER_CLIENT * TOTAL_CLIENTS
    };
    struct test_params params[TOTAL_CLIENTS];
    struct test_results results[TOTAL_CLIENTS];
    fill_params_structs(params, port);
    memset(results, 0, sizeof(results));

    vector<anchor_client::Client *> clients;
    for (uint8_t id = 0; id < TOTAL_CLIENTS; id++) {
        clients.emplace_back(
                new anchor_client::Client(client_hostname, port, id));
    }

    vector<thread *> threads;
    /* Launch requester threads: */
    uint8_t i = 0;
    for (; i < TOTAL_CLIENTS - 1; i++) {
        threads.emplace_back(new thread(test_thread, params + i, results + i,
                clients[i], &server_hostname));
    }
    test_thread(params + i, results + i, clients[i], &server_hostname);


    /* Wait for requester threads to finish: */
    struct test_results final;
    memset(&final, 0, sizeof(final));
    struct test_params *param;
    struct test_results *result;
    for (i = 0; i < TOTAL_CLIENTS; i++) {
        if (i < TOTAL_CLIENTS - 1) {
            threads[i]->join();
            delete threads[i];
        }
    }

    for (i = 0; i < TOTAL_CLIENTS; i++) {
        param = params + i;
        result = results + i;
        print_summary(false, param, result);
        add_result_to_final(&final, result);
    }
    print_summary(true, &total_params, &final);
}
