/* dcs_cli — Phase 2.4 port of the prototype CLI onto the new domain layer.
 * User-facing behavior (args, output format) is unchanged so the existing
 * 11-test test_cli.sh runs unmodified against this binary. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../src/framework/platform/iplatform.h"
#include "../src/domain/component.h"
#include "../src/domain/circuit.h"
#include "../src/domain/circuit_io.h"

#define MAX_INPUT_SPECS 16
#define MAX_STEPS       64

typedef struct tagt_input_spec {
    char     name[DOMAIN_NAME_LEN];
    signal_t values[MAX_STEPS];
    int      value_count;
} input_spec_t;

/* ── helpers ──────────────────────────────────────────────────────── */

static char sig_char(signal_t s) {
    return s == SIG_LOW ? '0' : s == SIG_HIGH ? '1' : 'X';
}

static void str_trim(char *s) {
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    int n = (int)strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

/* Format A: "name=v,v,v" → 1 spec, value_count=N
   Format B: "n1=v,n2=v"  → N specs, each value_count=1
   Detection: every comma-token contains '=' → B; only first → A. */
static int parse_input_arg(const char *arg, input_spec_t *specs, int *spec_count, int max_specs) {
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", arg);

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

    int all_have_eq = 1;
    for (int i = 0; i < ntok; i++)
        if (!strchr(toks[i], '=')) { all_have_eq = 0; break; }

    if (all_have_eq && ntok > 1) {
        for (int i = 0; i < ntok; i++) {
            if (*spec_count >= max_specs) return -1;
            char *eq = strchr(toks[i], '=');
            *eq = '\0';
            char *name = toks[i]; str_trim(name);
            char *val  = eq + 1;  str_trim(val);
            if (!*name || !*val) return -1;
            input_spec_t *sp = &specs[(*spec_count)++];
            snprintf(sp->name, DOMAIN_NAME_LEN, "%.*s", DOMAIN_NAME_LEN - 1, name);
            sp->values[0]   = (signal_t)atoi(val);
            sp->value_count = 1;
        }
    } else {
        if (*spec_count >= max_specs) return -1;
        char *eq = strchr(toks[0], '=');
        if (!eq) return -1;
        *eq = '\0';
        char *name = toks[0]; str_trim(name);
        char *first = eq + 1; str_trim(first);
        if (!*name || !*first) return -1;
        input_spec_t *sp = &specs[(*spec_count)++];
        snprintf(sp->name, DOMAIN_NAME_LEN, "%.*s", DOMAIN_NAME_LEN - 1, name);
        sp->values[0]   = (signal_t)atoi(first);
        sp->value_count = 1;
        for (int i = 1; i < ntok && sp->value_count < MAX_STEPS; i++) {
            char *t = toks[i]; str_trim(t);
            sp->values[sp->value_count++] = (signal_t)atoi(t);
        }
    }
    return 0;
}

static const input_spec_t *find_spec(const input_spec_t *specs, int n, const char *name) {
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

/* ── main ─────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    if (argc < 2) { print_usage(); return 1; }
    const char *filepath = argv[1];

    iplatform_t *p = platform_create();
    if (!p) { fprintf(stderr, "error: platform unavailable\n"); return 1; }

    input_spec_t specs[MAX_INPUT_SPECS];
    int       spec_count = 0;
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

    int len = 0;
    char *text = p->read_file(p->self, filepath, &len);
    if (!text) { fprintf(stderr, "error: cannot read file: %s\n", filepath); return 1; }

    char err[256] = {0};
    circuit_t *c = circuit_io_parse(text, err, sizeof(err));
    free(text);
    if (!c) { fprintf(stderr, "parse error: %s\n", err); return 1; }

    int steps = 1;
    for (int i = 0; i < spec_count; i++)
        if (specs[i].value_count > steps) steps = specs[i].value_count;

    if (steps == 1) {
        for (int i = 0; i < c->input_count; i++) {
            const input_spec_t *sp = find_spec(specs, spec_count, c->input_names[i]);
            signal_t v = sp ? sp->values[0] : SIG_UNDEF;
            circuit_set_input(c, c->input_names[i], v);
            printf("%s%s=%c", i == 0 ? "" : " ", c->input_names[i], sig_char(v));
        }
        circuit_evaluate(c);
        printf(" ->");
        for (int i = 0; i < c->output_count; i++)
            printf(" %s=%c", c->output_names[i],
                   sig_char(circuit_get_output(c, c->output_names[i])));
        printf("\n");
    } else {
        printf("step");
        for (int i = 0; i < c->input_count; i++)
            printf("  %*s", col_width(c->input_names[i]), c->input_names[i]);
        for (int i = 0; i < c->output_count; i++)
            printf("  %*s", col_width(c->output_names[i]), c->output_names[i]);
        printf("\n");

        for (int s = 0; s < steps; s++) {
            for (int i = 0; i < c->input_count; i++) {
                const input_spec_t *sp = find_spec(specs, spec_count, c->input_names[i]);
                signal_t v = SIG_UNDEF;
                if (sp) {
                    int idx = s < sp->value_count ? s : sp->value_count - 1;
                    v = sp->values[idx];
                }
                circuit_set_input(c, c->input_names[i], v);
            }
            circuit_evaluate(c);

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

    circuit_destroy(c);
    return 0;
}
