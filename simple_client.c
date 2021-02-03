#include <stdio.h>
#include "client_server_utils.h"

const char* usage_string = "USAGE: ./test_client <address> <port>\n";
char* hostname = "127.0.0.1";
char* port = "8042";

struct rpma_peer* peer;

int main(int argc, char** argv){
    int ret = -1;
    if (parseargs(argc, argv))
        return ret;

    if (initialize_peer())
        return ret;

    struct rpma_conn_cfg* config = NULL;
    struct rpma_conn_req* request = NULL;
    struct rpma_conn* connection = NULL;
    struct rpma_conn_private_data private_data;
    char message[] = "Hi!";

    if (rpma_conn_req_new(peer, hostname, port, config, &request) < 0){
        fprintf(stderr, "Error creating connection request\n");
        goto delete_peer;
    }

    private_data.ptr = (void*) message;
    private_data.len = strlen(message) + 1;
    printf("Connecting to server at %s, port %s...\n", hostname, port);
    if (rpma_conn_req_connect(&request, &private_data, &connection) < 0) {
        fprintf(stderr, "Error while issuing connection request\n");
        goto delete_peer;
    }

    if (establish_connection(connection) < 0)
        goto abort_connection;

    printf("Connected to server!\n");

    if (rpma_conn_get_private_data(connection, &private_data) == 0) {
        printf("From server: %s\n", (char*) private_data.ptr);
    }

    printf("Disconnecting...\n");
    (void) wait_for_disconnect_event(connection, 1);

    if (rpma_conn_disconnect(connection) < 0)
        fprintf(stderr, "Error while trying to disconnect\n");

abort_connection:
    if (rpma_conn_delete(&connection) < 0) {
        fprintf(stderr, "Failed free connection structure\n");
        rpma_peer_delete(&peer);
        return ret;
    }
    printf("Disconnected\n");

delete_peer:
    if (rpma_peer_delete(&peer) < 0)
        fprintf(stderr, "Failed to free peer structure\n");
    else
        ret = 0;

    return ret;
}

