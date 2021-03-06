#include <cstring>

#include "Client.h"
#include "test_common.h"
#include "simple_unit_test.h"

const char test_key[] = "42";
char *test_value;
char *incoming_test_value;

void test_callback(enum ret_val status, const void *tag) {
    switch (status) {
        case ret_val::OP_SUCCESS:
            cout << "Success" << endl;
            break;
        case ret_val::TIMEOUT:
            cerr << "Timeout" << endl;
            break;
        case ret_val::OP_FAILED:
            cerr << "Operation failed" << endl;
            break;
        case ret_val::INVALID_RESPONSE:
            cerr << "Invalid response" << endl;
    }

    EXPECT_EQUAL(OP_SUCCESS, status);
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

    global_params.key_size = sizeof(size_t);
    test_value = static_cast<char *>(malloc(KEY_SIZE));
    incoming_test_value = static_cast<char *>(malloc(VAL_SIZE));

    Client::init(client_hostname, port);
    Client client(id, KEY_SIZE, VAL_SIZE);
    if (0 > client.connect(server_hostname, port, key_do_not_use))
        return -1;

    value_from_key(test_value, VAL_SIZE, test_key, sizeof(test_key));

    BEGIN_TEST_DELIMITER("put operation");
    EXPECT_EQUAL(0, client.put((void *) test_key, sizeof(test_key),
            (void*) test_value, sizeof(test_value), test_callback,
            (void *) test_key, 10000))
    END_TEST_DELIMITER();
    
    BEGIN_TEST_DELIMITER("get operation");
    EXPECT_EQUAL(0, client.get((void *) test_key, sizeof(test_key),
            incoming_test_value, nullptr, test_callback,
            test_key, 10000))
    EXPECT_EQUAL(0, memcmp(incoming_test_value, test_value, VAL_SIZE))
    END_TEST_DELIMITER();

    BEGIN_TEST_DELIMITER("delete operation");
    EXPECT_EQUAL(0, client.del((void *) test_key, sizeof(test_key),
            test_callback, (void *) test_key, 10000));
    END_TEST_DELIMITER();

    (void ) client.disconnect();
    Client::terminate();
    PRINT_TEST_SUMMARY();
    return 0;
}
