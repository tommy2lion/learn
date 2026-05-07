/* Phase 2.4 port of the prototype's test_parser.c onto circuit_io. Same
   coverage (54 tests) over parse, serialize, round-trip, and error paths. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/domain/component.h"
#include "../src/domain/circuit.h"
#include "../src/domain/circuit_io.h"

static int failures = 0, total = 0;

static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

/* ── tests ───────────────────────────────────────────────────────── */

static void test_parse_and_gate(void) {
    const char *src =
        "inputs: a, b\n"
        "outputs: y\n"
        "\n"
        "y = and(a, b)\n";
    char err[128] = {0};
    circuit_t *c = circuit_io_parse(src, err, sizeof(err));
    check("and_gate: parse succeeds",   c != NULL);
    if (!c) { printf("  error: %s\n", err); return; }
    check("and_gate: 2 inputs",         c->input_count == 2);
    check("and_gate: 1 output",         c->output_count == 1);
    check("and_gate: 1 component",      c->component_count == 1);
    check("and_gate: kind is AND",      component_kind(c->components[0]) == COMP_AND);

    circuit_set_input(c, "a", SIG_HIGH); circuit_set_input(c, "b", SIG_HIGH);
    circuit_evaluate(c);
    check("and_gate: 1&1=1", circuit_get_output(c, "y") == SIG_HIGH);

    circuit_set_input(c, "a", SIG_LOW);
    circuit_evaluate(c);
    check("and_gate: 0&1=0", circuit_get_output(c, "y") == SIG_LOW);

    circuit_destroy(c);
}

static void test_parse_not_gate(void) {
    const char *src =
        "inputs: a\n"
        "outputs: y\n"
        "y = not(a)\n";
    char err[128] = {0};
    circuit_t *c = circuit_io_parse(src, err, sizeof(err));
    check("not_gate: parse succeeds",  c != NULL);
    if (!c) { printf("  error: %s\n", err); return; }
    check("not_gate: 1 component",     c->component_count == 1);
    check("not_gate: kind is NOT",     component_kind(c->components[0]) == COMP_NOT);

    circuit_set_input(c, "a", SIG_LOW); circuit_evaluate(c);
    check("not_gate: not(0)=1", circuit_get_output(c, "y") == SIG_HIGH);
    circuit_set_input(c, "a", SIG_HIGH); circuit_evaluate(c);
    check("not_gate: not(1)=0", circuit_get_output(c, "y") == SIG_LOW);
    circuit_destroy(c);
}

static void test_parse_half_adder(void) {
    const char *src =
        "inputs: a, b\n"
        "outputs: sum, carry\n"
        "\n"
        "carry   = and(a, b)\n"
        "a_or_b  = or(a, b)\n"
        "n_carry = not(carry)\n"
        "sum     = and(a_or_b, n_carry)\n";
    char err[128] = {0};
    circuit_t *c = circuit_io_parse(src, err, sizeof(err));
    check("half_adder: parse succeeds", c != NULL);
    if (!c) { printf("  error: %s\n", err); return; }
    check("half_adder: 2 inputs",     c->input_count == 2);
    check("half_adder: 2 outputs",    c->output_count == 2);
    check("half_adder: 4 components", c->component_count == 4);

    signal_t cases[4][4] = {{0,0,0,0},{1,0,1,0},{0,1,1,0},{1,1,0,1}};
    for (int i = 0; i < 4; i++) {
        circuit_set_input(c, "a", cases[i][0]);
        circuit_set_input(c, "b", cases[i][1]);
        circuit_evaluate(c);
        char name[64];
        snprintf(name, sizeof(name), "half_adder(%d,%d) sum=%d",
                 cases[i][0], cases[i][1], cases[i][2]);
        check(name, circuit_get_output(c, "sum") == cases[i][2]);
        snprintf(name, sizeof(name), "half_adder(%d,%d) carry=%d",
                 cases[i][0], cases[i][1], cases[i][3]);
        check(name, circuit_get_output(c, "carry") == cases[i][3]);
    }
    circuit_destroy(c);
}

