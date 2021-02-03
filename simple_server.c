
#include <stdio.h>
#include <unistd.h>
#include "client_server_utils.h"

char* hostname = "127.0.0.1";
char* port = "8042";
const char* usage_string = "USAGE: ./test_server <hostname> <port>\n";

struct rpma_peer* peer = NULL;

int main(int argc, char** argv) {
    if (parseargs(argc, argv))
        return -1;

    if (initialize_peer())
        return -1;

    struct rpma_ep* endpoint = NULL;
    struct rpma_conn_cfg* config = NULL;
    struct rpma_conn_req* request = NULL;
    struct rpma_conn* connection;

    struct rpma_conn_private_data private_data;
    char message[] = "Servus!";
    private_data.ptr = (void*) message;
    private_data.len = strlen(message) + 1;

    if (rpma_ep_listen(peer, hostname, port, &endpoint)) {
        fprintf(stderr, "Can not listen on %s, port %s\n", hostname, port);
        return -1;
    }
	printf("Listening on %s, port %s. Waiting for connection request...\n", hostname, port);

    /* Wait for connection request */
    if (rpma_ep_next_conn_req(endpoint, config, &request) < 0) {
        fprintf(stderr, "Something went wrong while accepting client request\n");
        return -1;
    }

    /* Accept client, do not send any private data */
    if (rpma_conn_req_connect(&request, &private_data, &connection) < 0) {
        fprintf(stderr, "Could not connect to client\n");
        return -1;
    }

    if (establish_connection(connection) < 0)
        return -1;

    printf("Client connected!\n");

    if (rpma_conn_get_private_data(connection, &private_data) == 0){
        printf("From client: %s\n", (char*) private_data.ptr);
    }

    printf("Disconnecting...\n");
    if (rpma_conn_disconnect(connection) < 0){
        fprintf(stderr, "Error trying to disconnect\n");
        return -1;
    }

    if (wait_for_disconnect_event(connection, 1) < 0)
        return -1;

    if (rpma_conn_delete(&connection) < 0) {
        fprintf(stderr, "Error trying to delete connection\n");
        return -1;
    }
    printf("Disconnected. Shutting down server...\n");

    if (rpma_ep_shutdown(&endpoint) < 0){
        fprintf(stderr, "Could not shut down endpoint\n");
        return -1;
    }
    printf("Server shut down successfully\n");

    return 0;
}

