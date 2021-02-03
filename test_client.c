#include <stdio.h>
#include "utils.h"

const char* usage_string = "USAGE: ./test_client <address> <port>\n";
char* hostname = "127.0.0.1";
char* port = "8042";

struct rpma_peer* peer;

int main(int argc, char** argv){
    if (parseargs(argc, argv))
        return -1;

    if (initialize_peer())
        return -1;


}

