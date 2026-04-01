#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include "server.h"

// make a socket non blocking
void make_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
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
        close(listen_soc);
        return -1;
    }

    make_nonblocking(listen_soc);
    return listen_soc;
}

// accept a client connection that is non blocking
int accept_client_nb(int listenfd) {
    int client_fd = accept(listenfd, NULL, NULL);
    if (client_fd == -1) {
        return -1;
    }
    make_nonblocking(client_fd);
    return client_fd;
}

// sends pending write data to the client without blocking
int send_write_buffer(ClientState *client) {
    ssize_t n;
    int to_write;

    if (client->fd < 0) {
        return -1;
    }

    if (client->wr_pos >= client->wr_total) {
        client->wr_pos = 0;
        client->wr_total = 0;
        return 0;
    }

    to_write = client->wr_total - client->wr_pos;
    if (to_write <= 0) {
        client->wr_pos = 0;
        client->wr_total = 0;
        return 0;
    }

#ifdef MSG_NOSIGNAL
    n = send(client->fd, client->wr_buf + client->wr_pos, (size_t)to_write, MSG_NOSIGNAL);
#else
    n = write(client->fd, client->wr_buf + client->wr_pos, (size_t)to_write);
#endif

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 1;
        }
        return -1;
    }

    if (n == 0) {
        return -1;
    }

    client->wr_pos += (int)n;
    if (client->wr_pos >= client->wr_total) {
        client->wr_pos = 0;
        client->wr_total = 0;
        return 0;
    }

    return 1;
}

// read a message without blocking
// returns 0 if a full message was received, 1 if more data is needed, and -1 on error
int read_message_nb(ClientState *client, MsgHeader *hdr, void *buf, int buf_size) {
    int total_needed;

    if (client->fd < 0) {
        return -1;
    }

    if (client->rd_pos < (int)sizeof(MsgHeader)) {
        int n = (int)read(client->fd,
                          client->rd_buf + client->rd_pos,
                          sizeof(MsgHeader) - client->rd_pos);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 1;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        client->rd_pos += n;
    }

    if (client->rd_pos < (int)sizeof(MsgHeader)) {
        return 1;
    }

    memcpy(hdr, client->rd_buf, sizeof(MsgHeader));
    if (hdr->length < 0 || hdr->length > (int)(sizeof(client->rd_buf) - sizeof(MsgHeader))) {
        return -1;
    }

    total_needed = (int)sizeof(MsgHeader) + hdr->length;
    if (client->rd_pos < total_needed) {
        int n = (int)read(client->fd,
                          client->rd_buf + client->rd_pos,
                          total_needed - client->rd_pos);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 1;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        client->rd_pos += n;
    }

    if (client->rd_pos < total_needed) {
        return 1;
    }

    if (hdr->length > 0 && buf != NULL && buf_size > 0) {
        int copy_len = hdr->length;
        if (copy_len > buf_size) {
            copy_len = buf_size;
        }
        memcpy(buf, client->rd_buf + sizeof(MsgHeader), copy_len);
    }

    client->rd_pos = 0;
    return 0;
}

// queue a message and attempt to flush it without blocking
int send_message_nb(ClientState *client, int type, int player, const void *buf, int len) {
    MsgHeader hdr;
    int total_size;
    int pending;

    if (client->fd < 0 || len < 0) {
        return -1;
    }

    hdr.type = type;
    hdr.player = player;
    hdr.length = len;
    total_size = (int)sizeof(hdr) + len;

    if (total_size > (int)sizeof(client->wr_buf)) {
        return -1;
    }

    pending = client->wr_total - client->wr_pos;
    if (pending < 0) {
        client->wr_pos = 0;
        client->wr_total = 0;
        pending = 0;
    }

    if (pending > 0 && client->wr_pos > 0) {
        memmove(client->wr_buf, client->wr_buf + client->wr_pos, pending);
        client->wr_pos = 0;
        client->wr_total = pending;
    } else if (pending == 0) {
        client->wr_pos = 0;
        client->wr_total = 0;
    }

    if (client->wr_total + total_size > (int)sizeof(client->wr_buf)) {
        return -1;
    }

    memcpy(client->wr_buf + client->wr_total, &hdr, sizeof(hdr));
    if (len > 0 && buf != NULL) {
        memcpy(client->wr_buf + client->wr_total + sizeof(hdr), buf, len);
    }
    client->wr_total += total_size;

    return (send_write_buffer(client) == -1) ? -1 : 0;
}

