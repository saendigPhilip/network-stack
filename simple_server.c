#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "client_server_utils.h"

char* hostname = "127.0.0.1";
char* port = "8042";
const char* usage_string = "USAGE: ./test_server <hostname> <port>\n";

struct rpma_peer* peer = NULL;

void disconnect(struct rpma_conn* connection) {
    printf("Disconnecting...\n");
    if (rpma_conn_disconnect(connection) < 0){
        fprintf(stderr, "Error trying to disconnect\n");
    }
}

void communicate(char* shared, size_t shared_size) {
    char* user_input = (char*) malloc(shared_size);
    if (!(shared && user_input))
        goto end_communicate;

    char termination_string[] = "END\n";
    char update_string[] = "UPD\n";
    printf("Type\n%s"
           "to update message buffer.\n"
           "Type\n%s"
           "to end communication and disconnect\n"
           "Any other message will be sent to the peer\n",
           update_string, termination_string);

    while(strcmp(fgets(user_input, shared_size, stdin), termination_string)) {
        if (strcmp(fgets(user_input, shared_size, stdin), update_string) == 0)
            puts(shared);
        else
            strcpy(shared, user_input);
    }

end_communicate:
    free(user_input);
    return;
}

int main(int argc, char** argv) {
    int ret = -1;
    if (parseargs(argc, argv))
        return ret;

    if (initialize_peer())
        return ret;

    struct rpma_ep* endpoint = NULL;
    struct rpma_mr_local* memory_region = NULL;
    struct rpma_conn_cfg* config = NULL;
    struct rpma_conn_req* request = NULL;
    struct rpma_conn* connection;
    char* shared = NULL;
    size_t shared_size = 1024;
    int usage = RPMA_MR_USAGE_READ_DST | RPMA_MR_USAGE_WRITE_DST;

    struct rpma_conn_private_data private_data;
    struct common_data data;

    /* Allocate shared memory region */
    shared = (char*) malloc_aligned(shared_size);
    if (!shared) {
        fprintf(stderr, "Memory allocation failure\n");
        goto free_peer;
    }
    strcpy(shared, "Servus!");

    /* Register shared memory region */
    if (rpma_mr_reg(peer, (void*) shared, shared_size, usage, &memory_region) < 0){
        fprintf(stderr, "Failed to register memory region\n");
        goto free_peer;
    }

    size_t descriptor_size;
    if (rpma_mr_get_descriptor_size(memory_region, &descriptor_size) < 0) {
        fprintf(stderr, "Failed to get size of registered memory\n");
        goto deregister_memory;
    }
    data.mr_desc_size = (uint8_t) descriptor_size;

    if (rpma_mr_get_descriptor(memory_region, data.descriptors) < 0) {
        fprintf(stderr, "Failed to get descriptor of registered memory\n");
        goto deregister_memory;
    }

    private_data.ptr = &data;
    private_data.len = sizeof(struct common_data);

    /* Listen on user-defined hostname and port */
    if (rpma_ep_listen(peer, hostname, port, &endpoint)) {
        fprintf(stderr, "Can not listen on %s, port %s\n", hostname, port);
        goto free_peer;
    }
	printf("Listening on %s, port %s. Waiting for connection request...\n", hostname, port);

    /* Wait for connection request */
    if (rpma_ep_next_conn_req(endpoint, config, &request) < 0) {
        fprintf(stderr, "Something went wrong while accepting client request\n");
        goto shutdown_endpoint;
    }

    /* Accept client, send data */
    if (rpma_conn_req_connect(&request, &private_data, &connection) < 0) {
        fprintf(stderr, "Could not connect to client\n");
        goto shutdown_endpoint;
    }

    if (establish_connection(connection) < 0) {
        goto free_connection;
    }
    printf("Client connected!\n");

    communicate(shared, shared_size);

    disconnect(connection);

    (void) wait_for_disconnect_event(connection, 1);

deregister_memory:
    if (rpma_mr_dereg(&memory_region) < 0)
        fprintf(stderr, "Error deregistering memory region\n");
    /* Structures that have been set up during the connection process have to be deleted */
free_connection:
    if (rpma_conn_delete(&connection) < 0)
        fprintf(stderr, "Error trying to free connection structure\n");

    printf("Disconnected. ");

shutdown_endpoint:
    printf("Shutting down endpoint...\n");
    if (rpma_ep_shutdown(&endpoint) < 0)
        fprintf(stderr, "Could not shut down endpoint\n");
    else
        printf("Endpoint shut down successfully\n");

free_peer:
    free(shared);
    if (rpma_peer_delete(&peer) < 0)
        fprintf(stderr, "Could not free peer structure\n");
    else
        ret = 0;

    return ret;
}

