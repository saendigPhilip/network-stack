#include <cstring>

#include "Client.h"
#include "test_common.h"
#include "simple_unit_test.h"

const char test_key[] = "42";
char test_value[TEST_MAX_VAL_SIZE];
char incoming_test_value[TEST_MAX_VAL_SIZE];

void test_callback(enum anchor_client::ret_val status, const void *tag) {
    switch (status) {
        case anchor_client::ret_val::OP_SUCCESS:
            cout << "Success" << endl;
            break;
        case anchor_client::ret_val::TIMEOUT:
            cerr << "Timeout" << endl;
            break;
        case anchor_client::ret_val::OP_FAILED:
            cerr << "Operation failed" << endl;
            break;
        case anchor_client::ret_val::INVALID_RESPONSE:
            cerr << "Invalid response" << endl;
    }

    EXPECT_EQUAL(anchor_client::OP_SUCCESS, status);
    EXPECT_EQUAL(test_key, tag);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] <<
            " <client ip-address> <server ip-address>" << endl;
        return -1;
    }
    std::string client_hostname(argv[1]);
    std::string server_hostname(argv[2]);
    uint16_t port = 31850;
    uint8_t id = 0;
    anchor_client::Client client(client_hostname, port, id);
    if (0 > client.connect(server_hostname, port, key_do_not_use))
        return -1;

    value_from_key(test_value, test_key, sizeof(test_key));

    BEGIN_TEST_DELIMITER("put operation");
    EXPECT_EQUAL(0, client.put((void *) test_key, sizeof(test_key),
            (void*) test_value, sizeof(test_value), test_callback,
            (void *) test_key, 10000))
    END_TEST_DELIMITER();
    
    BEGIN_TEST_DELIMITER("get operation");
    EXPECT_EQUAL(0, client.get((void *) test_key, sizeof(test_key),
            incoming_test_value, TEST_MAX_VAL_SIZE, nullptr, test_callback,
            test_key, 10000))
    EXPECT_EQUAL(0, memcmp(incoming_test_value, test_value, TEST_MAX_VAL_SIZE))
    END_TEST_DELIMITER();

    BEGIN_TEST_DELIMITER("delete operation");
    EXPECT_EQUAL(0, client.del((void *) test_key, sizeof(test_key),
            test_callback, (void *) test_key, 10000));
    END_TEST_DELIMITER();


    (void ) client.disconnect();
    PRINT_TEST_SUMMARY();
    return 0;
}
