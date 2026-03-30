#include <stdio.h>
#include "game.h"

static char get_piece(int player);

void init_board(Game *g) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            g->board[r][c] = '.';
        }
    }
}

int apply_move(Game *g, int player, int col) {
    if (col < 0 || col >= COLS) return 0;

    char piece = get_piece(player);

    // start from the bottom row and go up, checking each cell in the column
    for (int r = ROWS - 1; r >= 0; r--) {
        if (g->board[r][col] == '.') {
            g->board[r][col] = piece;
            return 1; // mark the piece 
        }
    }

    // the column is already full
    return 0;
}

int check_win(const Game *g, int player) {
    // return 1 if game is won 

    char piece = get_piece(player);

    // check horizontal 
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c <= COLS - 4; c++) {
            if (g->board[r][c] == piece &&
                g->board[r][c+1] == piece &&
                g->board[r][c+2] == piece &&
                g->board[r][c+3] == piece) {
                return 1;
            }
        }
    }

    // check vertical 
    for (int r = 0; r <= ROWS - 4; r++) {
        for (int c = 0; c < COLS; c++) {
            if (g->board[r][c] == piece &&
                g->board[r+1][c] == piece &&
                g->board[r+2][c] == piece &&
                g->board[r+3][c] == piece) {
                return 1;
            }
        }
    }

    // check diagonal down right
    for (int r = 0; r <= ROWS - 4; r++) {
        for (int c = 0; c <= COLS - 4; c++) {
            if (g->board[r][c] == piece &&
                g->board[r+1][c+1] == piece &&
                g->board[r+2][c+2] == piece &&
                g->board[r+3][c+3] == piece) {
                return 1;
            }
        }
    }


    // check diagonal down left
    for (int r = 0; r <= ROWS - 4; r++) {
        for (int c = 3; c < COLS; c++) {
            if (g->board[r][c] == piece &&
                g->board[r+1][c-1] == piece &&
                g->board[r+2][c-2] == piece &&
                g->board[r+3][c-3] == piece) {
                return 1;
            }
        }
    }

    // return 0 if game is lost
    return 0;
}

// draw == no empty spaces left
int check_draw(const Game *g) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (g->board[r][c] == '.') {
                return 0; // there is still space on the board
            }
        }
    }
    return 1; // there is no more space on the board
}

// helper function to get piece based on player code
char get_piece(int player) {
    if (player == 1) {
        return 'X';
    }
    return 'O';
}
