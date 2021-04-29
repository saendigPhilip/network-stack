#include <cstring>
#include <thread>
#include <ctime>
#include <random>
#include "Client.h"
#include "test_common.h"
#include "simple_unit_test.h"

char test_value[TEST_MAX_VAL_SIZE];
char incoming_test_value[TEST_MAX_VAL_SIZE];

thread_local uint64_t successful_puts = 0, successful_gets = 0;
thread_local unsigned char value_buf[TEST_MAX_VAL_SIZE];
thread_local uint64_t *total_time, *success_time;

struct test_params {
    std::string hostname;
    uint16_t port;
    uint8_t id;
    size_t get_requests;
    size_t put_requests;
    uint64_t total_time_ns;
    uint64_t success_time_ns;
};

inline uint64_t 
time_diff(const struct timespec *start, const struct timespec *end){
    return (uint64_t) (end->tv_sec - start->tv_sec) * 1'000'000'000ull + 
        end->tv_nsec - start->tv_nsec;
}


void get_callback(int status, const void *tag) {
    struct timespec end_time;
    (void) clock_gettime(CLOCK_MONOTONIC, &end_time);
    const struct timespec *start_time = (struct timespec *) tag;

    uint64_t diff = time_diff(start_time, &end_time);
    *total_time += diff;

    if (0 == status) {
        successful_gets++;
        *success_time += diff;
    }
}

void put_callback(int status, const void *tag) {
    struct timespec end_time;
    (void) clock_gettime(CLOCK_MONOTONIC, &end_time);
    const struct timespec *start_time = (struct timespec *) tag;

    uint64_t diff = time_diff(start_time, &end_time);
    *total_time += diff;

    if (0 == status) { 
        successful_puts++;
        *success_time += diff;
    }
}

void *test_thread(void *args) {
    struct test_params *params = (struct test_params *) args;
    if (0 > anchor_client::connect(params->hostname, 
            params->port, params->id, key_do_not_use)) {
        cerr << "Failed to connect to server" << endl;
        return nullptr;
    }
    srand((unsigned int) params->id);
    total_time = &(params->total_time_ns);
    success_time = &(params->success_time_ns);
    

    /* We have all start times in an array and pass the pointers 
     * to the put/get functions as a tag that is returned in the callback
     * TODO: Improve, as this is extremely memory-consuming
     */
    struct timespec *times = (struct timespec *)malloc(sizeof(struct timespec) * 
        (params->get_requests + params->put_requests));
    if (!times) {
        cerr << "Memory allocation failure" << endl;
        return nullptr;
    }

    size_t gets_performed = 0, puts_performed = 0;

    for (size_t req = 0; 
            gets_performed < params->get_requests 
            || gets_performed < params->put_requests;
            req++
    ) {
        size_t key = rand() % TEST_KV_SIZE;
        struct timespec *time_now = times + req;

        if (req % 2 == 0 && gets_performed < params->get_requests) {
            (void) clock_gettime(CLOCK_MONOTONIC, time_now);
            if (0 > anchor_client::get((void *) &key, sizeof(size_t), 
                (void *)value_buf, TEST_MAX_VAL_SIZE, get_callback, time_now)) {

                    cerr << "get() failed" << endl;
            }
            else
                gets_performed++;
        }
        else if (puts_performed < params->put_requests) {
            value_from_key((void *) value_buf, (void *) &key, sizeof(size_t));
            (void) clock_gettime(CLOCK_MONOTONIC, time_now);
            if (0 > anchor_client::put((void *) &key, sizeof(size_t), 
                (void *)value_buf, TEST_MAX_VAL_SIZE, put_callback, time_now)) {

                    cerr << "put() failed" << endl;
            }
            else 
                puts_performed++;
        }
    }
    params->get_requests = successful_gets;
    params->put_requests = successful_puts;
    free(times);
    return params;
}

void fill_params_structs(std::string hostname, uint16_t port, 
        struct test_params *params) {
    
    size_t loss_compensation = (TOTAL_REQUESTS / (TOTAL_CLIENTS * 16));
    size_t requests_per_client = TOTAL_REQUESTS / TOTAL_CLIENTS + 
        loss_compensation;

    struct test_params default_params = { 
        hostname, 
        port, 
        0, 
        requests_per_client / 2, 
        requests_per_client / 2,
        0,
        0
    };

    for (uint8_t id = 0; id < TOTAL_CLIENTS; id++) {
        memcpy((void *) (params + id), &default_params, 
            sizeof(struct test_params));
        params[id].id = id;
    }
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <ip-address>" << endl;
        return -1;
    }
    std::string server_hostname(argv[1]);
    uint16_t port = 31850;
    struct test_params params[TOTAL_CLIENTS];
    pthread_t threads[TOTAL_CLIENTS];
    fill_params_structs(server_hostname, port, params);
    
    /* Launch requester threads: */
    for (uint8_t i = 0; i < TOTAL_CLIENTS; i++) {
        if (pthread_create(&(threads[i]), nullptr, test_thread, 
                (void *) (params + i))) {
            cerr << "Failed to create thread " << i << endl;
            exit(1);
        }
    }

    /* Wait for requester threads to finish: */
    struct test_params *ret_param;
    for (uint8_t i = 0; i < TOTAL_CLIENTS; i++) {
        if (pthread_join(threads[i], (void **) &ret_param)) {
            if (!ret_param) {
                cerr << "Thread " << i << " exited with an error" << endl;
            }
        }
    }
}
