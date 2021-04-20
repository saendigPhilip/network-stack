#include "Client.h"

int main() {
    /* erpc::Nexus nexus(client_uri, 0, 0);

    rpc = new erpc::Rpc<erpc::CTransport>(&nexus, nullptr, 0, sm_handler);

    std::string server_uri = kServerHostname + ":" + std::to_string(kUDPPort);
    int session_num = rpc->create_session(server_uri, 0);

    while (!rpc->is_connected(session_num)) rpc->run_event_loop_once();

    req = rpc->alloc_msg_buffer_or_die(kMsgSize);
    resp = rpc->alloc_msg_buffer_or_die(kMsgSize);

    rpc->enqueue_request(session_num, kReqType, &req, &resp, cont_func, nullptr);
    rpc->run_event_loop(100);

    delete rpc;
    */
    /*
    const char *ip = "192.168.0.0";
    const uint16_t standard_udp_port = 31850;
    if (connect(ip, standard_udp_port, 10000)) {
        cerr << "Failed to connect to server" << endl;

        return -1;
    }
    const char *key = "Test";
    size_t key_len = 5;
    size_t buf_size = 2048;
    char *buf = (char *)malloc(buf_size);
    size_t timeout = 1000;

    if (get_from_server(key, key_len, 1024, buf, verbose_cont_func, timeout)){
        cerr << "get_from_server() failed..." << endl;
        return -1;
    }
    return disconnect();
     */
}