int handle_join_nb(ClientState *client) {
    MsgHeader hdr;
    int result = read_message_nb(client, &hdr, NULL, 0);

    if (result == 1) {
        return 1;
    }

    if (result == -1 || hdr.type != MSG_JOIN) {
        disconnect_client(client);
        return -1;
    }

    if (send_message_nb(client, MSG_JOIN_DONE, client->player, NULL, 0) == -1) {
        disconnect_client(client);
        return -1;
    }

    client->joined = 1;
    return 0;
}

int handle_move(Game *g, int player, int col) {
    int result = apply_move(g, player, col);
    return result; // 1 if it is a valid move, 0 if invalid
}

// handle a rematch without blocking; returns 0 if both players accepted,
// 1 while waiting for more responses, and -1 if a player declines/disconnects.
int handle_rematch(ClientState *client1, ClientState *client2) {
    ClientState *clients[2] = {client1, client2};
    int i;

    for (i = 0; i < 2; i++) {
        ClientState *client = clients[i];
        ClientState *other = clients[1 - i];

        if (client->fd < 0) {
            return -1;
        }

        if (!client->rematch_ready) {
            MsgHeader hdr;
            int want_rematch = 0;
            int ret = read_message_nb(client, &hdr, &want_rematch, sizeof(want_rematch));

            if (ret == -1) {
                if (other->fd >= 0) {
                    send_message_nb(other, MSG_QUIT, client->player, NULL, 0);
                }
                disconnect_client(client);
                return -1;
            }

            if (ret == 0) {
                if (hdr.type == MSG_QUIT) {
                    if (other->fd >= 0) {
                        send_message_nb(other, MSG_QUIT, client->player, NULL, 0);
                    }
                    disconnect_client(client);
                    return -1;
                }

                if (hdr.type != MSG_REMATCH || hdr.length != (int)sizeof(int)) {
                    if (other->fd >= 0) {
                        send_message_nb(other, MSG_QUIT, client->player, NULL, 0);
                    }
                    disconnect_client(client);
                    return -1;
                }

                client->want_rematch = (want_rematch != 0);
                client->rematch_ready = 1;
            }
        }
    }

    if ((client1->rematch_ready && !client1->want_rematch) ||
        (client2->rematch_ready && !client2->want_rematch)) {
        int approved = 0;
        send_message_nb(client1, MSG_REMATCH, 0, &approved, sizeof(approved));
        send_message_nb(client2, MSG_REMATCH, 0, &approved, sizeof(approved));
        client1->rematch_ready = 0;
        client2->rematch_ready = 0;
        client1->want_rematch = 0;
        client2->want_rematch = 0;
        return -1;
    }

    if (client1->rematch_ready && client2->rematch_ready) {
        int approved = 1;
        send_message_nb(client1, MSG_REMATCH, 0, &approved, sizeof(approved));
        send_message_nb(client2, MSG_REMATCH, 0, &approved, sizeof(approved));
        client1->rematch_ready = 0;
        client2->rematch_ready = 0;
        client1->want_rematch = 0;
        client2->want_rematch = 0;
        return 0;
    }

    return 1;
}

void broadcast_board(ClientState *client1, ClientState *client2, const Game *g, int current_turn) {
    send_message_nb(client1, MSG_BOARD, current_turn, g->board, sizeof(g->board));
    send_message_nb(client2, MSG_BOARD, current_turn, g->board, sizeof(g->board));
}

void disconnect_client(ClientState *client) {
    if (client->fd >= 0) {
        close(client->fd);
    }

    client->fd = -1;
    client->player = 0;
    client->rd_pos = 0;
    client->wr_pos = 0;
    client->wr_total = 0;
    client->rematch_ready = 0;
    client->want_rematch = 0;
    client->joined = 0;
}
