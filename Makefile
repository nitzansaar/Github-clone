CC=clang
CFLAGS=-Wall

all: serverM serverA serverR serverD client

serverM: serverM.c
	$(CC) $(CFLAGS) -o serverM serverM.c -pthread

serverA: serverA.c
	$(CC) $(CFLAGS) -o serverA serverA.c

serverR: serverR.c
	$(CC) $(CFLAGS) -o serverR serverR.c

serverD: serverD.c
	$(CC) $(CFLAGS) -o serverD serverD.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f serverM serverA serverR serverD client

kill:
	sudo pkill -f "server[MARD]" || true