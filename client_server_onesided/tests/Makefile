# Turn on additional warnings, link with librpma, openssl, libpmem
CFLAGS=-g -Wall -Wextra -Wpedantic -lrpma -lssl -lcrypto -lpmem 
SRC_DIR=../src/

UTILS=$(SRC_DIR)client_server_utils.c 
SERVER_TEST=$(SRC_DIR)onesided_server.c onesided_test_server.c onesided_test_all.c
CLIENT_TEST=$(SRC_DIR)onesided_client.c onesided_test_client.c onesided_test_all.c

.PHONY: all clean fresh asan clean_asan fresh_asan

all: server_test client_test
server_test:
	$(CC) $(SERVER_TEST) $(UTILS) -I$(SRC_DIR) -o $@ $^  $(CFLAGS)

client_test:
	$(CC) $(CLIENT_TEST) $(UTILS) -I$(SRC_DIR) -o $@ $^  $(CFLAGS)

clean:
	rm -f server_test client_test

fresh: clean
	$(MAKE) 


asan: server_test_asan client_test_asan
server_test_asan:
	$(CC) $(SERVER_TEST) $(UTILS) -I$(SRC_DIR) -o $@ $^  $(CFLAGS) -fsanitize=address

client_test_asan:
	$(CC) $(CLIENT_TEST) $(UTILS) -I$(SRC_DIR) -o $@ $^  $(CFLAGS) -fsanitize=address

clean_asan:
	rm -f server_test_asan client_test_asan

fresh_asan: clean_asan
	$(MAKE) asan
