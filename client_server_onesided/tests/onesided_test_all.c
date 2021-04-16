//
// Created by philip on 14.04.21.
//
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "onesided_test_all.h"


char *ip = "127.0.0.1";
char *port = "8042";



long timediff() {
    return (end_time.tv_sec - start_time.tv_sec) * 1000000000 +
           end_time.tv_nsec - start_time.tv_nsec;
}

void print_usage(char **argv) {
    fprintf(stderr, "USAGE: %s <hostname> <port>\n", argv[0]);
}

size_t min(size_t a, size_t b) {
    return a < b ? a : b;
}


/**
 * Parses the arguments. If arguments are invalid, either -1 is returned or the user
 * is asked whether to choose default values
 * Sets the variables hostname and port
 *
 * @return 0 on success, -1 if parsing the arguments failed
 */
int parseargs(int argc, char **argv){
    if (argv == NULL || argc == 0)
        return -1;
    if (argc == 1) {
ask:
        printf("No IP address or port given. Use localhost, port 8042? [y/n] ");
        unsigned char answer = fgetc(stdin);
        printf("\n");
        if (!(answer == 'y' || answer == 'Y')){
            if (answer == 'n' || answer == 'N'){
                print_usage(argv);
                return -1;
            }
            else
                goto ask;
        }
        return 0;
    }
    if (argc == 3) {
        ip = argv[1];
        port = argv[2];
        return 0;
    } else {
        print_usage(argv);
        return -1;
    }
}


void value_from_key(void *value, const void *key, size_t key_len) {
    size_t to_copy;
    for (size_t i = 0; i < TEST_KV_MAX_VAL_SIZE; i += to_copy) {
        to_copy = min(key_len, TEST_KV_MAX_VAL_SIZE - i);
        memcpy(value, key, to_copy);
        value = (void *) ((uint8_t *)value + to_copy);
    }
}


int setup_local_key_info(size_t value_entry_size) {
    void *buf;
    srand(1000);
    for (size_t i = 0; i < TEST_KV_NUM_ENTRIES; i++) {
        buf = malloc(sizeof(size_t));
        test_key_infos[i].sequence_number =
                ((uint64_t)rand() << 32) | (u_int64_t)rand();
        if (!buf) {
            PRINT_ERR("Could not initialize local_key_info structures");
            for (size_t j = 0; j < i; j++)
                free(test_key_infos[i].key);
            return -1;
        }
        /* We're using the index as key: */
        *((size_t *)buf) = i;
        test_key_infos[i].key = buf;
        test_key_infos[i].key_size = sizeof(size_t);
        test_key_infos[i].value_offset = i * value_entry_size;
    }

    return 0;
}

void free_local_key_info() {
    for (size_t i = 0; i < TEST_KV_NUM_ENTRIES; i++) {
        free(test_key_infos[i].key);
        test_key_infos[i].key = NULL;
    }
}


/*
 * Performs num_access get-operations iterations times and prints a summary
 * Returns 0 on success, -1 on failure
 */
int perform_test_get(
        void *(get_function)(struct local_key_info *, void *),
        uint32_t iterations, uint64_t num_accesses) {
    unsigned char result_buf[TEST_KV_MAX_VAL_SIZE];
    unsigned char correct_val_buf[TEST_KV_MAX_VAL_SIZE];
    void *get_dest;
    long total_time = 0;
    struct local_key_info *info;
    for (uint32_t i = 0; i < iterations; i++) {
        printf("Iteration %u of %u\r", i + 1, iterations);
        fflush(stdout);
        for (uint64_t j = 0; j < num_accesses; j++) {
            info = (struct local_key_info *) test_key_infos +
                    ((size_t) rand() % TEST_KV_NUM_ENTRIES);

            (void) clock_gettime(CLOCK_MONOTONIC, &start_time);
            get_dest = get_function(info, (void *) result_buf);
            (void) clock_gettime(CLOCK_MONOTONIC, &end_time);

            if (!get_dest || get_dest != result_buf)
                return -1;
            value_from_key(correct_val_buf, info->key, info->key_size);

            if (0 != memcmp((void *) get_dest,
                    (void *) correct_val_buf, TEST_KV_MAX_VAL_SIZE))
                return -1;

            total_time += timediff();
        }
    }
    double average_time = (double)total_time /
                          ((double)iterations * (double)num_accesses);
    printf("\n\nTotal time: %ld ns "
           "(Number of iterations: %u, Number of accesses per iteration: %lu)\n"
           "Average time per access: %f ns\n",
            total_time, iterations, num_accesses, average_time);
    return 0;
}
