CC=gcc
CFLAGS=-I.
DEPS = client_server.h

all: server client

server:server.c
	$(CC) -g -std=gnu99 -pthread -o server server.c $(CFLAGS)

client:client.c
	$(CC) -g -std=gnu99 -pthread -o client client.c $(CFLAGS)

clean:
	rm  server client
