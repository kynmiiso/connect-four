#include <stdio.h>
#include <unistd.h>
#include "client.h"
#include "protocol.h"

#ifndef PORT
#define PORT 30000
#endif

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
        handle_server_message(sockfd);

        // read the player id
        if (my_player == 0) {
            // player set inside handle_server_message — 
            // you may want to return player from it instead
        }

        // if it's this player's turn, prompt for a column
        if (current_turn == my_player) {
            int col;
            printf("Enter column from 0 - 6: ");
            scanf("%d", &col);
            send_move(sockfd, col);
        }
    }

    close(sockfd);
    return 0;
}