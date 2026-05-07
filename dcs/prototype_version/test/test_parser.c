#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sim.h"
#include "parser.h"

static int failures = 0, total = 0;

static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

/* ── individual tests ─────────────────────────────────────────── */

static void test_parse_and_gate(void) {
    const char *src =
        "inputs: a, b\n"
        "outputs: y\n"
        "\n"
        "y = and(a, b)\n";
    char err[128] = {0};
    Circuit *c = parse_circuit(src, err, sizeof(err));
    check("and_gate: parse succeeds",    c != NULL);
    if (!c) { printf("  error: %s\n", err); return; }
    check("and_gate: 2 inputs",          c->input_count  == 2);
    check("and_gate: 1 output",          c->output_count == 1);
    check("and_gate: 1 gate",            c->gate_count   == 1);
    check("and_gate: gate type is AND",  c->gates[0].type == GATE_AND);

    circuit_set_input(c, "a", SIG_HIGH); circuit_set_input(c, "b", SIG_HIGH);
    circuit_run(c);
    check("and_gate: 1&1=1", circuit_get_output(c, "y") == SIG_HIGH);

    circuit_set_input(c, "a", SIG_LOW);
    circuit_run(c);
    check("and_gate: 0&1=0", circuit_get_output(c, "y") == SIG_LOW);

    circuit_free(c);
}

static void test_parse_not_gate(void) {
    const char *src =
        "inputs: a\n"
        "outputs: y\n"
        "y = not(a)\n";
    char err[128] = {0};
    Circuit *c = parse_circuit(src, err, sizeof(err));
    check("not_gate: parse succeeds",   c != NULL);
    if (!c) { printf("  error: %s\n", err); return; }
    check("not_gate: 1 gate",           c->gate_count == 1);
    check("not_gate: gate type is NOT", c->gates[0].type == GATE_NOT);

    circuit_set_input(c, "a", SIG_LOW); circuit_run(c);
    check("not_gate: not(0)=1", circuit_get_output(c, "y") == SIG_HIGH);
    circuit_set_input(c, "a", SIG_HIGH); circuit_run(c);
    check("not_gate: not(1)=0", circuit_get_output(c, "y") == SIG_LOW);
    circuit_free(c);
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
    Circuit *c = parse_circuit(src, err, sizeof(err));
    check("half_adder: parse succeeds", c != NULL);
    if (!c) { printf("  error: %s\n", err); return; }
    check("half_adder: 2 inputs",  c->input_count  == 2);
    check("half_adder: 2 outputs", c->output_count == 2);
    check("half_adder: 4 gates",   c->gate_count   == 4);

    /* verify all 4 input combinations: {a, b, expected_sum, expected_carry} */
    Signal cases[4][4] = {{0,0,0,0},{1,0,1,0},{0,1,1,0},{1,1,0,1}};
    for (int i = 0; i < 4; i++) {
        circuit_set_input(c, "a", cases[i][0]);
        circuit_set_input(c, "b", cases[i][1]);
        circuit_run(c);
        char name[64];
        snprintf(name, sizeof(name), "half_adder(%d,%d) sum=%d",
                 cases[i][0], cases[i][1], cases[i][2]);
        check(name, circuit_get_output(c, "sum") == cases[i][2]);
        snprintf(name, sizeof(name), "half_adder(%d,%d) carry=%d",
                 cases[i][0], cases[i][1], cases[i][3]);
        check(name, circuit_get_output(c, "carry") == cases[i][3]);
    }
    circuit_free(c);
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
    Circuit *c = parse_circuit(src, err, sizeof(err));
    check("comments: parse succeeds",  c != NULL);
    if (!c) { printf("  error: %s\n", err); return; }
    check("comments: 1 input",         c->input_count  == 1);
    check("comments: 1 output",        c->output_count == 1);
    check("comments: 1 gate",          c->gate_count   == 1);
    check("comments: gate type NOT",   c->gates[0].type == GATE_NOT);
    circuit_free(c);
}

