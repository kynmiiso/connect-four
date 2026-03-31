.PHONY: all server client clean rebuild

all: server client

server:
	gcc -Wall -g -o server server_main.c server.c game.c

client:
	gcc -Wall -g -o client client_main.c client.c game.c

rebuild: clean all

clean:
	rm -f server client server.exe client.exe
	