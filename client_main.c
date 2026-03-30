#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "client.h"
#include "protocol.h"
#include "game.h"

#ifndef PORT
#define PORT 30000
#endif

static int prompt_for_column(void) {
    char line[100];
    char *endptr;
    long col;

    while (1) {
        printf("Enter column from 0 - %d: ", COLS - 1);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            return -1;
        }

        col = strtol(line, &endptr, 10);
        if (endptr == line) {
            printf("Please enter a number.\n");
            continue;
        }

        while (*endptr == ' ' || *endptr == '\t') {
            endptr++;
        }

        if (*endptr != '\n' && *endptr != '\0') {
            printf("Please enter just one number.\n");
            continue;
        }

        if (col < 0 || col >= COLS) {
            printf("Column must be between 0 and %d.\n", COLS - 1);
            continue;
        }

        return (int)col;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <host>\n", argv[0]);
        return 1;
    }

    int sockfd = connect_to_server(argv[1], PORT);
    if (sockfd == -1) {
        printf("Failed to connect to server.\n");
        return 1;
    }

    // send a join request
    MsgHeader hdr;
    hdr.type   = MSG_JOIN;
    hdr.player = 0;
    hdr.length = 0;
    write(sockfd, &hdr, sizeof(hdr));

    // main loop to listen for server messages and send moves
    int my_player = 0;
    int current_turn = 0;

    while (1) {
        int status = handle_server_message(sockfd, &my_player, &current_turn);
        if (status != 0) {
            break;
        }

        // if it's this player's turn, prompt for a column
        if (my_player != 0 && current_turn == my_player) {
            int col = prompt_for_column();
            if (col == -1) {
                break;
            }
            send_move(sockfd, col);
        }
    }

    close(sockfd);
    return 0;
}