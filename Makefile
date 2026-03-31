all: server client

server: server_main.c server.c game.c
	gcc -Wall -g -o server server_main.c server.c game.c

client: client_main.c client.c game.c
	gcc -Wall -g -o client client_main.c client.c game.c
	