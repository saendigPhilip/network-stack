#include "onesided_client.h"
#include "onesided_server.h"
#include "onesided_test.h"


#define END_TEST end_main
#define EXPECT_TRUE((expression)) \
    if ((expression)) \
        {passed_count++;} \
    else {\
        fprintf(stderr, "Failed test %u (line %d)\n", passed_count + 1, __line__); \
        goto END_TEST; \
    }

#define EXPECT_EQUAL(expected, is) EXPECT_TRUE((expected) = (is))


unsigned int passed_count = 0;

int test_init() {
    if (0 > parseargs(argc, argv))
        return -1;

    // if (0 > host_server(ip, port, ))

}

int main(int argc, char **argv) {

}
