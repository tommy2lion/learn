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

static void print_usage(FILE *out) {
    fprintf(out,
        "usage: dcs_cli <file.dcs> [--input \"name=v[,v...]\"]...\n"
        "       dcs_cli --help | -h           # brief usage\n"
        "       dcs_cli --help-format         # full .dcs file format reference\n"
        "\n"
        "  --input name=v,v,v   set input 'name' to a series of step values\n"
        "  --input n1=v,n2=v    set multiple inputs at once (one step each)\n");
}

/* --help-format prints a self-contained reference that an AI assistant
   (or a new contributor) can read once and use to generate valid .dcs
   files without external context. Grammar + every annotation + worked
   examples + current constraints. (R-16) */
static void print_format_help(FILE *out) {
    fputs(
"dcs_cli — .dcs file format reference\n"
"=====================================\n"
"\n"
"A `.dcs` file describes a digital circuit. The CLI reads the file,\n"
"runs the simulation against the input values you supply, and prints\n"
"the resulting output table.\n"
"\n"
"FILE STRUCTURE\n"
"--------------\n"
"\n"
"  inputs:  <name>[, <name>]*       # external input pins\n"
"  outputs: <name>[, <name>]*       # external output pins\n"
"\n"
"  <wire_name> = <gate>(<arg>[, <arg>])\n"
"\n"
"  # any line starting with '#' is a comment (except '# @...' annotations\n"
"    described below)\n"
"\n"
"Each non-blank, non-comment, non-declaration line is a gate expression:\n"
"it assigns the result of a gate applied to existing wires to a new wire\n"
"named on the left of '='. Arguments may be input names, output names,\n"
"or any wire produced by an earlier line. Wire names must be unique\n"
"across the whole file. Maximum name length: 32 characters.\n"
"\n"
"GATE KINDS\n"
"----------\n"
"\n"
"  and(a, b)   2-input logical AND\n"
"  or(a, b)    2-input logical OR\n"
"  not(a)      1-input logical NOT\n"
"\n"
"More kinds (NAND, NOR, XOR, multi-input variants, sequential primitives\n"
"like D-FF) are planned for Step 3 but are not available today.\n"
"\n"
"OPTIONAL ANNOTATIONS\n"
"--------------------\n"
"\n"
"These extend the file with GUI-only metadata. The simulator (CLI) ignores\n"
"them; the GUI editor reads them when opening, writes them when saving.\n"
"Annotations live after the gate expressions and are introduced by\n"
"`# @<name>` headers. Unknown annotations are silently ignored, so the\n"
"format is forward-compatible.\n"
"\n"
"  # @layout                        # component / pin positions in pixels\n"
"  # @  <wire_name> = X, Y          #   X, Y of a gate's centre\n"
"  # @  __input:<name>  = X, Y      #   X, Y of an external input pin\n"
"  # @  __output:<name> = X, Y      #   X, Y of an external output pin\n"
"\n"
"  # @wires                         # bend points for orthogonal wires\n"
"  # @  net=<wire_name>             # start a per-net section\n"
"  # @    h X1,Y → X2,Y             # horizontal segment (Y unchanged)\n"
"  # @    v X,Y1 → X,Y2             # vertical segment   (X unchanged)\n"
"  # @  net=<another>               # next net's segments...\n"
"\n"
"  # @display_mode = internal       # schematic view (default)\n"
"  # @display_mode = external       # black-box view (the box label is\n"
"                                   # @display_name if set, else the file\n"
"                                   # basename)\n"
"\n"
"  # @display_name = <free text>    # override the black-box label; omit\n"
"                                   # or leave blank to use the basename\n"
"\n"
"  # @pin_style = __input:<name> : clock\n"
"                                   # render the pin with a clock-edge\n"
"                                   # marker in the external view. The\n"
"                                   # only style today is `clock`; more\n"
"                                   # will be added.\n"
"\n"
"CONSTRAINTS (TODAY)\n"
"-------------------\n"
"\n"
"  - All gates take at most 2 inputs (DOMAIN_MAX_PINS_IN = 2).\n"
"  - Combinational only — no feedback loops. A wire cannot, directly\n"
"    or transitively, feed itself. Sequential circuits (CLK, latches,\n"
"    D-FF) are a Step-3 feature.\n"
"  - Wire names are case-sensitive and globally unique.\n"
"  - An `outputs:` name is automatically renamed to match whatever\n"
"    wire it consumes (\"smart rename\"). Easiest: name the producing\n"
"    wire the same as the output.\n"
"  - Input / output pin counts: at most 32 of each per circuit.\n"
"\n"
"WORKED EXAMPLE 1 — AND gate\n"
"----------------------------\n"
"\n"
"  inputs: a, b\n"
"  outputs: y\n"
"\n"
"  y = and(a, b)\n"
"\n"
"  # CLI:  dcs_cli circuits/and_gate.dcs --input \"a=1,b=1\"\n"
"  # ->   a=1 b=1 -> y=1\n"
"\n"
"WORKED EXAMPLE 2 — Half adder\n"
"------------------------------\n"
"\n"
"  inputs: a, b\n"
"  outputs: sum, carry\n"
"\n"
"  carry   = and(a, b)\n"
"  a_or_b  = or(a, b)\n"
"  n_carry = not(carry)\n"
"  sum     = and(a_or_b, n_carry)\n"
"\n"
"  # Multi-step:  --input \"a=0,1,0,1\" --input \"b=0,0,1,1\"\n"
"  # produces a 4-row timing table with one row per step.\n"
"\n"
"WORKED EXAMPLE 3 — 2:1 multiplexer\n"
"-----------------------------------\n"
"\n"
"  inputs: a, b, sel\n"
"  outputs: y\n"
"\n"
"  n_sel    = not(sel)\n"
"  a_pass   = and(a, n_sel)        # gated when sel = 0\n"
"  b_pass   = and(b, sel)          # gated when sel = 1\n"
"  y        = or(a_pass, b_pass)\n"
"\n"
"WORKED EXAMPLE 4 — black-box-ready cell (with annotations)\n"
"-----------------------------------------------------------\n"
"\n"
"  inputs: D, CLK\n"
"  outputs: Q, Qbar\n"
"\n"
"  # NOTE: not a real D-FF (no feedback). Stub for layout testing.\n"
"  Q    = and(D, CLK)\n"
"  Qbar = not(Q)\n"
"\n"
"  # @layout\n"
"  # @  Q              = 280, 240\n"
"  # @  Qbar           = 460, 320\n"
"  # @  __input:D      = 100, 220\n"
"  # @  __input:CLK    = 100, 340\n"
"  # @  __output:Q     = 460, 220\n"
"  # @  __output:Qbar  = 640, 320\n"
"\n"
"  # @display_mode = external\n"
"  # @display_name = DFF\n"
"  # @pin_style    = __input:CLK : clock\n"
"\n"
"  # @wires block is also accepted but the GUI re-routes automatically\n"
"  # on open when component positions changed, so AI-generated files can\n"
"  # safely omit it and let the GUI infer routes.\n"
"\n"
"OUTPUT FORMATS\n"
"--------------\n"
"\n"
"Single-step (every input has exactly one value, or no --input flag):\n"
"\n"
"  a=0 b=1 -> y=0\n"
"\n"
"Multi-step (any input has multiple comma-separated values):\n"
"\n"
"  step  a  b  sum  carry\n"
"     0  0  0    0      0\n"
"     1  1  0    1      0\n"
"     ...\n"
"\n"
"X (undefined) appears for any input / wire / output whose value the\n"
"simulator could not determine — typically an input that wasn't set.\n"
"\n"
"EXIT CODES\n"
"----------\n"
"\n"
"  0   success\n"
"  1   any error (missing file, parse error, bad --input, unknown flag)\n",
        out);
}

/* ── main ─────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    /* Help flags come first — handled before the positional-arg check so
       they work with zero args. Standard Unix convention: writes to stdout
       and exits 0. */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(stdout);
            return 0;
        }
        if (strcmp(argv[i], "--help-format") == 0) {
            print_format_help(stdout);
            return 0;
        }
    }
    if (argc < 2) { print_usage(stderr); return 1; }
    const char *filepath = argv[1];

    iplatform_t *p = platform_create();
    if (!p) { fprintf(stderr, "error: platform unavailable\n"); return 1; }

    input_spec_t specs[MAX_INPUT_SPECS];
    int       spec_count = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--input") == 0) {
            if (i + 1 >= argc) { print_usage(stderr); return 1; }
            if (parse_input_arg(argv[++i], specs, &spec_count, MAX_INPUT_SPECS) != 0) {
                fprintf(stderr, "error: invalid --input: %s\n", argv[i]);
                return 1;
            }
        } else {
            fprintf(stderr, "error: unknown argument: %s\n", argv[i]);
            print_usage(stderr);
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
