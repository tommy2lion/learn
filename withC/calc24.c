/*
 * CALC 24 GAME — C Edition
 * Use all 4 numbers with +  -  *  /  and parentheses to make 24.
 * Ace=1, J=11, Q=12, K=13
 *
 * Commands:
 *   <expression>   e.g.  (3+5)*3   or   8/4+2*10
 *   hint           show a valid solution
 *   skip           get new cards (counts as skipped)
 *   quit / q       exit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#define TARGET      24
#define EPS         1e-9
#define MAX_EXPR    256
#define INF         1e18

/* ── Card faces ──────────────────────────────────────────────────────────── */
static const char *face(int n) {
    static const char *tbl[] = {"","A","2","3","4","5","6","7","8","9","10","J","Q","K"};
    return (n >= 1 && n <= 13) ? tbl[n] : "?";
}

/* ── Game state ──────────────────────────────────────────────────────────── */
static int cards[4];
static int solved_count  = 0;
static int skipped_count = 0;

/* ── ASCII card display ──────────────────────────────────────────────────── */
static void print_cards(void) {
    /* top border */
    for (int i = 0; i < 4; i++) printf(" .-------.");
    printf("\n");
    /* top-left label */
    for (int i = 0; i < 4; i++) printf(" |%-2s     |", face(cards[i]));
    printf("\n");
    /* empty row */
    for (int i = 0; i < 4; i++) printf(" |       |");
    printf("\n");
    /* centre value */
    for (int i = 0; i < 4; i++) printf(" |  %2s   |", face(cards[i]));
    printf("\n");
    /* empty row */
    for (int i = 0; i < 4; i++) printf(" |       |");
    printf("\n");
    /* bottom-right label */
    for (int i = 0; i < 4; i++) printf(" |     %2s|", face(cards[i]));
    printf("\n");
    /* bottom border */
    for (int i = 0; i < 4; i++) printf(" '-------'");
    printf("\n");
    /* numeric index hint */
    printf("    ");
    for (int i = 0; i < 4; i++) printf("  [%d]=%2d  ", i + 1, cards[i]);
    printf("\n\n");
}

/* ── Solver ──────────────────────────────────────────────────────────────── */

/* Apply one arithmetic operator; returns INF on division-by-zero */
static double apply_op(double a, double b, int op) {
    switch (op) {
        case 0: return a + b;
        case 1: return a - b;
        case 2: return a * b;
        case 3: return fabs(b) < EPS ? INF : a / b;
    }
    return INF;
}

static const char *op_sym(int op) {
    static const char *s[] = {"+", "-", "*", "/"};
    return s[op];
}

/*
 * Five parenthesisation structures for four numbers n[0..3]
 * and three operators o[0..2]:
 *   0:  ((a o0 b) o1 c) o2 d
 *   1:  (a o1 (b o0 c)) o2 d
 *   2:  (a o0 b) o2 (c o1 d)
 *   3:  a o2 ((b o0 c) o1 d)
 *   4:  a o2 (b o1 (c o0 d))
 */
static double eval_struct(double *n, int *o, int s) {
    double a=n[0], b=n[1], c=n[2], d=n[3];
    switch (s) {
        case 0: return apply_op(apply_op(apply_op(a,b,o[0]),c,o[1]),d,o[2]);
        case 1: return apply_op(apply_op(a,apply_op(b,c,o[0]),o[1]),d,o[2]);
        case 2: return apply_op(apply_op(a,b,o[0]),apply_op(c,d,o[1]),o[2]);
        case 3: return apply_op(a,apply_op(apply_op(b,c,o[0]),d,o[1]),o[2]);
        case 4: return apply_op(a,apply_op(b,apply_op(c,d,o[0]),o[1]),o[2]);
    }
    return INF;
}

