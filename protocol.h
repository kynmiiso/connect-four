#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// message header struct
typedef struct {
    int type;               // type of message
    int player;             // player id
    int length;             // length of payload (size in bytes)
} MsgHeader;

#define MSG_JOIN            1   // sent when a player wants to join the game
#define MSG_JOIN_DONE       2   // sent when the join is complete / player has successfully joined
#define MSG_WAIT            3   // sent when the client is told to wait for another player
#define MSG_START           4   // sent when the game is starting
#define MSG_MOVE            5   // sent when the player sends a move to the server
#define MSG_INVALID         6   // sent when the input from a player is invalid
#define MSG_BOARD           7   // sent when the board is to be broadcasted to the client
#define MSG_GAME_OVER       8   // sent when the game is over (a player has won / draw)
#define MSG_QUIT            9   // sent when a player quits or disconnects
#define MSG_REMATCH         10  // sent when the server prompts the player to request or confirm a rematch

#endif
