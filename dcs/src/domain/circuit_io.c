#include "circuit_io.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#define MAX_LINE 512

/* ── string helpers ──────────────────────────────────────────────── */

static void str_trim(char *s) {
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

/* Pull the next line. Returns 0 (line written, possibly empty after trim/comment)
   or -1 at end of text. */
static int next_line(const char *text, int *pos, char *out, int out_len) {
    if (!text[*pos]) return -1;
    int start = *pos;
    while (text[*pos] && text[*pos] != '\n') (*pos)++;
    int raw = *pos - start;
    if (text[*pos] == '\n') (*pos)++;
    int copy = raw < out_len - 1 ? raw : out_len - 1;
    memcpy(out, text + start, copy);
    out[copy] = '\0';
    if (copy > 0 && out[copy - 1] == '\r') out[--copy] = '\0';   /* CRLF */
    strip_comment(out);
    str_trim(out);
    return 0;
}

/* Parse comma-separated identifiers. Returns count or -1. */
static int parse_namelist(const char *s, char names[][DOMAIN_NAME_LEN], int max) {
    int count = 0;
    const char *p = s;
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != ',') p++;
        int len = (int)(p - start);
        while (len > 0 && isspace((unsigned char)start[len - 1])) len--;
        if (len == 0 || len >= DOMAIN_NAME_LEN || count >= max) return -1;
        memcpy(names[count], start, len);
        names[count][len] = '\0';
        count++;
        if (*p == ',') p++;
    }
    return count > 0 ? count : -1;
}

/* Parse "gate_name(arg1)" or "gate_name(arg1, arg2)". */
static int parse_gate_expr(const char *s, component_kind_t *kind_out,
                           char in1[DOMAIN_NAME_LEN], char in2[DOMAIN_NAME_LEN],
                           int *nargs_out) {
    const char *lp = strchr(s, '(');
    const char *rp = lp ? strrchr(s, ')') : NULL;
    if (!lp || !rp || rp <= lp) return -1;

    int nlen = (int)(lp - s);
    if (nlen <= 0 || nlen >= DOMAIN_NAME_LEN) return -1;
    char gate_name[DOMAIN_NAME_LEN];
    memcpy(gate_name, s, nlen);
    gate_name[nlen] = '\0';
    str_trim(gate_name);

    if      (strcmp(gate_name, "and") == 0) *kind_out = COMP_AND;
    else if (strcmp(gate_name, "or")  == 0) *kind_out = COMP_OR;
    else if (strcmp(gate_name, "not") == 0) *kind_out = COMP_NOT;
    else return -1;

    int alen = (int)(rp - lp - 1);
    if (alen < 0 || alen >= MAX_LINE) return -1;
    char arg_str[MAX_LINE];
    memcpy(arg_str, lp + 1, alen);
    arg_str[alen] = '\0';

    char args[2][DOMAIN_NAME_LEN];
    int n = parse_namelist(arg_str, args, 2);
    if (n < 0) return -1;
    if (*kind_out == COMP_NOT && n != 1) return -1;
    if ((*kind_out == COMP_AND || *kind_out == COMP_OR) && n != 2) return -1;

    memcpy(in1, args[0], DOMAIN_NAME_LEN);
    if (n >= 2) memcpy(in2, args[1], DOMAIN_NAME_LEN);
    else        in2[0] = '\0';
    *nargs_out = n;
    return 0;
}

/* Free circuit, write error, return NULL. */
static circuit_t *fail(circuit_t *c, char *err_out, int err_len,
                       int lineno, const char *fmt, ...) {
    if (err_out && err_len > 0) {
        char msg[256];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(msg, sizeof(msg), fmt, ap);
        va_end(ap);
        if (lineno > 0) snprintf(err_out, err_len, "line %d: %s", lineno, msg);
        else            snprintf(err_out, err_len, "%s", msg);
    }
    circuit_destroy(c);
    return NULL;
}

/* ── public: parse ───────────────────────────────────────────────── */

