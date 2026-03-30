#include <stdio.h>
#include <unistd.h>
#include "server.h"
#include "protocol.h"
#include "game.h"

#ifndef PORT
#define PORT 30000
#endif

int main() {
    int listenfd = setup_server_socket(PORT);
    if (listenfd == -1) {
        printf("Failed to set up server socket.\n");
        return 1;
    }
    printf("Server listening on port %d\n", PORT);

    while (1) {
        // wait for two players
        int fd1 = -1, fd2 = -1;

        printf("Waiting for player 1...\n");
        accept_client(listenfd, &fd1);
        if (handle_join(fd1, 1) == -1) {
            disconnect_client(&fd1);
            continue;
        }
        send_message(fd1, MSG_WAIT, 1, NULL, 0);

        printf("Waiting for player 2...\n");
        accept_client(listenfd, &fd2);
        if (handle_join(fd2, 2) == -1) {
            disconnect_client(&fd1);
            disconnect_client(&fd2);
            continue;
        }

        // send message to both players that the game is starting
        send_message(fd1, MSG_START, 1, NULL, 0);
        send_message(fd2, MSG_START, 2, NULL, 0);

        // initialize the board
        Game g;
        init_board(&g);
        broadcast_board(fd1, fd2, &g, 1);

        int fds[2] = {fd1, fd2};
        int current = 0;  // index into fds[], alternates 0 and 1

        while (1) {
            int player = current + 1;               // current player is 1 or 2
            int client_fd = fds[current];

            // read the move from the current player
            MsgHeader hdr;
            int col;
            if (read_message(client_fd, &hdr, &col) == -1 || hdr.type != MSG_MOVE) {
                printf("Player %d disconnected.\n", player);
                disconnect_client(&fds[current]);
                break;
            }

            if (handle_move(&g, client_fd, player, col) == 0) {
                continue;
            }

            if (check_win(&g, player)) {
                broadcast_board(fd1, fd2, &g, player);
                send_message(fd1, MSG_GAME_OVER, player, NULL, 0);
                send_message(fd2, MSG_GAME_OVER, player, NULL, 0);
                break;
            } else if (check_draw(&g)) {
                broadcast_board(fd1, fd2, &g, 0);
                send_message(fd1, MSG_GAME_OVER, 0, NULL, 0);
                send_message(fd2, MSG_GAME_OVER, 0, NULL, 0);
                break;
            }

            current = 1 - current;  // switch turns
            broadcast_board(fd1, fd2, &g, current + 1);
        }

        disconnect_client(&fd1);
        disconnect_client(&fd2);
    }

    close(listenfd);
    return 0;
}