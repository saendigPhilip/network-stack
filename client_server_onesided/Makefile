# Turn on additional warnings, link with librpma, openssl
CFLAGS=-Wall -Wextra -Wpedantic -lrpma -lssl -lcrypto 
UTILS=onesided_global.h client_server_utils.c client_server_utils.h  onesided_test_cli.c onesided_test_cli.h
SERVER=server_test_cli.c onesided_server.c onesided_server.h onesided_test_all.c
CLIENT=client_test_cli.c onesided_client.c onesided_client.h onesided_test_all.c
SERVER_TEST=onesided_server.c onesided_server.h onesided_test_server.c simple_unit_test.h onesided_test_all.c
CLIENT_TEST=onesided_client.c onesided_client.h onesided_test_client.c simple_unit_test.h onesided_test_all.c

.PHONY: all clean_cli fresh_cli test clean_test fresh_test test_asan clean_test_asan fresh_test_asan
all: server_test_cli client_test_cli
server_test_cli: 
	$(CC) $(SERVER) $(UTILS) -o $@ $^ -g $(CFLAGS)

client_test_cli: 
	$(CC) $(CLIENT) $(UTILS) -o $@ $^ -g $(CFLAGS)

clean_cli:
	rm -f server_test_cli client_test_cli

fresh_cli: clean_cli
	$(MAKE) all


test: server_test client_test
server_test:
	$(CC) $(SERVER_TEST) $(UTILS) -o $@ $^ -O2 $(CFLAGS)

client_test:
	$(CC) $(CLIENT_TEST) $(UTILS) -o $@ $^ -O2 $(CFLAGS)

clean_test:
	rm -f server_test client_test

fresh_test: clean_test
	$(MAKE) test


test_asan:
	$(CC) $(TEST) $(UTILS) -o $@ $^ $(CFLAGS) -g -fsanitize=address

clean_test_asan:
	rm -f test_asan

fresh_test_asan: clean_test_asan
	$(MAKE) test_asan

