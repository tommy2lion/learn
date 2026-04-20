/*
 * cube.c - Rubik's Cube state operations.
 *
 * Each base CW quarter turn is encoded as 5 four-cycles on the 54-sticker
 * array: 2 cycles internal to the rotating face (corners and edges) plus
 * 3 cycles on the "belt" of stickers from the 4 adjacent faces.
 *
 * CCW (X') and 180 (X2) moves are derived at runtime by applying the base
 * CW turn 3 or 2 times respectively. This means we only need to encode 6
 * permutation tables, not 18.
 */

#include "cube.h"
#include <stdlib.h>
#include <string.h>

#define S(face, idx) ((face) * 9 + (idx))

/* Each row: {a, b, c, d} means sticker at a moves to b, b to c, c to d,
 * d to a. (The cycle a -> b -> c -> d -> a.) */
static const int BASE_CW[6][5][4] = {
    /* U (top, CW seen from above) */
    {
        { S(U_FACE, 0), S(U_FACE, 2), S(U_FACE, 8), S(U_FACE, 6) },
        { S(U_FACE, 1), S(U_FACE, 5), S(U_FACE, 7), S(U_FACE, 3) },
        { S(F_FACE, 0), S(L_FACE, 0), S(B_FACE, 0), S(R_FACE, 0) },
        { S(F_FACE, 1), S(L_FACE, 1), S(B_FACE, 1), S(R_FACE, 1) },
        { S(F_FACE, 2), S(L_FACE, 2), S(B_FACE, 2), S(R_FACE, 2) },
    },
    /* D (bottom, CW seen from below) */
    {
        { S(D_FACE, 0), S(D_FACE, 2), S(D_FACE, 8), S(D_FACE, 6) },
        { S(D_FACE, 1), S(D_FACE, 5), S(D_FACE, 7), S(D_FACE, 3) },
        { S(F_FACE, 6), S(R_FACE, 6), S(B_FACE, 6), S(L_FACE, 6) },
        { S(F_FACE, 7), S(R_FACE, 7), S(B_FACE, 7), S(L_FACE, 7) },
        { S(F_FACE, 8), S(R_FACE, 8), S(B_FACE, 8), S(L_FACE, 8) },
    },
    /* L (left, CW seen from the left) */
    {
        { S(L_FACE, 0), S(L_FACE, 2), S(L_FACE, 8), S(L_FACE, 6) },
        { S(L_FACE, 1), S(L_FACE, 5), S(L_FACE, 7), S(L_FACE, 3) },
        { S(U_FACE, 0), S(F_FACE, 0), S(D_FACE, 0), S(B_FACE, 8) },
        { S(U_FACE, 3), S(F_FACE, 3), S(D_FACE, 3), S(B_FACE, 5) },
        { S(U_FACE, 6), S(F_FACE, 6), S(D_FACE, 6), S(B_FACE, 2) },
    },
    /* R (right, CW seen from the right) */
    {
        { S(R_FACE, 0), S(R_FACE, 2), S(R_FACE, 8), S(R_FACE, 6) },
        { S(R_FACE, 1), S(R_FACE, 5), S(R_FACE, 7), S(R_FACE, 3) },
        { S(U_FACE, 2), S(B_FACE, 6), S(D_FACE, 8), S(F_FACE, 2) },
        { S(U_FACE, 5), S(B_FACE, 3), S(D_FACE, 5), S(F_FACE, 5) },
        { S(U_FACE, 8), S(B_FACE, 0), S(D_FACE, 2), S(F_FACE, 8) },
    },
    /* F (front, CW seen from the front) */
    {
        { S(F_FACE, 0), S(F_FACE, 2), S(F_FACE, 8), S(F_FACE, 6) },
        { S(F_FACE, 1), S(F_FACE, 5), S(F_FACE, 7), S(F_FACE, 3) },
        { S(U_FACE, 6), S(R_FACE, 0), S(D_FACE, 2), S(L_FACE, 8) },
        { S(U_FACE, 7), S(R_FACE, 3), S(D_FACE, 1), S(L_FACE, 5) },
        { S(U_FACE, 8), S(R_FACE, 6), S(D_FACE, 0), S(L_FACE, 2) },
    },
    /* B (back, CW seen from the back) */
    {
        { S(B_FACE, 0), S(B_FACE, 2), S(B_FACE, 8), S(B_FACE, 6) },
        { S(B_FACE, 1), S(B_FACE, 5), S(B_FACE, 7), S(B_FACE, 3) },
        { S(U_FACE, 0), S(L_FACE, 6), S(D_FACE, 8), S(R_FACE, 2) },
        { S(U_FACE, 1), S(L_FACE, 3), S(D_FACE, 7), S(R_FACE, 5) },
        { S(U_FACE, 2), S(L_FACE, 0), S(D_FACE, 6), S(R_FACE, 8) },
    },
};

static void apply_cycle(unsigned char *s, const int c[4])
{
    unsigned char tmp = s[c[3]];
    s[c[3]] = s[c[2]];
    s[c[2]] = s[c[1]];
    s[c[1]] = s[c[0]];
    s[c[0]] = tmp;
}

static void apply_base_cw(Cube *c, int face)
{
    for (int i = 0; i < 5; i++) {
        apply_cycle(c->s, BASE_CW[face][i]);
    }
}

void cube_reset(Cube *c)
{
    for (int f = 0; f < 6; f++) {
        for (int i = 0; i < 9; i++) {
            c->s[f * 9 + i] = (unsigned char)f;
        }
    }
}

void cube_apply(Cube *c, int move)
{
    if (move < 0 || move >= M_COUNT) return;
    int face = move / 3;
    int kind = move % 3;
    int turns = (kind == 0) ? 1 : (kind == 1) ? 3 : 2;
    for (int t = 0; t < turns; t++) {
        apply_base_cw(c, face);
    }
}

void cube_apply_seq(Cube *c, const int *moves, int n)
{
    for (int i = 0; i < n; i++) cube_apply(c, moves[i]);
}

void cube_scramble(Cube *c, int n)
{
    int last_face = -1;
    for (int i = 0; i < n; i++) {
        int face;
        do { face = rand() % 6; } while (face == last_face);
        int kind = rand() % 3;
        cube_apply(c, face * 3 + kind);
        last_face = face;
    }
}

bool cube_is_solved(const Cube *c)
{
    for (int f = 0; f < 6; f++) {
        unsigned char color = c->s[f * 9];
        for (int i = 1; i < 9; i++) {
            if (c->s[f * 9 + i] != color) return false;
        }
    }
    return true;
}

static const char *MOVE_NAMES[M_COUNT] = {
    "U", "U'", "U2",
    "D", "D'", "D2",
    "L", "L'", "L2",
    "R", "R'", "R2",
    "F", "F'", "F2",
    "B", "B'", "B2",
};

const char *cube_move_name(int move)
{
    if (move < 0 || move >= M_COUNT) return "?";
    return MOVE_NAMES[move];
}

int cube_inverse(int move)
{
    if (move < 0 || move >= M_COUNT) return move;
    int face = move / 3;
    int kind = move % 3;
    int inv  = (kind == 0) ? 1 : (kind == 1) ? 0 : 2;
    return face * 3 + inv;
}
