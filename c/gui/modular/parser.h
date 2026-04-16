#ifndef PARSER_H
#define PARSER_H

/*
 * parser — recursive-descent arithmetic expression evaluator
 *
 * Supports: integers, +  -  *  /  ( )
 * Tracks which integer literals appear in the expression.
 * No raylib dependency. Re-entrant (no global state).
 */

#define PARSER_INF 1e18

typedef struct {
    double value;      /* result, or PARSER_INF on error                */
    int    nums[8];    /* integer literals found during parse, in order  */
    int    num_count;  /* how many literals were found                   */
    int    error;      /* non-zero on syntax error or division-by-zero   */
} ParseResult;

ParseResult parser_eval(const char *expr);

#endif /* PARSER_H */
