#ifndef GAME_H
#define GAME_H

#define ROWS 6
#define COLS 7

typedef struct {
    // '.' means cell is empty, 'X' is for player 1, 'O' is for player 2
    char board[ROWS][COLS];  
} Game;

void init_board(Game *g);
int apply_move(Game *g, int player, int col);
int check_win(const Game *g, int player);
int check_draw(const Game *g);

#endif