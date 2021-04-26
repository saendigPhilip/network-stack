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
    if (0 > connect(server_hostname, port, id, 100))
        return -1;

    const char test_key[] = "0";
    char test_value[TEST_MAX_VAL_SIZE];
    if (0 > get_from_server((void *) test_key, sizeof(test_key),
            test_value, TEST_MAX_VAL_SIZE, test_callback, nullptr, 1000))
        cerr << "Failed to issue request" << endl;

    (void ) disconnect();

}
