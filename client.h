#ifndef CLIENT_H
#define CLIENT_H

int connect_to_server(const char *host, int port);
void handle_server_message(int sockfd);
void send_move(int sockfd, int col);

#endif