static void test_parse_comments(void) {
    const char *src =
        "# full-line comment\n"
        "inputs: a  # inline comment\n"
        "outputs: y\n"
        "\n"
        "# another comment\n"
        "y = not(a)\n";
    char err[128] = {0};
    circuit_t *c = circuit_io_parse(src, err, sizeof(err));
    check("comments: parse succeeds", c != NULL);
    if (!c) { printf("  error: %s\n", err); return; }
    check("comments: 1 input",      c->input_count == 1);
    check("comments: 1 output",     c->output_count == 1);
    check("comments: 1 component",  c->component_count == 1);
    check("comments: kind NOT",     component_kind(c->components[0]) == COMP_NOT);
    circuit_destroy(c);
}

static void test_serialize(void) {
    circuit_t *c = circuit_create();
    circuit_add_input(c,  "a");
    circuit_add_input(c,  "b");
    circuit_add_output(c, "y");
    circuit_add_component(c, gate_or_create("y"), "a", "b");

    char *text = circuit_io_serialize(c);
    check("serialize: non-NULL",          text != NULL);
    if (!text) { circuit_destroy(c); return; }
    check("serialize: has 'inputs:'",     strstr(text, "inputs:")  != NULL);
    check("serialize: has 'outputs:'",    strstr(text, "outputs:") != NULL);
    check("serialize: has 'or'",          strstr(text, "or")       != NULL);
    check("serialize: has wire 'y'",      strstr(text, "y")        != NULL);
    circuit_destroy(c);
    free(text);
}

static void test_round_trip(void) {
    const char *src =
        "inputs: a, b\n"
        "outputs: sum, carry\n"
        "\n"
        "carry = and(a, b)\n"
        "a_or_b = or(a, b)\n"
        "n_carry = not(carry)\n"
        "sum = and(a_or_b, n_carry)\n";

    char err[128] = {0};
    circuit_t *c1 = circuit_io_parse(src, err, sizeof(err));
    if (!c1) { check("round_trip: first parse", 0); printf("  %s\n", err); return; }

    char *text2 = circuit_io_serialize(c1);
    check("round_trip: serialize", text2 != NULL);
    if (!text2) { circuit_destroy(c1); return; }

    circuit_t *c2 = circuit_io_parse(text2, err, sizeof(err));
    check("round_trip: second parse", c2 != NULL);
    if (!c2) { printf("  %s\nSerialized:\n%s\n", err, text2); circuit_destroy(c1); free(text2); return; }

    check("round_trip: same input count",     c1->input_count == c2->input_count);
    check("round_trip: same output count",    c1->output_count == c2->output_count);
    check("round_trip: same component count", c1->component_count == c2->component_count);

    signal_t cases[4][4] = {{0,0,0,0},{1,0,1,0},{0,1,1,0},{1,1,0,1}};
    for (int i = 0; i < 4; i++) {
        circuit_set_input(c1, "a", cases[i][0]); circuit_set_input(c1, "b", cases[i][1]);
        circuit_set_input(c2, "a", cases[i][0]); circuit_set_input(c2, "b", cases[i][1]);
        circuit_evaluate(c1); circuit_evaluate(c2);
        char name[64];
        snprintf(name, sizeof(name), "round_trip(%d,%d) sum",   cases[i][0], cases[i][1]);
        check(name, circuit_get_output(c1, "sum") == circuit_get_output(c2, "sum"));
        snprintf(name, sizeof(name), "round_trip(%d,%d) carry", cases[i][0], cases[i][1]);
        check(name, circuit_get_output(c1, "carry") == circuit_get_output(c2, "carry"));
    }
    circuit_destroy(c1); circuit_destroy(c2); free(text2);
}

static void test_error_undef_wire(void) {
    const char *src =
        "inputs: a\n"
        "outputs: y\n"
        "y = and(a, x)\n";
    char err[128] = {0};
    circuit_t *c = circuit_io_parse(src, err, sizeof(err));
    check("undef_wire: returns NULL",        c == NULL);
    check("undef_wire: error msg not empty", err[0] != '\0');
    if (c) circuit_destroy(c);
}

