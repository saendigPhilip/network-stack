#include "Client.h"
#include "test_common.h"
#include "simple_unit_test.h"

const char test_key[] = "42";
char test_value[TEST_MAX_VAL_SIZE];
char incoming_test_value[TEST_MAX_VAL_SIZE];

void test_callback(int status, const void *tag) {
    EXPECT_EQUAL(0, status);
    EXPECT_EQUAL(test_key, tag);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <ip-address>" << endl;
        return -1;
    }
    std::string server_hostname(argv[1]);
    uint16_t port = 31850;
    uint8_t id = 0;
    if (0 > anchor_client::connect(server_hostname, port, id, key_do_not_use))
        return -1;


    BEGIN_TEST_DELIMITER("put operation");
    EXPECT_EQUAL(0, anchor_client::put((void *) test_key, sizeof(test_key), 
            (void*) test_value, sizeof(test_value), test_callback, (void *) test_key))
    END_TEST_DELIMITER();
    
    BEGIN_TEST_DELIMITER("get operation");
    EXPECT_EQUAL(0, anchor_client::get((void *) test_key, sizeof(test_key),
            test_value, TEST_MAX_VAL_SIZE, nullptr, test_callback, nullptr))
    END_TEST_DELIMITER();

    BEGIN_TEST_DELIMITER("delete operation");
    EXPECT_EQUAL(0, anchor_client::del((void *) test_key, sizeof(test_key), 
            test_callback, (void *) test_key));
    END_TEST_DELIMITER();


    (void ) anchor_client::disconnect();

}
