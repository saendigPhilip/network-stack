#include <librpma.h>
#include <stdio.h>
#include "utils.h"

const char* usage_string = "USAGE: ./test_client <address> <port>\n";
char* hostname = "127.0.0.1";
char* port = "8042";

int main(int argc, char** argv){
    if (parseargs(argc, argv))
        return -1;
}

