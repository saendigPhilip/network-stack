#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "client_server_utils.h"

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
 *                      -1 if an error occurs
 */
int establish_connection(struct rpma_conn* connection) {
    enum rpma_conn_event event;
    /* Wait for the "Connection established" event */
    if (rpma_conn_next_event(connection, &event) < 0){
        fprintf(stderr, "Could not establish connection\n");
        return -1;
    }

    if (event != RPMA_CONN_ESTABLISHED){
        fprintf(stderr, "Connection could not be established. Unexpected event: ");
        switch(event){
            case RPMA_CONN_CLOSED: fprintf(stderr, "connection closed\n"); break;
            case RPMA_CONN_LOST: fprintf(stderr, "connection lost\n"); break;
            case RPMA_CONN_UNDEFINED:
            default:
                fprintf(stderr, "unknown connection event\n"); break;
        }
        return -1;
    }
    return 0;
}


/**
 * Ends a connection by waiting for the RPMA_CONN_CLOSED event and calling rpma_conn_disconnect()
 * rpma_conn_disconnect has to be called
 *      - by clients after calling this method
 *      - by servers before calling this method
 * @param connection    connection to close
 * @param verbose       if not 0, error messages are printed to stderr
 * @return              0  if RPMA_CONN_CLOSED event was received
 *                      -1 if not
 */
int wait_for_disconnect_event(struct rpma_conn* connection, int verbose) {
    enum rpma_conn_event event;
    /* Wait for the "Connection Closed" event */
    if (rpma_conn_next_event(connection, &event) < 0) {
        if (verbose)
            fprintf(stderr, "Error waiting for the next connection event\n");
        return -1;
    }

    if (event != RPMA_CONN_CLOSED) {
        if (verbose)
            fprintf(stderr, "Connection could not be closed. Unexpected event: ");
        switch (event) {
            case RPMA_CONN_ESTABLISHED:
                if (verbose)
                    fprintf(stderr, "Connection established\n");
                break;
            case RPMA_CONN_LOST:
                if (verbose)
                    fprintf(stderr, "Connection lost\n");
                break;
            case RPMA_CONN_UNDEFINED:
            default:
                if (verbose)
                    fprintf(stderr, "Unknown connection event\n");
        }
        return -1;
    }

    return 0;
}

/**
 * Aligned malloc, mostly taken from examples from librpma:
 * https://github.com/pmem/rpma/blob/master/examples/common/common-conn.c
 *
 * @param size  size of the allocated memory
 * @return      Pointer to allocated memory (NULL, if something went wrong)
 */
void* malloc_aligned(size_t size){
    long pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize < 0) {
        perror("sysconf");
        return NULL;
    }

    /* allocate a page size aligned local memory pool */
    void *mem;
    int ret = posix_memalign(&mem, (size_t)pagesize, size);
    if (ret) {
        (void) fprintf(stderr, "posix_memalign: %s\n", strerror(ret));
        return NULL;
    }

    return mem;
}