circuit_t *circuit_io_parse(const char *text, char *err_out, int err_len) {
    if (err_out && err_len > 0) err_out[0] = '\0';

    circuit_t *c = circuit_create();
    if (!c) return fail(NULL, err_out, err_len, 0, "out of memory");

    int pos = 0, lineno = 0;
    int saw_inputs = 0, saw_outputs = 0;
    char line[MAX_LINE];

    while (next_line(text, &pos, line, MAX_LINE) == 0) {
        lineno++;
        if (!line[0]) continue;

        if (strncmp(line, "inputs:", 7) == 0) {
            if (saw_inputs) return fail(c, err_out, err_len, lineno, "duplicate 'inputs:'");
            saw_inputs = 1;
            char names[DOMAIN_MAX_IO][DOMAIN_NAME_LEN];
            int n = parse_namelist(line + 7, names, DOMAIN_MAX_IO);
            if (n < 0) return fail(c, err_out, err_len, lineno, "invalid inputs list");
            for (int i = 0; i < n; i++)
                if (circuit_add_input(c, names[i]) != 0)
                    return fail(c, err_out, err_len, lineno,
                                "cannot add input '%s'", names[i]);
            continue;
        }

        if (strncmp(line, "outputs:", 8) == 0) {
            if (saw_outputs) return fail(c, err_out, err_len, lineno, "duplicate 'outputs:'");
            saw_outputs = 1;
            char names[DOMAIN_MAX_IO][DOMAIN_NAME_LEN];
            int n = parse_namelist(line + 8, names, DOMAIN_MAX_IO);
            if (n < 0) return fail(c, err_out, err_len, lineno, "invalid outputs list");
            for (int i = 0; i < n; i++)
                if (circuit_add_output(c, names[i]) != 0)
                    return fail(c, err_out, err_len, lineno,
                                "cannot add output '%s'", names[i]);
            continue;
        }

        /* `out_wire = gate(...)` */
        char *eq = strchr(line, '=');
        if (!eq) return fail(c, err_out, err_len, lineno, "expected '=' in gate assignment");

        char lhs[DOMAIN_NAME_LEN];
        int llen = (int)(eq - line);
        if (llen <= 0 || llen >= DOMAIN_NAME_LEN)
            return fail(c, err_out, err_len, lineno, "wire name too long or empty");
        memcpy(lhs, line, llen);
        lhs[llen] = '\0';
        str_trim(lhs);

        char rhs[MAX_LINE];
        snprintf(rhs, sizeof(rhs), "%s", eq + 1);
        str_trim(rhs);

        component_kind_t kind;
        char in1[DOMAIN_NAME_LEN], in2[DOMAIN_NAME_LEN];
        int nargs;
        if (parse_gate_expr(rhs, &kind, in1, in2, &nargs) != 0)
            return fail(c, err_out, err_len, lineno, "invalid gate expression '%s'", rhs);

        component_t *comp = NULL;
        switch (kind) {
            case COMP_AND: comp = gate_and_create(lhs); break;
            case COMP_OR:  comp = gate_or_create (lhs); break;
            case COMP_NOT: comp = gate_not_create(lhs); break;
            default:       break;
        }
        if (!comp) return fail(c, err_out, err_len, lineno, "out of memory");

        const char *in2_p = (nargs >= 2) ? in2 : NULL;
        if (circuit_add_component(c, comp, in1, in2_p) != 0) {
            component_destroy(comp);
            return fail(c, err_out, err_len, lineno,
                        "cannot add component '%s' — check wire names", lhs);
        }
    }

    if (!saw_inputs)  return fail(c, err_out, err_len, 0, "missing 'inputs:' declaration");
    if (!saw_outputs) return fail(c, err_out, err_len, 0, "missing 'outputs:' declaration");
    return c;
}

/* ── public: serialize ───────────────────────────────────────────── */

char *circuit_io_serialize(const circuit_t *c) {
    int cap = 4096 + c->component_count * 256;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    int pos = 0;

    pos += snprintf(buf + pos, cap - pos, "inputs:");
    for (int i = 0; i < c->input_count; i++)
        pos += snprintf(buf + pos, cap - pos, "%s%s",
                        i == 0 ? " " : ", ", c->input_names[i]);
    pos += snprintf(buf + pos, cap - pos, "\n");

    pos += snprintf(buf + pos, cap - pos, "outputs:");
    for (int i = 0; i < c->output_count; i++)
        pos += snprintf(buf + pos, cap - pos, "%s%s",
                        i == 0 ? " " : ", ", c->output_names[i]);
    pos += snprintf(buf + pos, cap - pos, "\n\n");

    for (int i = 0; i < c->component_count; i++) {
        const component_t *comp = c->components[i];
        const char *kn = component_kind_name(component_kind(comp));
        if (component_pin_count_in(comp) == 1)
            pos += snprintf(buf + pos, cap - pos, "%s = %s(%s)\n",
                            comp->name, kn, comp->in_wires[0]);
        else
            pos += snprintf(buf + pos, cap - pos, "%s = %s(%s, %s)\n",
                            comp->name, kn, comp->in_wires[0], comp->in_wires[1]);
    }
    return buf;
}
