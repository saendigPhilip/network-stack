#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

int lower_port_bound = 1024, upper_port_bound = 32768;

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
