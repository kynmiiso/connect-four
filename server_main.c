#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/types.h>
#include "server.h"
#include "protocol.h"
#include "game.h"

#ifndef PORT
#define PORT 30000
#endif

// track one game session
typedef struct {
    ClientState client1;
    ClientState client2;
    Game game;
    int state; // 0 = start round, 1 = in progress, 2 = waiting for rematch, -1 = end session
    int current_turn; // player id (1 or 2)
} GameSession;

#define MAX_SESSIONS 10
#define MAX_PENDING 10

int main() {
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    int listenfd = setup_server_socket(PORT);
    if (listenfd == -1) {
        perror("setup_server_socket");
        printf("Failed to set up server socket on port %d.\n", PORT);
        return 1;
    }

    printf("Server listening on port %d\n", PORT);

    // pending clients waiting for a partner
    ClientState pending[MAX_PENDING];
    int pending_count = 0;

    // active game sessions
    GameSession sessions[MAX_SESSIONS];
    int session_count = 0;
    int i;

    // initialize all active game sessions as empty
    for (i = 0; i < MAX_SESSIONS; i++) {
        sessions[i].client1.fd = -1;
        sessions[i].client2.fd = -1;
    }
    
    // initialize pending clients as empty
    for (i = 0; i < MAX_PENDING; i++) {
        pending[i].fd = -1;
    }

    while (1) {
        fd_set readfds, writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

        // add listen socket to read set
        FD_SET(listenfd, &readfds);
        int maxfd = listenfd;

        // add all pending clients to the read and write sets
        for (i = 0; i < pending_count; i++) {
            if (pending[i].fd >= 0) {
                FD_SET(pending[i].fd, &readfds);
                if (pending[i].wr_total > pending[i].wr_pos) {
                    FD_SET(pending[i].fd, &writefds);
                }
                if (pending[i].fd > maxfd) maxfd = pending[i].fd;
            }
        }

        // add active game sessions to read and write sets
        for (i = 0; i < session_count; i++) {
            GameSession *sess = &sessions[i];
            if (sess->client1.fd >= 0) {
                FD_SET(sess->client1.fd, &readfds);
                if (sess->client1.wr_total > sess->client1.wr_pos) {
                    FD_SET(sess->client1.fd, &writefds);
                }
                if (sess->client1.fd > maxfd) maxfd = sess->client1.fd;
            }
            if (sess->client2.fd >= 0) {
                FD_SET(sess->client2.fd, &readfds);
                if (sess->client2.wr_total > sess->client2.wr_pos) {
                    FD_SET(sess->client2.fd, &writefds);
                }
                if (sess->client2.fd > maxfd) maxfd = sess->client2.fd;
            }
        }

        // call select with a timeout of up to 1 second
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        // returns when at least one socket is ready to read or write
        int select_ret = select(maxfd + 1, &readfds, &writefds, NULL, &tv);
        if (select_ret < 0) {
            perror("select");
            break;
        }

        // accept all ready connections without blocking
        if (FD_ISSET(listenfd, &readfds)) {
            while (1) {
                int client_fd = accept_client_nb(listenfd);
                if (client_fd < 0) {
                    break;
                }

                if (pending_count >= MAX_PENDING) {
                    close(client_fd);
                    continue;
                }

                memset(&pending[pending_count], 0, sizeof(ClientState));
                pending[pending_count].fd = client_fd;
                pending[pending_count].player = 0;
                pending_count++;
            }
        }

        // process the pending clients
        for (i = 0; i < pending_count; i++) {
            if (pending[i].fd >= 0 &&
                FD_ISSET(pending[i].fd, &writefds) &&
                send_write_buffer(&pending[i]) == -1) {
                disconnect_client(&pending[i]);
            }

            if (pending[i].fd < 0) {
                if (i < pending_count - 1) {
                    memmove(&pending[i], &pending[i + 1],
                            (pending_count - i - 1) * sizeof(ClientState));
                }
                pending_count--;
                i--;
                continue;
            }

            if (FD_ISSET(pending[i].fd, &readfds)) {
                int ret = handle_join_nb(&pending[i]);
                if (ret == -1) {
                    // there is an error or invalid join, so remove the client from pending
                    if (i < pending_count - 1) {
                        memmove(&pending[i], &pending[i + 1],
                                (pending_count - i - 1) * sizeof(ClientState));
                    }
                    pending_count--;
                    i--;
                }
            }
        }

        // pair joined clients into game sessions
        for (i = 0; i + 1 < pending_count; i++) {
            if (pending[i].joined && pending[i + 1].joined && session_count < MAX_SESSIONS) {
                // pair two joined clients
                ClientState p1 = pending[i];
                ClientState p2 = pending[i + 1];
                // set player id 
                p1.player = 1;
                p2.player = 2;

                GameSession *sess = &sessions[session_count];
                sess->client1 = p1;
                sess->client2 = p2;
                sess->client1.rematch_ready = 0;
                sess->client2.rematch_ready = 0;
                sess->client1.want_rematch = 0;
                sess->client2.want_rematch = 0;
                sess->state = 0;
                sess->current_turn = 1;
                init_board(&sess->game);
                session_count++;

                // remove paired clients from the pending list
                memmove(&pending[i], &pending[i + 2],
                        (pending_count - i - 2) * sizeof(ClientState));
                pending_count -= 2;
                i--;  // adjust index since we removed two elements
            }
        }

        // process the active sessions
        for (i = 0; i < session_count; i++) {
            GameSession *sess = &sessions[i];

            // flush write buffers and treat write failures as disconnects
            if (sess->client1.fd >= 0 &&
                FD_ISSET(sess->client1.fd, &writefds) &&
                send_write_buffer(&sess->client1) == -1) {
                disconnect_client(&sess->client1);
                if (sess->client2.fd >= 0) {
                    send_message_nb(&sess->client2, MSG_QUIT, 1, NULL, 0);
                }
                sess->state = -1;
            }
            if (sess->state != -1 &&
                sess->client2.fd >= 0 &&
                FD_ISSET(sess->client2.fd, &writefds) &&
                send_write_buffer(&sess->client2) == -1) {
                disconnect_client(&sess->client2);
                if (sess->client1.fd >= 0) {
                    send_message_nb(&sess->client1, MSG_QUIT, 2, NULL, 0);
                }
                sess->state = -1;
            }

            // handle reads from clients
            int p1_ready = (sess->client1.fd >= 0 && FD_ISSET(sess->client1.fd, &readfds));
            int p2_ready = (sess->client2.fd >= 0 && FD_ISSET(sess->client2.fd, &readfds));

            if (sess->state == 0) {
                // send the start message to both players
                send_message_nb(&sess->client1, MSG_START, 1, NULL, 0);
                send_message_nb(&sess->client2, MSG_START, 2, NULL, 0);
                broadcast_board(&sess->client1, &sess->client2, &sess->game, 1);
                sess->state = 1;
                sess->current_turn = 1;
            }

            if (sess->state == 1) {
                // game is in progress
                ClientState *current_client = (sess->current_turn == 1) ? &sess->client1 : &sess->client2;
                ClientState *other_client = (sess->current_turn == 1) ? &sess->client2 : &sess->client1;

                if (p1_ready || p2_ready) {
                    ClientState *active = p1_ready ? &sess->client1 : &sess->client2;
                    int is_current = (active == current_client);

                    MsgHeader hdr;
                    char payload[MAX_CHAT_LEN];
                    int ret = read_message_nb(active, &hdr, payload, sizeof(payload));

                    if (ret == 0) {
                        // a complete message was received
                        if (hdr.type == MSG_QUIT) { // handle quit
                            send_message_nb(other_client, MSG_QUIT, active->player, NULL, 0);
                            disconnect_client(active);
                            sess->state = -1; // end session
                        } else if (hdr.type == MSG_CHAT) { // handle chat
                            payload[MAX_CHAT_LEN - 1] = '\0';
                            send_message_nb(other_client, MSG_CHAT, active->player, payload, hdr.length);
                        } else if (is_current && hdr.type == MSG_MOVE && hdr.length == sizeof(int)) { // handle movement
                            int col;
                            memcpy(&col, payload, sizeof(int));
                            int valid = handle_move(&sess->game, active->player, col);
                            if (!valid) {
                                send_message_nb(active, MSG_INVALID, active->player, NULL, 0);
                            } else {
                                // check for win
                                if (check_win(&sess->game, active->player)) {
                                    broadcast_board(&sess->client1, &sess->client2, &sess->game, 0);
                                    send_message_nb(&sess->client1, MSG_GAME_OVER, active->player, NULL, 0);
                                    send_message_nb(&sess->client2, MSG_GAME_OVER, active->player, NULL, 0);
                                    sess->client1.rematch_ready = 0;
                                    sess->client2.rematch_ready = 0;
                                    sess->client1.want_rematch = 0;
                                    sess->client2.want_rematch = 0;
                                    sess->state = 2;
                                // check for draw
                                } else if (check_draw(&sess->game)) {
                                    broadcast_board(&sess->client1, &sess->client2, &sess->game, 0);
                                    send_message_nb(&sess->client1, MSG_GAME_OVER, 0, NULL, 0);
                                    send_message_nb(&sess->client2, MSG_GAME_OVER, 0, NULL, 0);
                                    sess->client1.rematch_ready = 0;
                                    sess->client2.rematch_ready = 0;
                                    sess->client1.want_rematch = 0;
                                    sess->client2.want_rematch = 0;
                                    sess->state = 2;
                                } else {
                                    // switch turns of players
                                    sess->current_turn = (sess->current_turn == 1) ? 2 : 1;
                                    broadcast_board(&sess->client1, &sess->client2, &sess->game, sess->current_turn);
                                }
                            }
                        }
                    } else if (ret == -1) {
                        // display a connection error
                        disconnect_client(active);
                        send_message_nb(other_client, MSG_QUIT, active->player, NULL, 0);
                        sess->state = -1;
                    }
                }
            }

            if (sess->state == 2) {
                int rematch_ret = handle_rematch(&sess->client1, &sess->client2);
                if (rematch_ret == 0) {
                    init_board(&sess->game);
                    sess->current_turn = 1;
                    send_message_nb(&sess->client1, MSG_START, 1, NULL, 0);
                    send_message_nb(&sess->client2, MSG_START, 2, NULL, 0);
                    broadcast_board(&sess->client1, &sess->client2, &sess->game, sess->current_turn);
                    sess->state = 1;
                } else if (rematch_ret == -1) {
                    sess->state = -1;
                }
            }

            // clean up ended sessions
            if (sess->state == -1) {
                disconnect_client(&sess->client1);
                disconnect_client(&sess->client2);
                if (i < session_count - 1) {
                    memmove(&sessions[i], &sessions[i + 1],
                            (session_count - i - 1) * sizeof(GameSession));
                }
                session_count--;
                i--;
            }
        }
    }

    close(listenfd);
    return 0;
}