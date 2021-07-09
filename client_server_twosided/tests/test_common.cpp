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

#define STRTOUI(var, str)                                               \
    errno = 0;                                                          \
    (var) = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 0)); \
    if (errno) {                                                        \
        fprintf(stderr, "Invalid %s: %s\n", (str), (argv[i]));          \
        return -1;                                                      \
    }

#define STRTOUI8(var, str) \
    errno = 0;                                                          \
    (var) = static_cast<uint8_t>(std::strtoul(argv[++i], nullptr, 0));  \
    if (errno) {                                                        \
        fprintf(stderr, "Invalid %s: %s\n", (str), (argv[i]));          \
        return -1;                                                      \
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
                STRTOUI(max_key_size, "Maximum key size");
                break;
            case 'n':
                STRTOUI8(num_clients, "Number of clients");
                break;
            case 'i':
                STRTOUL(event_loop_iterations,
                    "Number of event loop iterations");
                break;
            case 'p':
                STRTOUL(total_puts, "Number of puts per client");
                break;
            case 'g':
                STRTOUL(total_gets, "Number of gets per client");
                break;
            case 'd':
                STRTOUL(total_dels, "Number of deletes per client");
                break;
            case 'f':
                global_params.path_csv = argv[++i];
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
                 "\t[-p <total number of put operations>]\n"
                 "\t[-g <total number of get operations>]\n"
                 "\t[-d <total number of delete operations>]\n"
                 "\t[-f <csv filename>]\n"
                 << std::endl;
}
