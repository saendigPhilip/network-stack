//
// Created by philip on 05.04.21.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "onesided_test.h"

#define COMMAND_SIZE 4
#define USER_INPUT_SIZE (TEST_MAX_VALUE_SIZE - COMMAND_SIZE)

/* Sequence number should NOT be set to a fixed value */
static const size_t default_seq = 42;

static size_t keys[TEST_NUM_KV_ENTRIES] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

/*
 * Struct shared between client and server.
 * Should be exchanged in an initial handshake and definitely NOT be static
 */
struct local_key_info local_key_infos[TEST_NUM_KV_ENTRIES] = {
        {default_seq, (void *) (keys + 0), sizeof(size_t), 0 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 1), sizeof(size_t), 1 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 2), sizeof(size_t), 2 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 3), sizeof(size_t), 3 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 4), sizeof(size_t), 4 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 5), sizeof(size_t), 5 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 6), sizeof(size_t), 6 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 7), sizeof(size_t), 7 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 8), sizeof(size_t), 8 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 9), sizeof(size_t), 9 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 10), sizeof(size_t), 10 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 11), sizeof(size_t), 11 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 12), sizeof(size_t), 12 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 13), sizeof(size_t), 13 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 14), sizeof(size_t), 14 * TEST_ENTRY_SIZE},
        {default_seq, (void *) (keys + 15), sizeof(size_t), 15 * TEST_ENTRY_SIZE},
};

/*
 * Wrapper around strtol with bounds check for getting the address of the struct
 * Returns a pointer to the according local_key_info struct on success,
 * NULL otherwise
 */
struct local_key_info *get_key_info(const char *key_string) {
    if (!(key_string)) {
        PRINT_ERR("Invalid arguments");
        return NULL;
    }
    char* end_ptr;
    long int index = strtol(key_string, &end_ptr, 0);
    if (*key_string == '\0' || *end_ptr != '\0' || index < 0 || index >= TEST_NUM_KV_ENTRIES) {
        fprintf(stderr, "Invalid index: %s\n", key_string);
        return NULL;
    }
    return ((struct local_key_info*)local_key_infos) + (size_t)index;
}


void remove_trailing_newline(char *string) {
    while (*string) {
        if (*string == '\n') {
            *string = '\0';
            return;
        }
        else
            string++;
    }
}

char *skip_spaces(char *string) {
    while (*string) {
        if (*string != ' ')
            return string;
        else
            string++;
    }
    return string;
}

/* Parses a string until it finds a space
 * If a space is found, it is replaced with '\0' and a pointer to the next
 * non-space character is returned
 * Otherwise, a pointer to the end of the string is returned
 */
char *split(char *string) {
    while (*string) {
        if (*string == ' ') {
            *string = '\0';
            return skip_spaces(string + 1);
        }
        else
            string++;
    }
    return string;
}


void communicate(
        int (*get_function)(const char *key, char *value),
        int (*put_function)(const char *key, const char *value)) {

    char user_input[USER_INPUT_SIZE];
    char incoming_buf[VALUE_ENTRY_SIZE(TEST_MAX_VALUE_SIZE)];

    char get_string[COMMAND_SIZE] = "GET";
    char put_string[COMMAND_SIZE] = "PUT";
    char termination_string[COMMAND_SIZE] = "BYE";
    size_t command_len = COMMAND_SIZE - 1;

    printf("Type\n"
           "  - %s <key> to get the value for key.\n"
           "  - %s <key> <value> to put the key-value pair to the store\n"
           "  - %s to end communication and disconnect\n",
           get_string, put_string, termination_string);

    while(strncmp(fgets(user_input, USER_INPUT_SIZE, stdin),
            termination_string, command_len)) {

        remove_trailing_newline(user_input);
        char *key_string = skip_spaces(user_input + command_len);
        if (0 == strncmp(user_input, get_string, command_len)) {
            if (!get_function) {
                PRINT_ERR("GET is not supported");
                continue;
            }
            if (0 == get_function(key_string, incoming_buf)) {
                puts(incoming_buf);
            }
            else
                puts("Get failed");
        }
        else if (0 == strncmp(user_input, put_string, command_len)) {
            if (!put_function) {
                PRINT_ERR("PUT is not supported");
                continue;
            }
            char *val = split(key_string);
            if (0 == put_function(key_string, val)) {
                puts("Put was successful");
            }
            else
                puts("Put failed");
        }
        else
            puts("Invalid command");
    }

    puts("Ending communication\n");
    return;
}

void print_usage(char **argv) {
    fprintf(stderr, "USAGE: %s <hostname> <port>\n", argv[0]);
}

/**
 * Parses the arguments. If arguments are invalid, either -1 is returned or the user
 * is asked whether to choose default values
 * Sets the variables hostname and port
 *
 * @return              0  on success, -1 if parsing the arguments failed
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

