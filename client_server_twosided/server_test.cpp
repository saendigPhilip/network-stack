#include "Server.h"

int main() {
    int ret = -1;
    std::string ip = "192.168.2.113";
    const uint16_t standard_udp_port = 31850;
    if (host_server(ip, standard_udp_port, 100000)) {
        cerr << "Failed to host server" << endl;
        return ret;
    }
}
