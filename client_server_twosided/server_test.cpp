#include "Server.h"

int main() {
    int ret = -1;
    std::string ip = "192.168.2.113";
    if (host_server(ip, kUDPPort, 100000)) {
        cerr << "Failed to host server" << endl;
        return ret;
    }
}
