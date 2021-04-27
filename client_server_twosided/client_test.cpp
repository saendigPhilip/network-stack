#include "Client.h"
#include "test_common.h"

void test_callback(int status, const void *) {
    if (status == 0) {
        cout << "RPC executed successfully" << endl;
    }
    else {
        cout << "RPC failed" << endl;
    }
}

int main() {
    std::string server_hostname = "192.168.0.0";
    uint16_t port = 31850;
    uint8_t id = 0;
    if (0 > anchor_client::connect(server_hostname, port, id))
        return -1;

    const char test_key[] = "0";
    char test_value[TEST_MAX_VAL_SIZE];
    if (0 > anchor_client::get((void *) test_key, sizeof(test_key),
            test_value, TEST_MAX_VAL_SIZE, test_callback, nullptr, 1000))
        cerr << "Failed to issue request" << endl;

    (void ) anchor_client::disconnect();

}
