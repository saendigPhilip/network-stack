# Optimize, make debuggable, turn on additional warnings, link with librpma
CFLAGS=-O2 -g -Wall -Wextra -lrpma

.PHONY: all
all: test_server test_client
test_server: test_server.c utils.c utils.h
	$(CC) -o $@ $^ $(CFLAGS)

test_client: test_client.c utils.c utils.h
	$(CC) -o $@ $^ $(CFLAGS)


.PHONY: clean
clean:
	rm -f test_server test_client

