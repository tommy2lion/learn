/*
 * cube.h - Rubik's Cube data model (sticker representation)
 *
 * Sticker indexing convention:
 *   Each face has 9 stickers indexed 0..8 in row-major order, viewed from
 *   outside that face. The full cube state is a 54-element array where
 *   s[face*9 + idx] holds the color at that position.
 *
 *   Face indices: U=0 (Up), D=1 (Down), L=2 (Left), R=3 (Right),
 *                 F=4 (Front), B=5 (Back).
 *
 *   Orientation of each face (what "up" and "right" mean in the 3x3 grid):
 *     U: viewed from +y looking down. "Up" in view = -z (back). "Right" = +x.
 *     D: viewed from -y looking up.   "Up" in view = +z (front). "Right" = +x.
 *     F: viewed from +z.              "Up" = +y. "Right" = +x.
 *     B: viewed from -z.              "Up" = +y. "Right" = -x.
 *     L: viewed from -x.              "Up" = +y. "Right" = +z (front).
 *     R: viewed from +x.              "Up" = +y. "Right" = -z (back).
 *
 *   With this convention, a solved U face has U[0] at the back-left corner
 *   (world position x=-1, z=-1) and U[8] at the front-right corner.
 *
 * Move encoding:
 *   An int in [0, 18). move = face*3 + kind, where kind 0=CW, 1=CCW, 2=180.
 */

#ifndef CUBE_H
#define CUBE_H

#include <stdbool.h>

enum { U_FACE = 0, D_FACE = 1, L_FACE = 2, R_FACE = 3, F_FACE = 4, B_FACE = 5 };

enum { CUBE_WHITE = 0, CUBE_YELLOW = 1, CUBE_ORANGE = 2,
       CUBE_RED   = 3, CUBE_GREEN  = 4, CUBE_BLUE   = 5 };

enum {
    M_U = 0,  M_Up,  M_U2,
    M_D,      M_Dp,  M_D2,
    M_L,      M_Lp,  M_L2,
    M_R,      M_Rp,  M_R2,
    M_F,      M_Fp,  M_F2,
    M_B,      M_Bp,  M_B2,
    M_COUNT
};

typedef struct {
    unsigned char s[54];
} Cube;

/* Reset to the solved state: each face f filled with color f. */
void cube_reset(Cube *c);

/* Apply a single move (0..17). See M_* enum. */
void cube_apply(Cube *c, int move);

/* Apply a sequence of moves. */
void cube_apply_seq(Cube *c, const int *moves, int n);

/* Apply n random moves, avoiding two consecutive moves on the same face.
 * Caller is responsible for seeding rand() if repeatability is desired. */
void cube_scramble(Cube *c, int n);

/* True iff every face has a single uniform color. */
bool cube_is_solved(const Cube *c);

/* Printable name of a move (e.g. "U", "R'", "F2"). */
const char *cube_move_name(int move);

/* The inverse of a move: X -> X', X' -> X, X2 -> X2. */
int cube_inverse(int move);

#endif /* CUBE_H */
