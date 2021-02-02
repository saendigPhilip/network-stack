
#include <librpma.h>
#include <stdio.h>
#include <unistd.h>

const char* addr = "127.0.0.1";
const char* port = "8042";

const char* usage_string = "USAGE: ./test_server <address> <port>\n";

int main(int argc, char** argv) {
    if (argc == 1) {
        ask:
        printf("No IP address or port given. Use localhost, port 8042? [y/n] ");
        unsigned char answer = fgetc(stdin);
        printf("\n");
        if (!(answer == 'y' || answer == 'Y')){
            if (answer == 'n' || answer == 'N'){
                fputs(usage_string, stderr);
                return 1;
            }
            else
                goto ask;
        }
    } else if (argc == 3) {
        addr = argv[1];
        port = argv[2];
    } else {
        fputs(usage_string, stderr);
        return 1;
    }

    struct rpma_peer* peer = NULL;
    struct ibv_context* ibv_ctx = NULL;
    struct rpma_ep* endpoint = NULL;
    struct rpma_conn_cfg* config = NULL;
    struct rpma_conn_req* request = NULL;
    // struct rpma_conn_private_data private;
    struct rpma_conn* connection;
    enum rpma_conn_event event;


    switch (rpma_utils_get_ibv_context(addr, RPMA_UTIL_IBV_CONTEXT_LOCAL, &ibv_ctx)) {
        case RPMA_E_INVAL:
            fprintf(stderr, "Invalid arguments: Cannot get IBV context\n");
            return -1;
        case RPMA_E_NOMEM:
            fprintf(stderr, "Memory allocation failure\n");
            return -1;
        case RPMA_E_PROVIDER:
            fprintf(stderr, "Network error\n");
            return -1;
        case 0: break;
        default:
            fprintf(stderr, "Unknown error\n");
            return -1;
    }

    switch(rpma_peer_new(ibv_ctx, &peer)) {
        case RPMA_E_INVAL:
            fprintf(stderr, "Could not create peer structure. Internal error\n");
            return -1;
        case RPMA_E_NOMEM:
            fprintf(stderr, "Could not create peer structure. Memory allocation failure\n");
            return -1;
        case RPMA_E_PROVIDER:
            fprintf(stderr, "Could not create peer structure. "
                            "Failed to create verbs protection domain\n");
            return -1;
        case 0: break;
        default:
            printf("Could not create peer structure. Unknown error\n");
            return -1;
    }

    if (rpma_ep_listen(peer, addr, port, &endpoint)) {
        fprintf(stderr, "Can not listen on address %s, port %s\n", addr, port);
        return -1;
    }
	printf("Listening on address %s, port %s. Waiting for connection request...\n", addr, port);

    /* Wait for connection request */
    if (rpma_ep_next_conn_req(endpoint, config, &request) < 0) {
        fprintf(stderr, "Something went wrong while accepting client request\n");
        return -1;
    }

    /* Accept client, do not send any private data */
    if (rpma_conn_req_connect(&request, NULL, &connection) < 0) {
        fprintf(stderr, "Could not connect to client\n");
        return -1;
    }

    /* Wait for the "Connection established" event */
    if (rpma_conn_next_event(connection, &event) < 0){
        fprintf(stderr, "Could not establish connection\n");
        return -1;
    }

    if (event != RPMA_CONN_ESTABLISHED){
        fprintf(stderr, "Connection could not be established. Error: ");
        switch(event){
            case RPMA_CONN_CLOSED: fprintf(stderr, "connection closed\n"); break;
            case RPMA_CONN_LOST: fprintf(stderr, "connection lost\n"); break;
            case RPMA_CONN_UNDEFINED:
            default:
                fprintf(stderr, "undefined connection event\n"); break;
        }
    }
    printf("Client connected!\n");


    return 0;
}

