#include "parser.h"
#include <math.h>
#include <string.h>

#define P_EPS 1e-9

/*
 * Internal parse context — passed by pointer through all recursive calls.
 * This avoids global state and makes the parser re-entrant.
 */
typedef struct {
    const char *ptr;
    int         nums[8];
    int         num_count;
    int         error;
} ParseCtx;

/* Forward declarations */
static double p_expr  (ParseCtx *c);
static double p_term  (ParseCtx *c);
static double p_factor(ParseCtx *c);

static void p_skip(ParseCtx *c) {
    while (*c->ptr == ' ' || *c->ptr == '\t') c->ptr++;
}

static double p_factor(ParseCtx *c) {
    p_skip(c);

    if (*c->ptr == '(') {
        c->ptr++;
        double v = p_expr(c);
        p_skip(c);
        if (*c->ptr == ')') c->ptr++;
        else                c->error = 1;
        return v;
    }

    if (*c->ptr >= '0' && *c->ptr <= '9') {
        int n = 0;
        while (*c->ptr >= '0' && *c->ptr <= '9') {
            n = n * 10 + (*c->ptr - '0');
            c->ptr++;
        }
        if (c->num_count < 8) c->nums[c->num_count++] = n;
        return (double)n;
    }

    c->error = 1;
    return 0.0;
}

static double p_term(ParseCtx *c) {
    double v = p_factor(c);
    for (;;) {
        p_skip(c);
        if (*c->ptr == '*') {
            c->ptr++;
            v *= p_factor(c);
        } else if (*c->ptr == '/') {
            c->ptr++;
            double d = p_factor(c);
            if (fabs(d) < P_EPS) c->error = 1;
            else                 v /= d;
        } else {
            break;
        }
    }
    return v;
}

static double p_expr(ParseCtx *c) {
    double v = p_term(c);
    for (;;) {
        p_skip(c);
        if      (*c->ptr == '+') { c->ptr++; v += p_term(c); }
        else if (*c->ptr == '-') { c->ptr++; v -= p_term(c); }
        else break;
    }
    return v;
}

ParseResult parser_eval(const char *expr) {
    ParseCtx ctx;
    ctx.ptr       = expr;
    ctx.num_count = 0;
    ctx.error     = 0;
    memset(ctx.nums, 0, sizeof(ctx.nums));

    double r = p_expr(&ctx);
    p_skip(&ctx);
    if (*ctx.ptr != '\0') ctx.error = 1;

    ParseResult res;
    res.value     = ctx.error ? PARSER_INF : r;
    res.error     = ctx.error;
    res.num_count = ctx.num_count;
    memcpy(res.nums, ctx.nums, (size_t)ctx.num_count * sizeof(int));
    return res;
}
