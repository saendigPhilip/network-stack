# Turn on additional warnings, link with librpma, openssl
CFLAGS=-g -Wall -Wextra -Wpedantic -lrpma -lssl -lcrypto 
SRC_DIR=../src/
TEST_DIR=../tests/

UTILS=onesided_test_cli.c $(SRC_DIR)client_server_utils.c $(TEST_DIR)onesided_test_all.c
SERVER=server_test_cli.c $(SRC_DIR)onesided_server.c 
CLIENT=client_test_cli.c $(SRC_DIR)onesided_client.c 

.PHONY: all clean fresh 
all: server_test_cli client_test_cli
server_test_cli: 
	$(CC) $(SERVER) $(UTILS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $^ $(CFLAGS)

client_test_cli: 
	$(CC) $(CLIENT) $(UTILS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $^ $(CFLAGS)

clean:
	rm -f server_test_cli client_test_cli

fresh: clean
	$(MAKE) 


