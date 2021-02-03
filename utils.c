#include <stdio.h>
#include "utils.h"

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
