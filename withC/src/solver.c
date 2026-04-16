#include "solver.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define S_EPS  1e-9
#define S_INF  1e18

static const char *OP_SYM[] = {"+", "-", "*", "/"};

static double s_apply(double a, double b, int op) {
    switch (op) {
        case 0: return a + b;
        case 1: return a - b;
        case 2: return a * b;
        case 3: return fabs(b) < S_EPS ? S_INF : a / b;
    }
    return S_INF;
}

/*
 * Five parenthesisation structures for four values n[0..3]
 * and three operators o[0..2]:
 *   0:  ((a o0 b) o1 c) o2 d
 *   1:  (a o1 (b o0 c)) o2 d
 *   2:  (a o0 b) o2 (c o1 d)
 *   3:  a o2 ((b o0 c) o1 d)
 *   4:  a o2 (b o1 (c o0 d))
 */
static double s_eval(double *n, int *o, int s) {
    double a=n[0], b=n[1], c=n[2], d=n[3];
    switch (s) {
        case 0: return s_apply(s_apply(s_apply(a,b,o[0]),c,o[1]),d,o[2]);
        case 1: return s_apply(s_apply(a,s_apply(b,c,o[0]),o[1]),d,o[2]);
        case 2: return s_apply(s_apply(a,b,o[0]),s_apply(c,d,o[1]),o[2]);
        case 3: return s_apply(a,s_apply(s_apply(b,c,o[0]),d,o[1]),o[2]);
        case 4: return s_apply(a,s_apply(b,s_apply(c,d,o[0]),o[1]),o[2]);
    }
    return S_INF;
}

static void s_fmt(double *n, int *o, int s, char *buf, int sz) {
    char a[16], b[16], c[16], d[16];
    snprintf(a,16,"%.4g",n[0]); snprintf(b,16,"%.4g",n[1]);
    snprintf(c,16,"%.4g",n[2]); snprintf(d,16,"%.4g",n[3]);
    switch (s) {
        case 0: snprintf(buf,sz,"((%s%s%s)%s%s)%s%s",a,OP_SYM[o[0]],b,OP_SYM[o[1]],c,OP_SYM[o[2]],d); break;
        case 1: snprintf(buf,sz,"(%s%s(%s%s%s))%s%s",a,OP_SYM[o[1]],b,OP_SYM[o[0]],c,OP_SYM[o[2]],d); break;
        case 2: snprintf(buf,sz,"(%s%s%s)%s(%s%s%s)",a,OP_SYM[o[0]],b,OP_SYM[o[2]],c,OP_SYM[o[1]],d); break;
        case 3: snprintf(buf,sz,"%s%s((%s%s%s)%s%s)",a,OP_SYM[o[2]],b,OP_SYM[o[0]],c,OP_SYM[o[1]],d); break;
        case 4: snprintf(buf,sz,"%s%s(%s%s(%s%s%s))",a,OP_SYM[o[2]],b,OP_SYM[o[1]],c,OP_SYM[o[0]],d); break;
    }
}

/* All 24 permutations of four doubles */
static double s_perms[24][4];
static int    s_perm_cnt;

static void s_swap(double *a, double *b) { double t=*a; *a=*b; *b=t; }

static void s_gen_perms(double *arr, int start, int n) {
    if (start == n) {
        memcpy(s_perms[s_perm_cnt++], arr, (size_t)n * sizeof(double));
        return;
    }
    for (int i = start; i < n; i++) {
        s_swap(&arr[start], &arr[i]);
        s_gen_perms(arr, start + 1, n);
        s_swap(&arr[start], &arr[i]);
    }
}

int solver_find(const SolverConfig *cfg, const int *nums,
                char *out, int out_sz) {
    if (cfg->num_count != 4) return 0;  /* only 4-card variant supported */

    double arr[4] = {
        (double)nums[0], (double)nums[1],
        (double)nums[2], (double)nums[3]
    };
    s_perm_cnt = 0;
    s_gen_perms(arr, 0, 4);

    for (int p = 0; p < 24; p++)
    for (int o1 = 0; o1 < 4; o1++)
    for (int o2 = 0; o2 < 4; o2++)
    for (int o3 = 0; o3 < 4; o3++) {
        int ops[3] = {o1, o2, o3};
        for (int s = 0; s < 5; s++) {
            double v = s_eval(s_perms[p], ops, s);
            if (fabs(v - (double)cfg->target) < S_EPS) {
                if (out) s_fmt(s_perms[p], ops, s, out, out_sz);
                return 1;
            }
        }
    }
    return 0;
}
