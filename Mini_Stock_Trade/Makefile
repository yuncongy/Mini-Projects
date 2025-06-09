CC = gcc
CFLAGS = -Wall -g

# Targets
all: serverM serverA serverQ serverP client

serverM: serverM.c
	$(CC) $(CFLAGS) -o serverM serverM.c

serverA: serverA.c
	$(CC) $(CFLAGS) -o serverA serverA.c

serverQ: serverQ.c
	$(CC) $(CFLAGS) -o serverQ serverQ.c

serverP: serverP.c
	$(CC) $(CFLAGS) -o serverP serverP.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f serverM serverA serverQ serverP client
