//
// Created by philip on 14.04.21.
//
#include "onesided_test_all.h"

char *ip = "127.0.0.1";
char *port = "8042";


void print_usage(char **argv) {
    fprintf(stderr, "USAGE: %s <hostname> <port>\n", argv[0]);
}

/**
 * Parses the arguments. If arguments are invalid, either -1 is returned or the user
 * is asked whether to choose default values
 * Sets the variables hostname and port
 *
 * @return 0 on success, -1 if parsing the arguments failed
 */
int parseargs(int argc, char **argv){
    if (argv == NULL || argc == 0)
        return -1;
    if (argc == 1) {
ask:
        printf("No IP address or port given. Use localhost, port 8042? [y/n] ");
        unsigned char answer = fgetc(stdin);
        printf("\n");
        if (!(answer == 'y' || answer == 'Y')){
            if (answer == 'n' || answer == 'N'){
                print_usage(argv);
                return -1;
            }
            else
                goto ask;
        }
        return 0;
    }
    if (argc == 3) {
        ip = argv[1];
        port = argv[2];
        return 0;
    } else {
        print_usage(argv);
        return -1;
    }
}
