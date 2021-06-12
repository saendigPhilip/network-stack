#include <cstdint>
#include <cstring>
#include <iostream>
#include "test_common.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

#define STRTOUL(var, str)                                       \
    errno = 0;                                                  \
    (var) = std::strtoul(argv[++i], nullptr, 0);                \
    if (errno) {                                                \
        fprintf(stderr, "Invalid %s: %s\n", (str), (argv[i]));  \
        return -1;                                              \
    }


struct global_test_params global_params;


void value_from_key(
        void *value, size_t value_len, const void *key, size_t key_len) {
    size_t to_copy;
    for (size_t i = 0; i < value_len; i += to_copy) {
        to_copy = min(key_len, value_len - i);
        memcpy(value, key, to_copy);
        value = (void *) ((uint8_t *)value + to_copy);
    }
}

int global_test_params::parse_args(int argc, const char **argv) {
    for (int i = 0; i < argc; i++) {
        switch (argv[i][1]) {
            case 'k':
                STRTOUL(key_size, "key size");
                break;
            case 'v':
                STRTOUL(val_size, "value size");
                break;
            case 's':
                STRTOUL(max_key_size, "Maximum key size");
                break;
            case 'n':
                STRTOUL(num_clients, "Number of clients");
                break;
            case 'i':
                STRTOUL(event_loop_iterations,
                    "Number of event loop iterations");
                break;
            case 't':
                STRTOUL(client_timeout_us,
                    "Client timeout in case of server overload");
                break;
            case 'p':
                STRTOUL(puts_per_client, "Number of puts per client");
                break;
            case 'g':
                STRTOUL(gets_per_client, "Number of gets per client");
                break;
            case 'd':
                STRTOUL(dels_per_client, "Number of deletes per client");
                break;
            default:
                std::cerr << "Unknown commandline option: "
                          << argv[i] << std::endl;
                return -1;
        }
    }
    return 0;
}

void global_test_params::print_options() {
    std::cout << "\t[-k <key size>]\n"
                 "\t[-v <value size>]\n"
                 "\t[-s <Maximum Key size>]\n"
                 "\t[-n <number clients>]\n"
                 "\t[-i <number of client event loop iterations>]\n"
                 "\t[-t <client timeout on server overload in us>]"
                 "\t[-p <number of put operations per client>]\n"
                 "\t[-g <number of get operations per client>]\n"
                 "\t[-d <number of delete operations per client>]\n"
                 << std::endl;
}
