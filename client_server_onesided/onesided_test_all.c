#include "onesided_client.h"


#define END_TEST end_main
#define EXPECT_EQUAL(expected, is) \
    if ((expected) == (is)) \
        {passed_count++;} \
    else {\
        fprintf(stderr, "Failed test %u (line %d)\n", passed_count + 1, __line__); \
        goto END_TEST; \
    }

unsigned int passed_count = 0;

int main(void) {

}
