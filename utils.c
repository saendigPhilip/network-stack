#include <stdio.h>
#include "utils.h"

/**
 * Parses the arguments. If arguments are invalid, either -1 is returned or the user
 * is asked whether to choose default values
 * Sets the variables hostname and port
 *
 * @param hostname      Location where the hostname will be stored
 * @param port          Int-Pointer to where the port will be stored
 * @return              0  on success
 *                      -1 if parsing the arguments failed
 */
int parseargs(int argc, char** argv){
    if (argv == NULL || argc == 0)
        return -2;
    if (argc == 1) {
        ask:
        printf("No IP address or port given. Use localhost, port 8042? [y/n] ");
        unsigned char answer = fgetc(stdin);
        printf("\n");
        if (!(answer == 'y' || answer == 'Y')){
            if (answer == 'n' || answer == 'N'){
                fputs(usage_string, stderr);
                return -2;
            }
            else
                goto ask;
        }
        return 0;
    }
    if (argc == 3) {
        hostname = argv[1];
        port = argv[2];
        return 0;
    } else {
        fputs(usage_string, stderr);
        return -2;
    }
}

/**
 * Initializes the rpma_peer structure that is needed by both, client and server to
 * communicate via RPMA
 *
 * @return  0  on success
 *          -1 if some error occurs and the peer structure can't be initialized
 */
int initialize_peer(){
    /* ibv_ctx can be local because it's not needed after initialization */
    struct ibv_context* ibv_ctx;

    switch (rpma_utils_get_ibv_context(hostname, RPMA_UTIL_IBV_CONTEXT_LOCAL, &ibv_ctx)) {
        case 0: break;
        case RPMA_E_INVAL:
            fprintf(stderr, "Invalid arguments: Cannot get IBV context\n");
            return -1;
        case RPMA_E_NOMEM:
            fprintf(stderr, "Memory allocation failure\n");
            return -1;
        case RPMA_E_PROVIDER:
            fprintf(stderr, "Network error\n");
            return -1;
        default:
            fprintf(stderr, "Unknown error\n");
            return -1;
    }

    switch(rpma_peer_new(ibv_ctx, &peer)) {
        case 0: return 0;
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
        default:
            printf("Could not create peer structure. Unknown error\n");
            return -1;
    }
}

/**
 * Waits for the next event and checks, if it is an RPMA_CONN_ESTABLISHED event
 *
 * @param connection    Connection structure on which the event is obtained
 * @return              0  on success
 *                      -1 if an error occures
 */
int establish_connection(struct rpma_conn* connection) {
    enum rpma_conn_event event;
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
        return -1;
    }
    printf("Client connected!\n");
    return 0;
}
