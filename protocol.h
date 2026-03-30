#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// message header struct
typedef struct {
    int type;           // type of message
    int player;         // player id
    int length;         // length of payload (size in bytes)
} MsgHeader;

#define MSG_JOIN            1
#define MSG_JOIN_DONE       2
#define MSG_WAIT            3
#define MSG_START           4
#define MSG_MOVE            5
#define MSG_INVALID         6
#define MSG_BOARD           7
#define MSG_GAME_OVER       8

#endif