static void fmt_struct(double *n, int *o, int s, char *buf, int bufsz) {
    char a[16],b[16],c[16],d[16];
    snprintf(a,16,"%.4g",n[0]); snprintf(b,16,"%.4g",n[1]);
    snprintf(c,16,"%.4g",n[2]); snprintf(d,16,"%.4g",n[3]);
    switch (s) {
        case 0: snprintf(buf,bufsz,"((%s%s%s)%s%s)%s%s",
                         a,op_sym(o[0]),b,op_sym(o[1]),c,op_sym(o[2]),d); break;
        case 1: snprintf(buf,bufsz,"(%s%s(%s%s%s))%s%s",
                         a,op_sym(o[1]),b,op_sym(o[0]),c,op_sym(o[2]),d); break;
        case 2: snprintf(buf,bufsz,"(%s%s%s)%s(%s%s%s)",
                         a,op_sym(o[0]),b,op_sym(o[2]),c,op_sym(o[1]),d); break;
        case 3: snprintf(buf,bufsz,"%s%s((%s%s%s)%s%s)",
                         a,op_sym(o[2]),b,op_sym(o[0]),c,op_sym(o[1]),d); break;
        case 4: snprintf(buf,bufsz,"%s%s(%s%s(%s%s%s))",
                         a,op_sym(o[2]),b,op_sym(o[1]),c,op_sym(o[0]),d); break;
    }
}

/* Generate all 24 permutations of arr[0..n-1] into perms[][]. */
static double perms[24][4];
static int    perm_cnt;

static void swap_d(double *a, double *b) { double t=*a; *a=*b; *b=t; }
static void gen_perms(double *arr, int start, int n) {
    if (start == n) { memcpy(perms[perm_cnt++], arr, n * sizeof(double)); return; }
    for (int i = start; i < n; i++) {
        swap_d(&arr[start], &arr[i]);
        gen_perms(arr, start + 1, n);
        swap_d(&arr[start], &arr[i]);
    }
}

/*
 * solve() — returns 1 if a solution exists.
 * If solution != NULL, writes a human-readable expression into it.
 */
static int solve(int *nums, char *solution) {
    double arr[4] = { nums[0], nums[1], nums[2], nums[3] };
    perm_cnt = 0;
    gen_perms(arr, 0, 4);

    for (int p = 0; p < 24; p++)
        for (int o1 = 0; o1 < 4; o1++)
        for (int o2 = 0; o2 < 4; o2++)
        for (int o3 = 0; o3 < 4; o3++) {
            int ops[3] = {o1, o2, o3};
            for (int s = 0; s < 5; s++) {
                double v = eval_struct(perms[p], ops, s);
                if (fabs(v - TARGET) < EPS) {
                    if (solution) fmt_struct(perms[p], ops, s, solution, 128);
                    return 1;
                }
            }
        }
    return 0;
}

/* ── Expression parser ───────────────────────────────────────────────────── */
/* Recursive-descent parser with number tracking */

static const char *pp;        /* parse pointer */
static int pnums[8];          /* numbers encountered */
static int pnum_cnt;
static int perr;              /* parse error flag */

static double parse_expr(void);
static double parse_term(void);
static double parse_factor(void);

static void skip_ws(void) { while (*pp == ' ' || *pp == '\t') pp++; }

static double parse_factor(void) {
    skip_ws();
    if (*pp == '(') {
        pp++;
        double v = parse_expr();
        skip_ws();
        if (*pp == ')') pp++; else perr = 1;
        return v;
    }
    if (isdigit((unsigned char)*pp)) {
        int num = 0;
        while (isdigit((unsigned char)*pp)) { num = num * 10 + (*pp - '0'); pp++; }
        if (pnum_cnt < 8) pnums[pnum_cnt++] = num;
        return (double)num;
    }
    perr = 1;
    return 0;
}

static double parse_term(void) {
    double v = parse_factor();
    for (;;) {
        skip_ws();
        if      (*pp == '*') { pp++; v *= parse_factor(); }
        else if (*pp == '/') { pp++; double d = parse_factor();
                               if (fabs(d) < EPS) perr = 1; else v /= d; }
        else break;
    }
    return v;
}

static double parse_expr(void) {
    double v = parse_term();
    for (;;) {
        skip_ws();
        if      (*pp == '+') { pp++; v += parse_term(); }
        else if (*pp == '-') { pp++; v -= parse_term(); }
        else break;
    }
    return v;
}

static double evaluate(const char *s, int *used, int *used_cnt) {
    pp = s; pnum_cnt = 0; perr = 0;
    double result = parse_expr();
    skip_ws();
    if (*pp != '\0') perr = 1;
    if (used)     memcpy(used, pnums, pnum_cnt * sizeof(int));
    if (used_cnt) *used_cnt = pnum_cnt;
    return perr ? INF : result;
}

