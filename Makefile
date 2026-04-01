PORT = 67033
FLAGS = -DPORT=$(PORT) -Wall -Wextra -g

.PHONY: all server client clean rebuild
all: server client

server:
	gcc $(FLAGS) -o server server_main.c server.c game.c

client:
	gcc $(FLAGS) -o client client_main.c client.c game.c

rebuild: clean all

clean:
	rm -f server client server.exe client.exe
