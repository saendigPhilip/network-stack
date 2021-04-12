# Optimize, make debuggable, turn on additional warnings, link with librpma
CFLAGS=-O2 -g -Wall -Wextra -lrpma

.PHONY: all
all: server client
server: server.c client_server_utils.c client_server_utils.h
	$(CC) -o $@ $^ $(CFLAGS)

client: client.c client_server_utils.c client_server_utils.h
	$(CC) -o $@ $^ $(CFLAGS)


.PHONY: clean
clean:
	rm -f server client