/* Check that the numbers in the expression exactly match cards[]. */
static int nums_match(int *used, int cnt) {
    if (cnt != 4) return 0;
    int ca[4], ua[4];
    memcpy(ca, cards, 4 * sizeof(int));
    memcpy(ua, used,  4 * sizeof(int));
    /* simple insertion sort for both arrays */
    for (int i = 1; i < 4; i++) {
        int t; int j;
        t = ca[i]; for (j=i; j>0 && ca[j-1]>t; j--) ca[j]=ca[j-1]; ca[j]=t;
        t = ua[i]; for (j=i; j>0 && ua[j-1]>t; j--) ua[j]=ua[j-1]; ua[j]=t;
    }
    for (int i = 0; i < 4; i++) if (ca[i] != ua[i]) return 0;
    return 1;
}

/* ── UI helpers ──────────────────────────────────────────────────────────── */
static void new_round(void) {
    for (int i = 0; i < 4; i++) cards[i] = (rand() % 13) + 1;
}

static void print_header(void) {
    printf("\n==========================================\n");
    printf("        CALC 24 GAME  (C Edition)        \n");
    printf("==========================================\n");
    printf("  Solved: %-4d   Skipped: %d\n", solved_count, skipped_count);
    printf("------------------------------------------\n");
}

static void print_help(void) {
    printf("\n  HOW TO PLAY\n");
    printf("  -----------\n");
    printf("  Use all 4 card numbers with  + - * /  and\n");
    printf("  parentheses to make the result equal 24.\n\n");
    printf("  COMMANDS\n");
    printf("  --------\n");
    printf("  <expr>   e.g.  (1+2+3)*4   8/(4-2)*6\n");
    printf("  hint     show one valid solution\n");
    printf("  skip     new cards (skipped +1)\n");
    printf("  help     show this help\n");
    printf("  quit     exit the game\n\n");
    printf("  NOTE: Ace=1  J=11  Q=12  K=13\n\n");
}

/* ── Entry point ─────────────────────────────────────────────────────────── */
int main(void) {
    srand((unsigned)time(NULL));
    new_round();

    print_header();
    print_help();

    char input[MAX_EXPR];

    for (;;) {
        print_cards();
        printf(">>> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\r\n")] = '\0';

        /* strip leading whitespace */
        char *cmd = input;
        while (*cmd == ' ' || *cmd == '\t') cmd++;
        if (*cmd == '\0') continue;

        /* ── Built-in commands ── */
        if (!strcmp(cmd,"quit") || !strcmp(cmd,"exit") || !strcmp(cmd,"q")) {
            printf("\nFinal — Solved: %d  Skipped: %d\nGoodbye!\n\n",
                   solved_count, skipped_count);
            break;
        }

        if (!strcmp(cmd,"help") || !strcmp(cmd,"?")) {
            print_help();
            continue;
        }

        if (!strcmp(cmd,"skip") || !strcmp(cmd,"new") || !strcmp(cmd,"n")) {
            char sol[128];
            skipped_count++;
            if (solve(cards, sol))
                printf("  (A solution existed: %s = 24)\n", sol);
            else
                printf("  (No solution existed for those cards.)\n");
            new_round();
            print_header();
            continue;
        }

        if (!strcmp(cmd,"hint") || !strcmp(cmd,"h")) {
            char sol[128];
            if (solve(cards, sol)) {
                printf("  Hint: %s = 24\n\n", sol);
            } else {
                printf("  No solution exists for these cards. Skipping...\n");
                skipped_count++;
                new_round();
                print_header();
            }
            continue;
        }

        /* ── Evaluate user expression ── */
        int used[8], used_cnt = 0;
        double result = evaluate(cmd, used, &used_cnt);

        if (perr || result > INF / 2) {
            printf("  Invalid expression — check your syntax.\n\n");
            continue;
        }

        if (!nums_match(used, used_cnt)) {
            printf("  Use all 4 numbers (%d, %d, %d, %d) exactly once!\n\n",
                   cards[0], cards[1], cards[2], cards[3]);
            continue;
        }

        if (fabs(result - TARGET) < EPS) {
            printf("\n  *** Correct!  %s = 24  ***\n\n", cmd);
            solved_count++;
            new_round();
            print_header();
        } else {
            printf("  = %.6g  — not 24. Try again!\n\n", result);
        }
    }

    return 0;
}