static void test_error_syntax(void) {
    char err[128];

    /* missing '=' */
    const char *s1 = "inputs: a\noutputs: y\nnot(a)\n";
    circuit_t *c = circuit_io_parse(s1, err, sizeof(err));
    check("syntax: missing '=' -> NULL", c == NULL);
    if (c) circuit_destroy(c);

    /* unknown gate name */
    const char *s2 = "inputs: a, b\noutputs: y\ny = xor(a, b)\n";
    c = circuit_io_parse(s2, err, sizeof(err));
    check("syntax: unknown gate -> NULL", c == NULL);
    if (c) circuit_destroy(c);

    /* missing inputs: */
    const char *s3 = "outputs: y\n";
    c = circuit_io_parse(s3, err, sizeof(err));
    check("syntax: missing inputs: -> NULL", c == NULL);
    if (c) circuit_destroy(c);

    /* missing outputs: */
    const char *s4 = "inputs: a\n";
    c = circuit_io_parse(s4, err, sizeof(err));
    check("syntax: missing outputs: -> NULL", c == NULL);
    if (c) circuit_destroy(c);

    /* wrong arg count for NOT */
    const char *s5 = "inputs: a, b\noutputs: y\ny = not(a, b)\n";
    c = circuit_io_parse(s5, err, sizeof(err));
    check("syntax: not(a,b) -> NULL", c == NULL);
    if (c) circuit_destroy(c);
}

/* ── Phase 2.6: layout annotation block ─────────────────────────── */

static void test_parse_layout_block(void) {
    const char *src =
        "inputs: a, b\n"
        "outputs: y\n"
        "\n"
        "y = and(a, b)\n"
        "\n"
        "# @layout\n"
        "# @  y = 460, 380\n"
        "# @  __input:a = 100, 320\n"
        "# @  __input:b = 100, 440\n"
        "# @  __output:y = 820, 380\n";
    char err[128] = {0};
    circuit_t *c = circuit_io_parse(src, err, sizeof(err));
    check("layout: parse succeeds", c != NULL);
    if (!c) { printf("  error: %s\n", err); return; }
    check("layout: comp y x=460", c->components[0]->position.x == 460.0f);
    check("layout: comp y y=380", c->components[0]->position.y == 380.0f);
    check("layout: input a pos",  c->input_positions[0].x == 100.0f
                               && c->input_positions[0].y == 320.0f);
    check("layout: input b pos",  c->input_positions[1].x == 100.0f
                               && c->input_positions[1].y == 440.0f);
    check("layout: output y pos", c->output_positions[0].x == 820.0f
                               && c->output_positions[0].y == 380.0f);
    circuit_destroy(c);
}

static void test_unknown_annotation_ignored(void) {
    /* `# @future = stuff` should be skipped without error */
    const char *src =
        "inputs: a\n"
        "outputs: y\n"
        "y = not(a)\n"
        "# @somefuturetag\n"
        "# @  another = 1, 2\n"   /* not in @layout, so ignored */
        "# random comment\n";
    char err[128] = {0};
    circuit_t *c = circuit_io_parse(src, err, sizeof(err));
    check("unknown annotation: parse succeeds", c != NULL);
    if (c) circuit_destroy(c);
}

static void test_serialize_with_layout(void) {
    /* build a circuit, set positions, serialize, look for the layout block */
    circuit_t *c = circuit_create();
    circuit_add_input(c,  "a");
    circuit_add_input(c,  "b");
    circuit_add_output(c, "y");
    circuit_add_component(c, gate_and_create("y"), "a", "b");
    /* set a few positions */
    c->components[0]->position = (vec2_t){460, 380};
    c->input_positions[0]      = (vec2_t){100, 320};
    c->input_positions[1]      = (vec2_t){100, 440};
    c->output_positions[0]     = (vec2_t){820, 380};

    char *text = circuit_io_serialize(c);
    check("ser-layout: non-NULL",        text != NULL);
    if (!text) { circuit_destroy(c); return; }
    check("ser-layout: has '# @layout'", strstr(text, "# @layout") != NULL);
    check("ser-layout: has comp entry",  strstr(text, "# @  y = 460, 380") != NULL);
    check("ser-layout: has __input:a",   strstr(text, "# @  __input:a = 100, 320") != NULL);
    check("ser-layout: has __output:y",  strstr(text, "# @  __output:y = 820, 380") != NULL);
    free(text);
    circuit_destroy(c);
}

static void test_no_layout_when_zero_positions(void) {
    /* fresh circuit with default (zero) positions: no layout block */
    circuit_t *c = circuit_create();
    circuit_add_input(c, "a");
    circuit_add_output(c, "y");
    circuit_add_component(c, gate_not_create("y"), "a", NULL);
    char *text = circuit_io_serialize(c);
    check("zero-pos: no layout block",   strstr(text, "# @layout") == NULL);
    free(text);
    circuit_destroy(c);
}

