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

        while (1) {
            // send a message to both players that the game is starting
            send_message(fd1, MSG_START, 1, NULL, 0);
            send_message(fd2, MSG_START, 2, NULL, 0);

            // initialize the board
            Game g;
            init_board(&g);
            broadcast_board(fd1, fd2, &g, 1);

            int fds[2] = {fd1, fd2};
            int current = 0;
            int game_over = 0;
            int session_active = 1;

            while (1) {
                int player = current + 1;
                int other = 1 - current;
                int client_fd = fds[current];

                // read the move from the current player
                MsgHeader hdr;
                int col;
                if (read_message(client_fd, &hdr, &col) == -1) {
                    printf("Player %d disconnected.\n", player);
                    send_message(fds[other], MSG_QUIT, player, NULL, 0);
                    session_active = 0;
                    break;
                }

                if (hdr.type == MSG_QUIT) {
                    printf("Player %d quit the game.\n", player);
                    send_message(fds[other], MSG_QUIT, player, NULL, 0);
                    session_active = 0;
                    break;
                }

                if (hdr.type != MSG_MOVE) {
                    send_message(client_fd, MSG_INVALID, player, NULL, 0);
                    continue;
                }

                if (handle_move(&g, client_fd, player, col) == 0) {
                    continue;
                }

                if (check_win(&g, player)) {
                    broadcast_board(fd1, fd2, &g, 0);
                    send_message(fd1, MSG_GAME_OVER, player, NULL, 0);
                    send_message(fd2, MSG_GAME_OVER, player, NULL, 0);
                    game_over = 1;
                    break;
                } else if (check_draw(&g)) {
                    broadcast_board(fd1, fd2, &g, 0);
                    send_message(fd1, MSG_GAME_OVER, 0, NULL, 0);
                    send_message(fd2, MSG_GAME_OVER, 0, NULL, 0);
                    game_over = 1;
                    break;
                }

                current = other;
                broadcast_board(fd1, fd2, &g, current + 1);
            }

            if (!session_active) {
                break;
            }

            if (!game_over) {
                break;
            }

            int rematch = handle_rematch(fd1, fd2);
            if (rematch == 1) {
                int yes = 1;
                send_message(fd1, MSG_REMATCH, 0, &yes, sizeof(int));
                send_message(fd2, MSG_REMATCH, 0, &yes, sizeof(int));
                continue;
            }

            if (rematch == -1) {
                printf("A player disconnected during the rematch prompt.\n");
                send_message(fd1, MSG_QUIT, 0, NULL, 0);
                send_message(fd2, MSG_QUIT, 0, NULL, 0);
            } else {
                int no = 0;
                send_message(fd1, MSG_REMATCH, 0, &no, sizeof(int));
                send_message(fd2, MSG_REMATCH, 0, &no, sizeof(int));
            }
            break;
        }

        // disconnect both clients before waiting for the next pair
        disconnect_client(&fd1);
        disconnect_client(&fd2);
    }

    // close the server socket
    close(listenfd);
    return 0;
}