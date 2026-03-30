#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include "protocol.h"
#include "game.h"

int setup_server_socket(int port);
void accept_client(int listenfd, int *client_fd);
int read_message(int fd, MsgHeader *hdr, void *buf);
int send_message(int fd, int type, int player, const void *buf, int len);
int handle_join(int client_fd, int player);
int handle_move(Game *g, int client_fd, int player, int col);
int handle_rematch(int fd1, int fd2);
void broadcast_board(int fd1, int fd2, const Game *g, int current_turn);
void disconnect_client(int *fd);

#endif