all: server client 
server: server.o
	gcc server.o -o server

client: client.o
	gcc client.o -o client

server.o: server.c
	gcc -Wall -c server.c -o server.o

client.o: client.c
	gcc -Wall -c client.c -o client.o

clean:
	\rm -rf *.o *~
