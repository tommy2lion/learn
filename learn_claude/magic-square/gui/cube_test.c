/*
 * cube_test.c - Sanity tests for the cube data model.
 *
 * Exit code is the number of failures (0 = all pass), so this can be
 * wired into a build pipeline.
 */

#include "cube.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int total    = 0;

static void check(int cond, const char *msg)
{
    total++;
    if (cond) {
        printf("  PASS  %s\n", msg);
    } else {
        printf("  FAIL  %s\n", msg);
        failures++;
    }
}

static int cubes_equal(const Cube *a, const Cube *b)
{
    return memcmp(a->s, b->s, 54) == 0;
}

int main(void)
{
    Cube c, solved;
    char buf[128];

    cube_reset(&solved);

    printf("== reset / is_solved ==\n");
    cube_reset(&c);
    check(cube_is_solved(&c), "fresh cube reports solved");

    /* Each face: X^4 = identity */
    printf("\n== X^4 = identity ==\n");
    for (int face = 0; face < 6; face++) {
        cube_reset(&c);
        for (int i = 0; i < 4; i++) cube_apply(&c, face * 3);
        snprintf(buf, sizeof buf, "%s applied 4x returns to solved",
                 cube_move_name(face * 3));
        check(cubes_equal(&c, &solved), buf);
    }

    /* Each face: X X' = identity */
    printf("\n== X X' = identity ==\n");
    for (int face = 0; face < 6; face++) {
        cube_reset(&c);
        cube_apply(&c, face * 3);     /* CW  */
        cube_apply(&c, face * 3 + 1); /* CCW */
        snprintf(buf, sizeof buf, "%s then %s returns to solved",
                 cube_move_name(face * 3), cube_move_name(face * 3 + 1));
        check(cubes_equal(&c, &solved), buf);
    }

    /* Each face: X2 X2 = identity */
    printf("\n== X2 X2 = identity ==\n");
    for (int face = 0; face < 6; face++) {
        cube_reset(&c);
        cube_apply(&c, face * 3 + 2);
        cube_apply(&c, face * 3 + 2);
        snprintf(buf, sizeof buf, "%s applied 2x returns to solved",
                 cube_move_name(face * 3 + 2));
        check(cubes_equal(&c, &solved), buf);
    }

    /* "Sexy move" R U R' U' has order 6 -- repeating it 6 times solves. */
    printf("\n== (R U R' U')^6 = identity ==\n");
    cube_reset(&c);
    int sexy[] = { M_R, M_U, M_Rp, M_Up };
    for (int i = 0; i < 6; i++) cube_apply_seq(&c, sexy, 4);
    check(cube_is_solved(&c), "(R U R' U') applied 6 times solves the cube");

    /* Sune (R U R' U R U2 R') has order 6 too. */
    printf("\n== Sune^6 = identity ==\n");
    cube_reset(&c);
    int sune[] = { M_R, M_U, M_Rp, M_U, M_R, M_U2, M_Rp };
    for (int i = 0; i < 6; i++) cube_apply_seq(&c, sune, 7);
    check(cube_is_solved(&c), "Sune applied 6 times solves the cube");

    /* Scramble + reverse-inverse round-trip. */
    printf("\n== scramble round-trip ==\n");
    srand(1234);
    cube_reset(&c);
    int moves[40];
    for (int i = 0; i < 40; i++) {
        moves[i] = rand() % M_COUNT;
        cube_apply(&c, moves[i]);
    }
    check(!cube_is_solved(&c), "scrambled cube is not solved");
    for (int i = 39; i >= 0; i--) {
        cube_apply(&c, cube_inverse(moves[i]));
    }
    check(cube_is_solved(&c), "applying inverses in reverse order solves it");

    /* cube_scramble does not crash and produces a non-solved state. */
    printf("\n== cube_scramble ==\n");
    srand(42);
    cube_reset(&c);
    cube_scramble(&c, 25);
    check(!cube_is_solved(&c), "cube_scramble(25) leaves cube unsolved");

    printf("\n%d / %d tests passed (%d failures)\n",
           total - failures, total, failures);
    return failures;
}
