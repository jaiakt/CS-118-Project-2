CC=gcc
CFLAGS=-I.
DEPS = utilities.h

build: server client ccserver

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server: server.o 
	gcc -o server server.o -I.
client: client.o 
	gcc -o client client.o -I.
ccserver: ccserver.o 
	gcc -o ccserver ccserver.o -I.

clean:
	rm server client *.o
