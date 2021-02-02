# Optimize, turn on additional warnings, link with librpma
CFLAGS=-O2 -g -Wall -Wextra -lrpma

.PHONY: all
all: test_server
test_server: test_server.c
	$(CC) -o $@ $^ $(CFLAGS) 
.PHONY: clean
clean:
	rm -f test_server

