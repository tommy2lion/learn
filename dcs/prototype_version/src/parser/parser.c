#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#define MAX_LINE 512

/* ── string helpers ───────────────────────────────────────────── */

static void trim(char *s) {
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    int n = (int)strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

static void strip_comment(char *s) {
    char *h = strchr(s, '#');
    if (h) *h = '\0';
}

/* Write the next line from text[*pos] into out (trimmed, comment-stripped).
   Returns 0 on success (line may be empty after stripping), -1 at end of text. */
static int next_line(const char *text, int *pos, char *out, int out_len) {
    if (!text[*pos]) return -1;
    int start = *pos;
    while (text[*pos] && text[*pos] != '\n') (*pos)++;
    int raw = *pos - start;
    if (text[*pos] == '\n') (*pos)++;
    int copy = raw < out_len - 1 ? raw : out_len - 1;
    memcpy(out, text + start, copy);
    out[copy] = '\0';
    if (copy > 0 && out[copy - 1] == '\r') out[--copy] = '\0'; /* CRLF */
    strip_comment(out);
    trim(out);
    return 0;
}

/* Parse comma-separated identifiers from s into names[][SIM_NAME_LEN].
   Returns count (>= 1) on success, -1 on error. */
static int parse_namelist(const char *s, char names[][SIM_NAME_LEN], int max) {
    int count = 0;
    const char *p = s;
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != ',') p++;
        int len = (int)(p - start);
        while (len > 0 && isspace((unsigned char)start[len - 1])) len--;
        if (len == 0 || len >= SIM_NAME_LEN || count >= max) return -1;
        strncpy(names[count], start, len);
        names[count][len] = '\0';
        count++;
        if (*p == ',') p++;
    }
    return count > 0 ? count : -1;
}

/* Parse "gate_name(arg1)" or "gate_name(arg1, arg2)".
   Fills type_out, in1, in2, and nargs. Returns 0 on success, -1 on error. */
static int parse_gate_expr(const char *s, GateType *type_out,
                           char in1[SIM_NAME_LEN], char in2[SIM_NAME_LEN],
                           int *nargs) {
    const char *lp = strchr(s, '(');
    const char *rp = lp ? strrchr(s, ')') : NULL;
    if (!lp || !rp || rp <= lp) return -1;

    /* gate name */
    int nlen = (int)(lp - s);
    if (nlen <= 0 || nlen >= SIM_NAME_LEN) return -1;
    char gate_name[SIM_NAME_LEN];
    strncpy(gate_name, s, nlen);
    gate_name[nlen] = '\0';
    trim(gate_name);

    if      (strcmp(gate_name, "and") == 0) *type_out = GATE_AND;
    else if (strcmp(gate_name, "or")  == 0) *type_out = GATE_OR;
    else if (strcmp(gate_name, "not") == 0) *type_out = GATE_NOT;
    else return -1;

    /* argument list */
    int alen = (int)(rp - lp - 1);
    if (alen < 0 || alen >= MAX_LINE) return -1;
    char arg_str[MAX_LINE];
    strncpy(arg_str, lp + 1, alen);
    arg_str[alen] = '\0';

    char args[2][SIM_NAME_LEN];
    int n = parse_namelist(arg_str, args, 2);
    if (n < 0) return -1;
    if (*type_out == GATE_NOT && n != 1) return -1;
    if ((*type_out == GATE_AND || *type_out == GATE_OR) && n != 2) return -1;

    memcpy(in1, args[0], SIM_NAME_LEN);
    in2[0] = '\0';
    if (n >= 2) memcpy(in2, args[1], SIM_NAME_LEN);
    *nargs = n;
    return 0;
}

/* Helper: free circuit, write error, return NULL. */
static Circuit *fail(Circuit *c, char *err_out, int err_len,
                     int lineno, const char *fmt, ...) {
    if (err_out && err_len > 0) {
        char msg[256];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(msg, sizeof(msg), fmt, ap);
        va_end(ap);
        if (lineno > 0)
            snprintf(err_out, err_len, "line %d: %s", lineno, msg);
        else
            snprintf(err_out, err_len, "%s", msg);
    }
    circuit_free(c);
    return NULL;
}

/* ── public API ───────────────────────────────────────────────── */

