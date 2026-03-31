#ifndef CLIENT_H
#define CLIENT_H

int connect_to_server(const char *host, int port);
int handle_server_message(int sockfd, int *my_player, int *current_turn);
void send_move(int sockfd, int col);
void send_quit(int sockfd);
void send_rematch_response(int sockfd, int want_rematch);
void send_chat_message(int sockfd, const char *message);

#endif
