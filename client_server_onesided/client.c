#include <stdio.h>
#include <stdlib.h>
#include "client_server_utils.h"

const char* usage_string = "USAGE: ./test_client <address> <port>\n";
char* hostname = "127.0.0.1";
char* port = "8042";

struct rpma_peer* peer;

void disconnect(struct rpma_conn* connection) {
    printf("Disconnecting...\n");
    if (rpma_conn_disconnect(connection) < 0)
        fprintf(stderr, "Error while trying to disconnect\n");
}

void communicate(
        struct rpma_conn* connection, struct rpma_mr_local* mem_local, char* local_buf,
        struct rpma_mr_remote* shared_remote, size_t remote_size) {

    char* user_input = malloc(remote_size);
    if (!(user_input && connection && mem_local && local_buf && shared_remote)){
        fprintf(stderr, "Can't communicate!\n");
        goto end_communication;
    }
    char termination_string[] = "END\n";
    char update_string[] = "UPD\n";
        struct rpma_completion completion;

    printf("Type\n%s"
           "to update message buffer.\n"
           "Type\n%s"
           "to end communication and disconnect\n"
           "Any other message will be sent to the peer\n",
           update_string, termination_string);

    while(strcmp(fgets(user_input, remote_size, stdin), termination_string)) {
        if (strcmp(user_input, update_string) == 0) {
            if (rpma_read(connection, mem_local, 0, shared_remote, 0, remote_size,
                          RPMA_F_COMPLETION_ALWAYS, NULL) < 0) {
                fprintf(stderr, "Error while reading from server\n");
                goto end_communication;
            }

            if (rpma_conn_completion_wait(connection) < 0) {
                fprintf(stderr, "Error while waiting for read completion\n");
                goto end_communication;
            }

            if (rpma_conn_completion_get(connection, &completion) < 0 ||
                    completion.op != RPMA_OP_READ || completion.op_status != IBV_WC_SUCCESS) {
                fprintf(stderr, "Error while getting read results\n");
            } else
                puts(local_buf);
        } else {
            strcpy(local_buf, user_input);
            if (rpma_write(connection, shared_remote, 0, mem_local, 0, remote_size,
                           RPMA_F_COMPLETION_ALWAYS, NULL) < 0) {
                fprintf(stderr, "Error while writing to remote memory\n");
                goto end_communication;
            }
        }
    }

end_communication:
    puts("Ending communication\n");
    free(user_input);
    return;
}

int get(void* key, size_t key_len, void* value, size_t* value) {

}

int main(int argc, char** argv){
    int ret = -1;
    if (parseargs(argc, argv))
        return ret;

    if (initialize_peer())
        return ret;

    struct rpma_conn_cfg* config = NULL;
    struct rpma_conn_req* request = NULL;
    struct rpma_conn* connection = NULL;
    char* local_buf = NULL;
    size_t local_size = 1024;
    int usage = RPMA_MR_USAGE_READ_DST | RPMA_MR_USAGE_WRITE_SRC;
    struct rpma_mr_local* mem_local = NULL;
    struct rpma_conn_private_data private_data;
    struct rpma_mr_remote* shared_remote = NULL;
    size_t remote_size;
    struct common_data* data;

    local_buf = (char*) malloc_aligned(local_size);
    if (!local_buf) {
        fprintf(stderr, "Memory allocation failure\n");
        goto delete_peer;
    }

    if (rpma_mr_reg(peer, (void*) local_buf, local_size, usage, &mem_local) < 0) {
        fprintf(stderr, "Could not register local memory region!\n");
        goto delete_peer;
    }

    if (rpma_conn_req_new(peer, hostname, port, config, &request) < 0){
        fprintf(stderr, "Error creating connection request\n");
        goto deregister_memory;
    }

    printf("Connecting to server at %s, port %s...\n", hostname, port);
    if (rpma_conn_req_connect(&request, NULL, &connection) < 0) {
        fprintf(stderr, "Error while issuing connection request\n");
        goto deregister_memory;
    }

    if (establish_connection(connection) < 0)
        goto abort_connection;

    printf("Connected to server!\n");

    if (rpma_conn_get_private_data(connection, &private_data) < 0) {
        fprintf(stderr, "Server didn't specify remote memory location\n");
        goto disconnect;
    }

    data = private_data.ptr;
    if (rpma_mr_remote_from_descriptor(data->descriptors, data->mr_desc_size, &shared_remote) < 0) {
        fprintf(stderr, "Could not get remote shared memory\n");
        goto disconnect;
    }

    if (rpma_mr_remote_get_size(shared_remote, &remote_size) < 0) {
        fprintf(stderr, "Could not get size of remote shared memory");
        goto delete_remote;
    }
    if(remote_size > local_size) {
        fprintf(stderr, "Size of remote memory region is bigger than local one!\n");
        goto disconnect;
    }

    communicate(connection, mem_local, local_buf, shared_remote, remote_size);


delete_remote:
    (void) rpma_mr_remote_delete(&shared_remote);

disconnect:
    disconnect(connection);

    (void) wait_for_disconnect_event(connection, 1);

abort_connection:
    if (rpma_conn_delete(&connection) < 0) {
        fprintf(stderr, "Failed free connection structure\n");
        rpma_peer_delete(&peer);
        return ret;
    }
    printf("Disconnected\n");

deregister_memory:
    (void) rpma_mr_dereg(&mem_local);

delete_peer:
    free(local_buf);
    if (rpma_peer_delete(&peer) < 0)
        fprintf(stderr, "Failed to free peer structure\n");
    else
        ret = 0;

    return ret;
}

