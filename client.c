#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "client.h"
#include "protocol.h"
#include "game.h"

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

int connect_to_server(const char *host, int port) {
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    // look up server address from the host name
    server = gethostbyname(host);
    if (server == NULL) {
        return -1;
    }

    // create the socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return -1;
    }

    // set up server address
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    // connect to the server 
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int handle_server_message(int sockfd, int *my_player, int *current_turn) {
    MsgHeader hdr;
    char board[ROWS][COLS];
    int rematch = 0;

    // read the next message header from the server
    if (read_all(sockfd, &hdr, sizeof(hdr)) <= 0) {
        printf("Disconnected from server.\n");
        return -1;
    }

    // server lets player know their player id
    if (hdr.type == MSG_JOIN_DONE) {
        *my_player = hdr.player;
        printf("You have joined the game as player %d.\n", hdr.player);
    }
    // wait for second player to connect
    else if (hdr.type == MSG_WAIT) {
        printf("Waiting for another player to join...\n");
    }
    // start the game 
    else if (hdr.type == MSG_START) {
        *my_player = hdr.player;
        printf("Game has started! You are player %d.\n", hdr.player);
    }
    // broadcast the board
    else if (hdr.type == MSG_BOARD) {
        if (read_all(sockfd, board, sizeof(board)) <= 0) {
            printf("Disconnected from server.\n");
            return -1;
        }

        *current_turn = hdr.player;
        printf("\n");
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                printf("%c ", board[r][c]);
            }
            printf("\n");
        }
        if (hdr.player != 0) {
            printf("Turn: Player %d\n", hdr.player);
        }
    }
    // handle invalid moves
    else if (hdr.type == MSG_INVALID) {
        printf("Invalid move! Please try again.\n");
    }
    // handle quit / session ended
    else if (hdr.type == MSG_QUIT) {
        if (hdr.player != 0) {
            printf("Player %d quit the game.\n", hdr.player);
        } else {
            printf("The session has ended.\n");
        }
        return -1;
    }
    // game ended (win / draw)
    else if (hdr.type == MSG_GAME_OVER) {
        if (hdr.player == 0) {
            printf("Game Over: It was a draw!\n");
        } else {
            printf("Game Over: Player %d wins!\n", hdr.player);
        }
        return 1;
    }
    // server sends the result of the rematch decision
    else if (hdr.type == MSG_REMATCH) {
        if (hdr.length == sizeof(int)) {
            if (read_all(sockfd, &rematch, sizeof(int)) <= 0) {
                printf("Disconnected from server.\n");
                return -1;
            }
        }

        if (rematch) {
            printf("Both players accepted the rematch. Starting a new game!\n");
        } else {
            printf("Rematch declined. Ending session.\n");
            return -1;
        }
    }
    // handle chat messages from the other player
    else if (hdr.type == MSG_CHAT) {
        char message[MAX_CHAT_LEN];

        if (hdr.length <= 0 || hdr.length > MAX_CHAT_LEN) {
            printf("Received invalid chat message.\n");
            return 0;
        }

        if (read_all(sockfd, message, hdr.length) <= 0) {
            printf("Disconnected from server.\n");
            return -1;
        }

        message[MAX_CHAT_LEN - 1] = '\0';
        printf("Player %d said: %s\n", hdr.player, message);
    }
    // handle unexpected messages
    else {
        printf("Unknown message type: %d. Please type a valid message.\n", hdr.type);
    }

    return 0;
}

void send_move(int sockfd, int col) {
    MsgHeader hdr;
    hdr.type   = MSG_MOVE;
    hdr.player = 0;
    hdr.length = sizeof(int);

    // send the chosen move and column to the server
    write_all(sockfd, &hdr, sizeof(hdr));
    write_all(sockfd, &col, sizeof(int));
}

void send_quit(int sockfd) {
    MsgHeader hdr;
    hdr.type   = MSG_QUIT;
    hdr.player = 0;
    hdr.length = 0;

    // let the server know that the player quit
    write_all(sockfd, &hdr, sizeof(hdr));
}

void send_rematch_response(int sockfd, int want_rematch) {
    MsgHeader hdr;
    hdr.type   = MSG_REMATCH;
    hdr.player = 0;
    hdr.length = sizeof(int);

    // send rematch request decision 
    write_all(sockfd, &hdr, sizeof(hdr));
    write_all(sockfd, &want_rematch, sizeof(int));
}

void send_chat_message(int sockfd, const char *message) {
    MsgHeader hdr;
    char buffer[MAX_CHAT_LEN];
    int len;

    strncpy(buffer, message, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    len = (int)strlen(buffer) + 1;

    hdr.type   = MSG_CHAT;
    hdr.player = 0;
    hdr.length = len;

    write_all(sockfd, &hdr, sizeof(hdr));
    write_all(sockfd, buffer, len);
}
