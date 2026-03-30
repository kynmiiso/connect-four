#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "client.h"
#include "protocol.h"
#include "game.h"

int connect_to_server(const char *host, int port) {
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    server = gethostbyname(host);
    if (server == NULL) {
        return -1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int handle_server_message(int sockfd, int *my_player, int *current_turn) {
    MsgHeader hdr;
    char board[ROWS][COLS];

    if (read(sockfd, &hdr, sizeof(hdr)) <= 0) {
        printf("Disconnected from server.\n");
        return -1;
    }

    if (hdr.type == MSG_JOIN_DONE) {
        *my_player = hdr.player;
        printf("You have joined the game as player %d.\n", hdr.player);
    }
    else if (hdr.type == MSG_WAIT) {
        printf("Waiting for another player to join...\n");
    }
    else if (hdr.type == MSG_START) {
        *my_player = hdr.player;
        printf("Game has started! You are player %d.\n", hdr.player);
    }
    else if (hdr.type == MSG_BOARD) {
        if (read(sockfd, board, sizeof(board)) <= 0) {
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
    else if (hdr.type == MSG_INVALID) {
        printf("Invalid move! Please try again.\n");
    }
    else if (hdr.type == MSG_GAME_OVER) {
        if (hdr.player == 0) {
            printf("Game Over: It was a draw!\n");
        } else {
            printf("Game Over: Player %d wins!\n", hdr.player);
        }
        return 1;
    }
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

    write(sockfd, &hdr, sizeof(hdr));
    write(sockfd, &col, sizeof(int));
}
