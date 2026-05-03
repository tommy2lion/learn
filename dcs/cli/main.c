#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sim.h"
#include "parser.h"

#define MAX_INPUT_SPECS 16
#define MAX_STEPS       64

typedef struct {
    char   name[SIM_NAME_LEN];
    Signal values[MAX_STEPS];
    int    value_count;
} InputSpec;

/* ── helpers ──────────────────────────────────────────────────── */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return NULL; }
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, len, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static char sig_char(Signal s) {
    return s == SIG_LOW ? '0' : s == SIG_HIGH ? '1' : 'X';
}

static void str_trim(char *s) {
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    int n = (int)strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

/* Parse one --input arg into one or more InputSpecs.
   Format A: "name=v,v,v"     → 1 spec, value_count = N (time steps)
   Format B: "n1=v,n2=v,..."  → N specs, each value_count = 1 (multi-assignment)
   Detected by: all comma-separated tokens contain '=' (B) vs only the first (A). */
static int parse_input_arg(const char *arg, InputSpec *specs, int *spec_count, int max_specs) {
    char buf[512];
    strncpy(buf, arg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* split into tokens by ',' */
    char *toks[64];
    int   ntok = 0;
    char *p = buf;
    toks[ntok++] = p;
    while (*p) {
        if (*p == ',') {
            *p++ = '\0';
            if (ntok >= 64) return -1;
            toks[ntok++] = p;
        } else p++;
    }

    /* check if every token has '=' (Format B) */
    int all_have_eq = 1;
    for (int i = 0; i < ntok; i++)
        if (!strchr(toks[i], '=')) { all_have_eq = 0; break; }

    if (all_have_eq && ntok > 1) {
        /* Format B: multiple assignments, each one step */
        for (int i = 0; i < ntok; i++) {
            if (*spec_count >= max_specs) return -1;
            char *eq = strchr(toks[i], '=');
            *eq = '\0';
            char *name = toks[i]; str_trim(name);
            char *val  = eq + 1;  str_trim(val);
            if (!*name || !*val) return -1;
            InputSpec *sp = &specs[(*spec_count)++];
            strncpy(sp->name, name, SIM_NAME_LEN - 1);
            sp->name[SIM_NAME_LEN - 1] = '\0';
            sp->values[0]   = (Signal)atoi(val);
            sp->value_count = 1;
        }
    } else {
        /* Format A: one input, time-step values */
        if (*spec_count >= max_specs) return -1;
        char *eq = strchr(toks[0], '=');
        if (!eq) return -1;
        *eq = '\0';
        char *name = toks[0]; str_trim(name);
        char *first_val = eq + 1; str_trim(first_val);
        if (!*name || !*first_val) return -1;

        InputSpec *sp = &specs[(*spec_count)++];
        strncpy(sp->name, name, SIM_NAME_LEN - 1);
        sp->name[SIM_NAME_LEN - 1] = '\0';
        sp->values[0]   = (Signal)atoi(first_val);
        sp->value_count = 1;
        for (int i = 1; i < ntok && sp->value_count < MAX_STEPS; i++) {
            char *t = toks[i]; str_trim(t);
            sp->values[sp->value_count++] = (Signal)atoi(t);
        }
    }
    return 0;
}

/* Find spec for a given input name. Returns NULL if not specified. */
static const InputSpec *find_spec(const InputSpec *specs, int n, const char *name) {
    for (int i = 0; i < n; i++)
        if (strcmp(specs[i].name, name) == 0) return &specs[i];
    return NULL;
}

static int col_width(const char *header) {
    int w = (int)strlen(header);
    return w < 1 ? 1 : w;
}

static void print_usage(void) {
    fprintf(stderr,
        "usage: dcs_cli <file.dcs> [--input \"name=v[,v...]\"]...\n"
        "\n"
        "  --input name=v,v,v   set input 'name' to a series of step values\n"
        "  --input n1=v,n2=v    set multiple inputs at once (one step each)\n");
}

/* ── main ─────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    if (argc < 2) { print_usage(); return 1; }

    const char *filepath = argv[1];
    InputSpec   specs[MAX_INPUT_SPECS];
    int         spec_count = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--input") == 0) {
            if (i + 1 >= argc) { print_usage(); return 1; }
            if (parse_input_arg(argv[++i], specs, &spec_count, MAX_INPUT_SPECS) != 0) {
                fprintf(stderr, "error: invalid --input: %s\n", argv[i]);
                return 1;
            }
        } else {
            fprintf(stderr, "error: unknown argument: %s\n", argv[i]);
            print_usage();
            return 1;
        }
    }

    char *text = read_file(filepath);
    if (!text) {
        fprintf(stderr, "error: cannot read file: %s\n", filepath);
        return 1;
    }

    char err[256] = {0};
    Circuit *c = parse_circuit(text, err, sizeof(err));
    free(text);
    if (!c) {
        fprintf(stderr, "parse error: %s\n", err);
        return 1;
    }

    /* number of steps = max value_count across all specs (default 1) */
    int steps = 1;
    for (int i = 0; i < spec_count; i++)
        if (specs[i].value_count > steps) steps = specs[i].value_count;

    if (steps == 1) {
        /* Compact form: a=1 b=0 -> y=0 */
        for (int i = 0; i < c->input_count; i++) {
            const InputSpec *sp = find_spec(specs, spec_count, c->input_names[i]);
            Signal v = sp ? sp->values[0] : SIG_UNDEF;
            circuit_set_input(c, c->input_names[i], v);
            printf("%s%s=%c", i == 0 ? "" : " ", c->input_names[i], sig_char(v));
        }
        circuit_run(c);
        printf(" ->");
        for (int i = 0; i < c->output_count; i++)
            printf(" %s=%c", c->output_names[i],
                   sig_char(circuit_get_output(c, c->output_names[i])));
        printf("\n");
    } else {
        /* Table form */
        printf("step");
        for (int i = 0; i < c->input_count; i++)
            printf("  %*s", col_width(c->input_names[i]), c->input_names[i]);
        for (int i = 0; i < c->output_count; i++)
            printf("  %*s", col_width(c->output_names[i]), c->output_names[i]);
        printf("\n");

        for (int s = 0; s < steps; s++) {
            for (int i = 0; i < c->input_count; i++) {
                const InputSpec *sp = find_spec(specs, spec_count, c->input_names[i]);
                Signal v = SIG_UNDEF;
                if (sp) {
                    int idx = s < sp->value_count ? s : sp->value_count - 1;
                    v = sp->values[idx];
                }
                circuit_set_input(c, c->input_names[i], v);
            }
            circuit_run(c);

            printf("%4d", s);
            for (int i = 0; i < c->input_count; i++)
                printf("  %*c", col_width(c->input_names[i]),
                       sig_char(circuit_get_wire(c, c->input_names[i])));
            for (int i = 0; i < c->output_count; i++)
                printf("  %*c", col_width(c->output_names[i]),
                       sig_char(circuit_get_output(c, c->output_names[i])));
            printf("\n");
        }
    }

    circuit_free(c);
    return 0;
}
