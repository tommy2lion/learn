/* Phase 2.4 port of the prototype's test_sim.c onto the new domain types.
 * Same coverage (33 tests) plus a few that exercise the new component_t
 * vtable (kind, pin counts, kind_name). */

#include <stdio.h>
#include <string.h>
#include "../src/domain/component.h"
#include "../src/domain/circuit.h"

static int failures = 0, total = 0;

static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

/* ── helpers: build single-component circuits ────────────────────── */

static signal_t eval2(component_kind_t kind, signal_t a, signal_t b) {
    circuit_t *c = circuit_create();
    circuit_add_input(c, "a");
    circuit_add_input(c, "b");
    circuit_add_output(c, "y");
    component_t *comp = NULL;
    switch (kind) {
        case COMP_AND: comp = gate_and_create("y"); break;
        case COMP_OR:  comp = gate_or_create ("y"); break;
        default: break;
    }
    circuit_add_component(c, comp, "a", "b");
    circuit_set_input(c, "a", a);
    circuit_set_input(c, "b", b);
    circuit_evaluate(c);
    signal_t r = circuit_get_output(c, "y");
    circuit_destroy(c);
    return r;
}

static signal_t eval1(component_kind_t kind, signal_t a) {
    circuit_t *c = circuit_create();
    circuit_add_input(c, "a");
    circuit_add_output(c, "y");
    component_t *comp = (kind == COMP_NOT) ? gate_not_create("y") : NULL;
    circuit_add_component(c, comp, "a", NULL);
    circuit_set_input(c, "a", a);
    circuit_evaluate(c);
    signal_t r = circuit_get_output(c, "y");
    circuit_destroy(c);
    return r;
}

/* ── tests ───────────────────────────────────────────────────────── */

static void test_and_gate(void) {
    check("and(0,0)=0", eval2(COMP_AND, 0, 0) == SIG_LOW);
    check("and(0,1)=0", eval2(COMP_AND, 0, 1) == SIG_LOW);
    check("and(1,0)=0", eval2(COMP_AND, 1, 0) == SIG_LOW);
    check("and(1,1)=1", eval2(COMP_AND, 1, 1) == SIG_HIGH);
}

static void test_or_gate(void) {
    check("or(0,0)=0", eval2(COMP_OR, 0, 0) == SIG_LOW);
    check("or(0,1)=1", eval2(COMP_OR, 0, 1) == SIG_HIGH);
    check("or(1,0)=1", eval2(COMP_OR, 1, 0) == SIG_HIGH);
    check("or(1,1)=1", eval2(COMP_OR, 1, 1) == SIG_HIGH);
}

static void test_not_gate(void) {
    check("not(0)=1", eval1(COMP_NOT, 0) == SIG_HIGH);
    check("not(1)=0", eval1(COMP_NOT, 1) == SIG_LOW);
}

static void test_nand_chain(void) {
    /* nand(a,b) = not(and(a,b)) */
    circuit_t *c = circuit_create();
    circuit_add_input(c, "a");
    circuit_add_input(c, "b");
    circuit_add_output(c, "y");
    circuit_add_component(c, gate_and_create("ab"), "a", "b");
    circuit_add_component(c, gate_not_create("y"),  "ab", NULL);

    signal_t cases[4][3] = {{0,0,1},{0,1,1},{1,0,1},{1,1,0}};
    for (int i = 0; i < 4; i++) {
        circuit_set_input(c, "a", cases[i][0]);
        circuit_set_input(c, "b", cases[i][1]);
        circuit_evaluate(c);
        char name[64];
        snprintf(name, sizeof(name), "nand(%d,%d)=%d",
                 cases[i][0], cases[i][1], cases[i][2]);
        check(name, circuit_get_output(c, "y") == cases[i][2]);
    }
    circuit_destroy(c);
}

static void test_half_adder(void) {
    circuit_t *c = circuit_create();
    circuit_add_input(c, "a");
    circuit_add_input(c, "b");
    circuit_add_output(c, "sum");
    circuit_add_output(c, "carry");
    circuit_add_component(c, gate_and_create("carry"),   "a", "b");
    circuit_add_component(c, gate_or_create ("a_or_b"),  "a", "b");
    circuit_add_component(c, gate_not_create("n_carry"), "carry", NULL);
    circuit_add_component(c, gate_and_create("sum"),     "a_or_b", "n_carry");

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

static void test_undefined_signal(void) {
    check("and(X,0)=0",  eval2(COMP_AND, SIG_UNDEF, SIG_LOW)  == SIG_LOW);
    check("and(0,X)=0",  eval2(COMP_AND, SIG_LOW,  SIG_UNDEF) == SIG_LOW);
    check("and(X,1)=X",  eval2(COMP_AND, SIG_UNDEF, SIG_HIGH) == SIG_UNDEF);
    check("and(1,X)=X",  eval2(COMP_AND, SIG_HIGH, SIG_UNDEF) == SIG_UNDEF);
    check("or(X,1)=1",   eval2(COMP_OR,  SIG_UNDEF, SIG_HIGH) == SIG_HIGH);
    check("or(1,X)=1",   eval2(COMP_OR,  SIG_HIGH,  SIG_UNDEF)== SIG_HIGH);
    check("or(X,0)=X",   eval2(COMP_OR,  SIG_UNDEF, SIG_LOW)  == SIG_UNDEF);
    check("not(X)=X",    eval1(COMP_NOT, SIG_UNDEF)            == SIG_UNDEF);
}

static void test_add_component_errors(void) {
    circuit_t *c = circuit_create();
    circuit_add_input(c, "a");
    circuit_add_input(c, "b");

    /* missing input wire */
    component_t *bad = gate_and_create("y");
    int rc = circuit_add_component(c, bad, "x", "b");
    check("add_component missing in1 -> -1", rc == -1);
    if (rc != 0) component_destroy(bad);

    /* duplicate output wire */
    circuit_add_component(c, gate_and_create("ab"), "a", "b");
    component_t *dup = gate_or_create("ab");
    int rc2 = circuit_add_component(c, dup, "a", "b");
    check("add_component duplicate out -> -1", rc2 == -1);
    if (rc2 != 0) component_destroy(dup);

    circuit_destroy(c);
}

static void test_component_metadata(void) {
    component_t *a = gate_and_create("g1");
    component_t *o = gate_or_create ("g2");
    component_t *n = gate_not_create("g3");
    check("AND kind",          component_kind(a) == COMP_AND);
    check("AND pin_count_in",  component_pin_count_in(a)  == 2);
    check("AND pin_count_out", component_pin_count_out(a) == 1);
    check("OR  kind",          component_kind(o) == COMP_OR);
    check("OR  pin_count_in",  component_pin_count_in(o)  == 2);
    check("NOT kind",          component_kind(n) == COMP_NOT);
    check("NOT pin_count_in",  component_pin_count_in(n)  == 1);
    check("NOT pin_count_out", component_pin_count_out(n) == 1);
    check("kind_name and",     strcmp(component_kind_name(COMP_AND), "and") == 0);
    check("kind_name or",      strcmp(component_kind_name(COMP_OR),  "or")  == 0);
    check("kind_name not",     strcmp(component_kind_name(COMP_NOT), "not") == 0);
    component_destroy(a);
    component_destroy(o);
    component_destroy(n);
}

int main(void) {
    printf("=== domain: circuit / component tests ===\n");
    test_and_gate();
    test_or_gate();
    test_not_gate();
    test_nand_chain();
    test_half_adder();
    test_undefined_signal();
    test_add_component_errors();
    test_component_metadata();
    printf("\n%d / %d passed\n", total - failures, total);
    return failures;
}
