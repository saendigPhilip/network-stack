# Make debuggable, turn on additional warnings, link with librpma, openssl
CFLAGS=-g -Wall -Wextra -Wpedantic -lrpma -lssl -lcrypto
UTILS=onesided_global.h client_server_utils.c client_server_utils.h  onesided_test.c onesided_test.h
SERVER=server_test.c onesided_server.c onesided_server.h
CLIENT=client_test.c onesided_client.c onesided_client.h

.PHONY: all clean fresh asan clean_asan fresh_asan
all: server_test client_test
server_test: 
	$(CC) $(SERVER) $(UTILS) -o $@ $^ $(CFLAGS)

client_test: 
	$(CC) $(CLIENT) $(UTILS) -o $@ $^ $(CFLAGS)

clean:
	rm -f server_test client_test

fresh: clean
	$(MAKE) all

asan: server_test_asan client_test_asan
server_test_asan:
	$(CC) $(SERVER) $(UTILS) -o $@ $^ $(CFLAGS) -fsanitize=address

client_test_asan: 
	$(CC) $(CLIENT) $(UTILS) -o $@ $^ $(CFLAGS) -fsanitize=address

clean_asan:
	rm -f server_test_asan client_test_asan

fresh_asan: clean_asan
	$(MAKE) asan

