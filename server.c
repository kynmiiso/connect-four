#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "server.h"

static int read_all(int fd, void *buf, int count) {
    int total = 0;
    char *ptr = buf;

    while (total < count) {
        int n = (int)read(fd, ptr + total, count - total);
        if (n <= 0) {
            return n;
        }
        total += n;
    }

    return total;
}

static int write_all(int fd, const void *buf, int count) {
    int total = 0;
    const char *ptr = buf;

    while (total < count) {
        int n = (int)write(fd, ptr + total, count - total);
        if (n <= 0) {
            return n;
        }
        total += n;
    }

    return total;
}

// setup the socket for the server
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

int read_message(int fd, MsgHeader *hdr, void *buf, int buf_size) {
    // read header with a fixed size
    if (read_all(fd, hdr, sizeof(*hdr)) <= 0) {
        return -1;
    }

    // read the payload (if any)
    if (hdr->length > 0) {
        if (buf == NULL || hdr->length > buf_size) {
            return -1;
        }

        if (read_all(fd, buf, hdr->length) <= 0) {
            return -1;
        }
    }
    return 0;
}

int send_message(int fd, int type, int player, const void *buf, int len) {
    // header setup
    MsgHeader hdr;
    hdr.type = type;
    hdr.length = len;
    hdr.player = player;

    // write message to the client specified
    if (write_all(fd, &hdr, sizeof(hdr)) <= 0) {
        return -1;
    }

    if (len > 0 && buf != NULL) {
        if (write_all(fd, buf, len) <= 0) {
            return -1;
        }
    }
    
    return 0;
}

int handle_join(int client_fd, int player) {
    MsgHeader hdr;

    // read the request to join from the client
    if (read_message(client_fd, &hdr, NULL, 0) == -1 || hdr.type != MSG_JOIN) {
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
        // column is invalid or full, so send message that it is an invalid move
        send_message(client_fd, MSG_INVALID, player, NULL, 0);
        return 0;
    }

    return 1;
}

// handle a rematch 
int handle_rematch(int fd1, int fd2) {
    MsgHeader hdr;

    int want1 = 0; // want1 = 1 => player 1 wants a rematch
    int want2 = 0; // want2 = 1 => player 2 wants a rematch

    // read player 1's response
    if (read_message(fd1, &hdr, &want1, sizeof(want1)) == -1) {
        return -1; 
    }
    // handle player 1 quit
    if (hdr.type == MSG_QUIT) {
        return 0;
    }
    // if the message from player 1 is not a valid rematch response then no rematch
    if (hdr.type != MSG_REMATCH || hdr.length != sizeof(int)) {
        return 0;
    }
    // read player 2's response
    if (read_message(fd2, &hdr, &want2, sizeof(want2)) == -1) {
        return -1;
    }
    // handle player 2 quit
    if (hdr.type == MSG_QUIT) {
        return 0;
    }
    // if the message from player 2 is not a valid rematch response then no rematch
    if (hdr.type != MSG_REMATCH || hdr.length != sizeof(int)) {
        return 0;
    }

    return want1 && want2;
}

void broadcast_board(int fd1, int fd2, const Game *g, int current_turn) {
    // broadcast the board to both client streams
    send_message(fd1, MSG_BOARD, current_turn, g->board, sizeof(g->board));
    send_message(fd2, MSG_BOARD, current_turn, g->board, sizeof(g->board));
}

void disconnect_client(int *fd) {
    // handle client disconnection
    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}