Circuit *parse_circuit(const char *text, char *err_out, int err_len) {
    if (err_out && err_len > 0) err_out[0] = '\0';

    Circuit *c = circuit_new();
    if (!c) return fail(NULL, err_out, err_len, 0, "out of memory");

    int pos = 0, lineno = 0;
    int saw_inputs = 0, saw_outputs = 0;
    char line[MAX_LINE];

    while (next_line(text, &pos, line, MAX_LINE) == 0) {
        lineno++;
        if (!line[0]) continue; /* blank or comment-only */

        /* inputs: a, b, ... */
        if (strncmp(line, "inputs:", 7) == 0) {
            if (saw_inputs)
                return fail(c, err_out, err_len, lineno, "duplicate 'inputs:'");
            saw_inputs = 1;
            char names[SIM_MAX_INPUTS][SIM_NAME_LEN];
            int n = parse_namelist(line + 7, names, SIM_MAX_INPUTS);
            if (n < 0)
                return fail(c, err_out, err_len, lineno, "invalid inputs list");
            for (int i = 0; i < n; i++)
                if (circuit_add_input(c, names[i]) != 0)
                    return fail(c, err_out, err_len, lineno,
                                "cannot add input '%s'", names[i]);
            continue;
        }

        /* outputs: x, y, ... */
        if (strncmp(line, "outputs:", 8) == 0) {
            if (saw_outputs)
                return fail(c, err_out, err_len, lineno, "duplicate 'outputs:'");
            saw_outputs = 1;
            char names[SIM_MAX_OUTPUTS][SIM_NAME_LEN];
            int n = parse_namelist(line + 8, names, SIM_MAX_OUTPUTS);
            if (n < 0)
                return fail(c, err_out, err_len, lineno, "invalid outputs list");
            for (int i = 0; i < n; i++)
                if (circuit_add_output(c, names[i]) != 0)
                    return fail(c, err_out, err_len, lineno,
                                "cannot add output '%s'", names[i]);
            continue;
        }

        /* out_wire = gate_expr */
        char *eq = strchr(line, '=');
        if (!eq)
            return fail(c, err_out, err_len, lineno,
                        "expected '=' in gate assignment");

        char lhs[SIM_NAME_LEN];
        int llen = (int)(eq - line);
        if (llen <= 0 || llen >= SIM_NAME_LEN)
            return fail(c, err_out, err_len, lineno, "wire name too long or empty");
        strncpy(lhs, line, llen);
        lhs[llen] = '\0';
        trim(lhs);

        char rhs[MAX_LINE];
        strncpy(rhs, eq + 1, MAX_LINE - 1);
        rhs[MAX_LINE - 1] = '\0';
        trim(rhs);

        GateType gtype;
        char in1[SIM_NAME_LEN], in2[SIM_NAME_LEN];
        int nargs;
        if (parse_gate_expr(rhs, &gtype, in1, in2, &nargs) != 0)
            return fail(c, err_out, err_len, lineno,
                        "invalid gate expression '%s'", rhs);

        const char *in2_ptr = (nargs >= 2) ? in2 : NULL;
        if (circuit_add_gate(c, gtype, lhs, in1, in2_ptr) != 0)
            return fail(c, err_out, err_len, lineno,
                        "cannot add gate '%s = %s' — check wire names", lhs, rhs);
    }

    if (!saw_inputs)
        return fail(c, err_out, err_len, 0, "missing 'inputs:' declaration");
    if (!saw_outputs)
        return fail(c, err_out, err_len, 0, "missing 'outputs:' declaration");

    return c;
}

char *serialize_circuit(const Circuit *c) {
    int cap = 4096 + c->gate_count * 256;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int pos = 0;

    /* inputs: a, b, ... */
    pos += snprintf(buf + pos, cap - pos, "inputs:");
    for (int i = 0; i < c->input_count; i++)
        pos += snprintf(buf + pos, cap - pos, "%s%s",
                        i == 0 ? " " : ", ", c->input_names[i]);
    pos += snprintf(buf + pos, cap - pos, "\n");

    /* outputs: x, y, ... */
    pos += snprintf(buf + pos, cap - pos, "outputs:");
    for (int i = 0; i < c->output_count; i++)
        pos += snprintf(buf + pos, cap - pos, "%s%s",
                        i == 0 ? " " : ", ", c->output_names[i]);
    pos += snprintf(buf + pos, cap - pos, "\n");

    pos += snprintf(buf + pos, cap - pos, "\n");

    /* one line per gate */
    for (int i = 0; i < c->gate_count; i++) {
        const GateInst *g = &c->gates[i];
        if (g->in_count == 1)
            pos += snprintf(buf + pos, cap - pos, "%s = %s(%s)\n",
                            g->out_wire, gate_type_name(g->type), g->in_wires[0]);
        else
            pos += snprintf(buf + pos, cap - pos, "%s = %s(%s, %s)\n",
                            g->out_wire, gate_type_name(g->type),
                            g->in_wires[0], g->in_wires[1]);
    }

    return buf;
}
