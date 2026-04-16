#ifndef SOLVER_H
#define SOLVER_H

/*
 * solver — brute-force 24-game solver
 *
 * No raylib dependency. Safe to unit-test standalone.
 *
 * Current implementation supports exactly 4 numbers (num_count must be 4).
 * The target field makes the algorithm target-agnostic at runtime.
 */

typedef struct {
    int target;      /* number to reach (default 24)          */
    int num_count;   /* cards in play  (must be 4 currently)  */
} SolverConfig;

/*
 * Returns 1 if a solution exists for the given numbers.
 * If out != NULL, writes a human-readable expression into out[0..out_sz-1].
 */
int solver_find(const SolverConfig *cfg, const int *nums,
                char *out, int out_sz);

#endif /* SOLVER_H */