static void test_round_trip_with_positions(void) {
    /* build → set positions → serialize → parse → positions identical */
    circuit_t *c1 = circuit_create();
    circuit_add_input(c1,  "a");
    circuit_add_input(c1,  "b");
    circuit_add_output(c1, "sum");
    circuit_add_output(c1, "carry");
    circuit_add_component(c1, gate_and_create("carry"),   "a", "b");
    circuit_add_component(c1, gate_or_create ("a_or_b"),  "a", "b");
    circuit_add_component(c1, gate_not_create("n_carry"), "carry", NULL);
    circuit_add_component(c1, gate_and_create("sum"),     "a_or_b", "n_carry");

    c1->input_positions[0]  = (vec2_t){100,  320};
    c1->input_positions[1]  = (vec2_t){100,  440};
    c1->components[0]->position = (vec2_t){280, 320};
    c1->components[1]->position = (vec2_t){280, 440};
    c1->components[2]->position = (vec2_t){460, 320};
    c1->components[3]->position = (vec2_t){640, 380};
    c1->output_positions[0] = (vec2_t){820, 380};
    c1->output_positions[1] = (vec2_t){820, 320};

    char *text = circuit_io_serialize(c1);
    check("rt-layout: serialize", text != NULL);
    if (!text) { circuit_destroy(c1); return; }

    char err[128] = {0};
    circuit_t *c2 = circuit_io_parse(text, err, sizeof(err));
    check("rt-layout: re-parse",  c2 != NULL);
    if (!c2) { printf("  err=%s\n", err); free(text); circuit_destroy(c1); return; }

    check("rt-layout: in_a pos",     c2->input_positions[0].x == 100 && c2->input_positions[0].y == 320);
    check("rt-layout: in_b pos",     c2->input_positions[1].x == 100 && c2->input_positions[1].y == 440);
    check("rt-layout: carry pos",    c2->components[0]->position.x == 280 && c2->components[0]->position.y == 320);
    check("rt-layout: a_or_b pos",   c2->components[1]->position.x == 280 && c2->components[1]->position.y == 440);
    check("rt-layout: n_carry pos",  c2->components[2]->position.x == 460 && c2->components[2]->position.y == 320);
    check("rt-layout: sum pos",      c2->components[3]->position.x == 640 && c2->components[3]->position.y == 380);
    check("rt-layout: out sum pos",  c2->output_positions[0].x == 820 && c2->output_positions[0].y == 380);
    check("rt-layout: out carry pos",c2->output_positions[1].x == 820 && c2->output_positions[1].y == 320);

    free(text);
    circuit_destroy(c1);
    circuit_destroy(c2);
}

static void test_load_no_layout_then_save_no_layout(void) {
    /* prototype-style file (no layout block) loads fine; positions stay zero;
       saving back produces no layout block — preserving the lean format. */
    const char *src =
        "inputs: a, b\n"
        "outputs: y\n"
        "y = and(a, b)\n";
    char err[128] = {0};
    circuit_t *c = circuit_io_parse(src, err, sizeof(err));
    check("no-layout-load: ok", c != NULL);
    if (!c) { printf("  err=%s\n", err); return; }
    check("no-layout-load: comp pos zero",
          c->components[0]->position.x == 0 && c->components[0]->position.y == 0);
    char *text = circuit_io_serialize(c);
    check("no-layout-load: re-serialize has no layout",
          strstr(text, "# @layout") == NULL);
    free(text);
    circuit_destroy(c);
}

int main(void) {
    printf("=== domain: circuit_io tests ===\n");
    test_parse_and_gate();
    test_parse_not_gate();
    test_parse_half_adder();
    test_parse_comments();
    test_serialize();
    test_round_trip();
    test_error_undef_wire();
    test_error_syntax();
    /* Phase 2.6 */
    test_parse_layout_block();
    test_unknown_annotation_ignored();
    test_serialize_with_layout();
    test_no_layout_when_zero_positions();
    test_round_trip_with_positions();
    test_load_no_layout_then_save_no_layout();
    printf("\n%d / %d passed\n", total - failures, total);
    return failures;
}
