# Optimize, make debuggable, turn on additional warnings, link with librpma
CFLAGS=-O2 -g -Wall -Wextra -lrpma

.PHONY: all
all: simple_server simple_client
simple_server: simple_server.c client_server_utils.c client_server_utils.h
	$(CC) -o $@ $^ $(CFLAGS)

simple_client: simple_client.c client_server_utils.c client_server_utils.h
	$(CC) -o $@ $^ $(CFLAGS)


.PHONY: clean
clean:
	rm -f simple_server simple_client

