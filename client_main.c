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
        printf("Enter column from 0 - %d (or q to quit): ", COLS - 1);

        // read the player's input
        if (fgets(line, sizeof(line), stdin) == NULL) {
            return -1;
        }

        // handle player quitting by pressing q
        if (line[0] == 'q' || line[0] == 'Q') {
            return -1;
        }

        // convert the input string to a number
        col = strtol(line, &endptr, 10);
        if (endptr == line) {
            printf("Please enter a number.\n");
            continue;
        }

        // skip spaces after the number
        while (*endptr == ' ' || *endptr == '\t') {
            endptr++;
        }

        // handle extra characters
        if (*endptr != '\n' && *endptr != '\0') {
            printf("Please enter just one number.\n");
            continue;
        }

        // make sure the column is within the board
        if (col < 0 || col >= COLS) {
            printf("Column must be between 0 and %d.\n", COLS - 1);
            continue;
        }

        return (int)col;
    }
}

static int prompt_for_rematch(void) {
    char line[100];

    while (1) {
        printf("Play again? (y/n): ");

        // read the player's rematch response 
        if (fgets(line, sizeof(line), stdin) == NULL) {
            return -1;
        }

        // yes -> play another game
        if (line[0] == 'y' || line[0] == 'Y') {
            return 1;
        }

        // no -> end the game session
        if (line[0] == 'n' || line[0] == 'N') {
            return 0;
        }

        // reject input besides y or n
        printf("Please enter y or n.\n");
    }
}

int main(int argc, char *argv[]) {
    // disable the default buffering (each character is printed to stdout as soon as printf is called)
    setbuf(stdout, NULL);

    // if the user did not provide exactly one argument along with ./client, print a usage message
    if (argc != 2) {
        printf("Usage: %s <host>\n", argv[0]);
        return 1;
    }

    // handle failed connect to server
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
        if (status == -1) {
            break;
        }

        // handle prompting for a rematch
        if (status == 1) {
            int want_rematch = prompt_for_rematch();
            if (want_rematch == -1) {
                send_quit(sockfd);
                break;
            }
            send_rematch_response(sockfd, want_rematch);
            continue;
        }

        // if it's this player's turn, prompt their input for a column index
        if (my_player != 0 && current_turn == my_player) {
            int col = prompt_for_column();
            if (col == -1) {
                send_quit(sockfd);
                break;
            }
            send_move(sockfd, col);
        }
    }

    close(sockfd);
    return 0;
}