static void test_serialize(void) {
    /* Build a circuit via API, then serialize it. */
    Circuit *c = circuit_new();
    circuit_add_input(c,  "a");
    circuit_add_input(c,  "b");
    circuit_add_output(c, "y");
    circuit_add_gate(c, GATE_OR, "y", "a", "b");

    char *text = serialize_circuit(c);
    check("serialize: non-NULL",         text != NULL);
    if (!text) { circuit_free(c); return; }
    check("serialize: has 'inputs:'",    strstr(text, "inputs:")  != NULL);
    check("serialize: has 'outputs:'",   strstr(text, "outputs:") != NULL);
    check("serialize: has 'or'",         strstr(text, "or")       != NULL);
    check("serialize: has wire name 'y'",strstr(text, "y")        != NULL);
    circuit_free(c);
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
    Circuit *c1 = parse_circuit(src, err, sizeof(err));
    if (!c1) { check("round_trip: first parse", 0); printf("  %s\n", err); return; }

    char *text2 = serialize_circuit(c1);
    check("round_trip: serialize", text2 != NULL);
    if (!text2) { circuit_free(c1); return; }

    Circuit *c2 = parse_circuit(text2, err, sizeof(err));
    check("round_trip: second parse", c2 != NULL);
    if (!c2) { printf("  %s\nSerialized:\n%s\n", err, text2); circuit_free(c1); free(text2); return; }

    check("round_trip: same input count",  c1->input_count  == c2->input_count);
    check("round_trip: same output count", c1->output_count == c2->output_count);
    check("round_trip: same gate count",   c1->gate_count   == c2->gate_count);

    Signal cases[4][4] = {{0,0,0,0},{1,0,1,0},{0,1,1,0},{1,1,0,1}};
    for (int i = 0; i < 4; i++) {
        circuit_set_input(c1, "a", cases[i][0]); circuit_set_input(c1, "b", cases[i][1]);
        circuit_set_input(c2, "a", cases[i][0]); circuit_set_input(c2, "b", cases[i][1]);
        circuit_run(c1); circuit_run(c2);
        char name[64];
        snprintf(name, sizeof(name), "round_trip(%d,%d) sum",   cases[i][0], cases[i][1]);
        check(name, circuit_get_output(c1, "sum") == circuit_get_output(c2, "sum"));
        snprintf(name, sizeof(name), "round_trip(%d,%d) carry", cases[i][0], cases[i][1]);
        check(name, circuit_get_output(c1, "carry") == circuit_get_output(c2, "carry"));
    }
    circuit_free(c1); circuit_free(c2); free(text2);
}

static void test_error_undef_wire(void) {
    /* 'x' is not declared as an input */
    const char *src =
        "inputs: a\n"
        "outputs: y\n"
        "y = and(a, x)\n";
    char err[128] = {0};
    Circuit *c = parse_circuit(src, err, sizeof(err));
    check("undef_wire: returns NULL",       c == NULL);
    check("undef_wire: error msg not empty", err[0] != '\0');
    if (c) circuit_free(c);
}

static void test_error_syntax(void) {
    char err[128];

    /* missing '=' */
    const char *s1 = "inputs: a\noutputs: y\nnot(a)\n";
    Circuit *c = parse_circuit(s1, err, sizeof(err));
    check("syntax: missing '=' -> NULL", c == NULL);
    if (c) circuit_free(c);

    /* unknown gate name */
    const char *s2 = "inputs: a, b\noutputs: y\ny = xor(a, b)\n";
    c = parse_circuit(s2, err, sizeof(err));
    check("syntax: unknown gate -> NULL", c == NULL);
    if (c) circuit_free(c);

    /* missing inputs: declaration */
    const char *s3 = "outputs: y\n";
    c = parse_circuit(s3, err, sizeof(err));
    check("syntax: missing inputs: -> NULL", c == NULL);
    if (c) circuit_free(c);

    /* missing outputs: declaration */
    const char *s4 = "inputs: a\n";
    c = parse_circuit(s4, err, sizeof(err));
    check("syntax: missing outputs: -> NULL", c == NULL);
    if (c) circuit_free(c);

    /* wrong arg count for NOT */
    const char *s5 = "inputs: a, b\noutputs: y\ny = not(a, b)\n";
    c = parse_circuit(s5, err, sizeof(err));
    check("syntax: not(a,b) -> NULL", c == NULL);
    if (c) circuit_free(c);
}

int main(void) {
    printf("=== parser unit tests ===\n");
    test_parse_and_gate();
    test_parse_not_gate();
    test_parse_half_adder();
    test_parse_comments();
    test_serialize();
    test_round_trip();
    test_error_undef_wire();
    test_error_syntax();
    printf("\n%d / %d passed\n", total - failures, total);
    return failures;
}
