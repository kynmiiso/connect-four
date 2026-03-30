#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "server.h"

int setup_server_socket(int port) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(addr.sin_zero), 0, 8);

    int listen_soc = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_soc == -1) {
        return -1;
    }

    int opt = 1;
    setsockopt(listen_soc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_soc, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) == -1) {
        close(listen_soc);
        return -1;
    }

    if (listen(listen_soc, 5) == -1) {
        return -1;
    }

    return listen_soc;
}

void accept_client(int listenfd, int *client_fd) {
    *client_fd = accept(listenfd, NULL, NULL);
}

int read_message(int fd, MsgHeader *hdr, void *buf) {
    // read header with a fixed size
    int n = read(fd, hdr, sizeof(*hdr));
    if (n <= 0) {
        return -1;
    }

    // read the payload (if any)
    if (hdr->length > 0 && buf != NULL) {
        n = read(fd, buf, hdr->length);
        if (n <= 0) return -1;
    }
    return 0;
}

int send_message(int fd, int type, int player, const void *buf, int len) {
    MsgHeader hdr;
    hdr.type = type;
    hdr.length = len;
    hdr.player = player;

    if (write(fd, &hdr, sizeof(hdr)) == -1) {
        return -1;
    }

    if (len > 0 && buf != NULL) {
        if (write(fd, buf, len) == -1) {
            return -1;
        }
    }
    
    return 0;
}

int handle_join(int client_fd, int player) {
    MsgHeader hdr;

    // read the request to join from the client
    if (read_message(client_fd, &hdr, NULL) == -1 || hdr.type != MSG_JOIN) {
        disconnect_client(&client_fd);
        return -1;
    }

    // confirm the player number assigned by the server
    if (send_message(client_fd, MSG_JOIN_DONE, player, NULL, 0) == -1) {
        disconnect_client(&client_fd);
        return -1;
    }

    return 0;
}

int handle_move(Game *g, int client_fd, int player, int col) {
    int result = apply_move(g, player, col);
    if (result == 0) {
        // column is invalid or full, send message that it is an invalid move
        send_message(client_fd, MSG_INVALID, player, NULL, 0);
        return 0;
    }

    return 1;
}

void broadcast_board(int fd1, int fd2, const Game *g, int current_turn) {
    send_message(fd1, MSG_BOARD, current_turn, g->board, sizeof(g->board));
    send_message(fd2, MSG_BOARD, current_turn, g->board, sizeof(g->board));
}

void disconnect_client(int *fd) {
    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}
