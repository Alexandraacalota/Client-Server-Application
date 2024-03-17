CFLAGS = -Wall -g

# Server port
PORT = 12345

# Server IP address
IP_SERVER = 127.0.0.1

all: server subscriber

struct.o: struct.c

server: server.c struct.o
	gcc $(CFLAGS) -o server server.c struct.o

subscriber: client.c struct.o
	gcc $(CFLAGS) -o subscriber client.c struct.o -lm

.PHONY: clean run_server run_client

run_server:
	./server ${IP_SERVER} ${PORT}

run_client:
	./subscriber ${IP_SERVER} ${PORT}

clean:
	rm -rf server subscriber *.o *.dSYM
