#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include "protocol.h"
#include "game.h"

#define CLIENT_BUFFER_SIZE 2048

// client state to manage buffered I/O in non-blocking mode
typedef struct {
    int fd;                             // client socket fd
    int player;                         // player number (1 or 2)
    char rd_buf[CLIENT_BUFFER_SIZE];    // read buffer for incomplete messages
    int rd_pos;                         // bytes read so far in rd_buf
    char wr_buf[CLIENT_BUFFER_SIZE];    // queued outgoing data
    int wr_pos;                         // bytes already written from wr_buf
    int wr_total;                       // total queued bytes in wr_buf
    int rematch_ready;                  // 1 once this client replied about rematch
    int want_rematch;                   // 1 if client wants rematch, 0 otherwise
    int joined;                         // 1 if client has completed join handshake
} ClientState;

int setup_server_socket(int port);
int accept_client_nb(int listenfd);
int read_message_nb(ClientState *client, MsgHeader *hdr, void *buf, int buf_size);
int send_message_nb(ClientState *client, int type, int player, const void *buf, int len);
int handle_join_nb(ClientState *client);
int handle_move(Game *g, int player, int col);
int handle_rematch(ClientState *client1, ClientState *client2);
void broadcast_board(ClientState *client1, ClientState *client2, const Game *g, int current_turn);
void disconnect_client(ClientState *client);
void make_nonblocking(int fd);
int send_write_buffer(ClientState *client);

#endif