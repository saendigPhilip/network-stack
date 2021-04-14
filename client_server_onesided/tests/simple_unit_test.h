//
// Created by philip on 13.04.21.
//

#ifndef CLIENT_SERVER_ONESIDED_SIMPLE_UNIT_TEST_H
#define CLIENT_SERVER_ONESIDED_SIMPLE_UNIT_TEST_H

#define EXPECT_TRUE(expression) \
    total_count++; \
    if ((expression)) \
        {passed_count++;} \
    else { \
        failed_count++; \
        fprintf(stderr, "Failed test %u (line %d)\n", total_count, __LINE__); \
    }

#define EXPECT_EQUAL(expected, is) EXPECT_TRUE((expected) == (is))

#define BEGIN_TEST_DELIMITER(name) \
    test_name = (name); \
    printf("\n\n----------Testing %s----------\n\n", test_name)


#define END_TEST_DELIMITER() \
    printf("\n\n----------Finished testing %s----------\n\n", test_name)

#define PRINT_TEST_SUMMARY() printf("\n\n\n" \
    "Finished Testing\n" \
    "TOTAL:  %u\n" \
    "PASSED: %u\n" \
    "FAILED: %u\n\n", \
    total_count, passed_count, failed_count)

unsigned int total_count = 0;
unsigned int passed_count = 0;
unsigned int failed_count = 0;

const char *test_name;

#endif //CLIENT_SERVER_ONESIDED_SIMPLE_UNIT_TEST_H
