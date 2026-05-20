/* Phase 2.4 port of the prototype's test_parser.c onto circuit_io. Same
   coverage (54 tests) over parse, serialize, round-trip, and error paths. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/domain/component.h"
#include "../src/domain/circuit.h"
#include "../src/domain/circuit_io.h"
#include "../src/domain/wire_geometry.h"

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

/* ── supplement Phase 7: # @wires round-trip ─────────────────────── */

static void test_serialize_with_wires(void) {
    /* Build a tiny circuit: 1 input → 1 NOT → 1 output. */
    circuit_t *c = circuit_create();
    circuit_add_input (c, "a");
    circuit_add_output(c, "y");
    component_t *n1 = gate_not_create("n1");
    circuit_add_component(c, n1, "a", NULL);
    snprintf(c->output_names[0], DOMAIN_NAME_LEN, "n1");

    /* Hand-build a tiny wire geometry: two nets, mix of H and V. */
    wire_geometry_t g; wire_geometry_init(&g);
    int a_idx  = wire_geometry_get_or_create(&g, "a");
    int n1_idx = wire_geometry_get_or_create(&g, "n1");
    wire_segment_t a_segs[] = {
        {{100, 50}, {180, 50}},        /* H */
        {{180, 50}, {180, 80}},        /* V */
    };
    wire_segment_t n_segs[] = {
        {{240, 80}, {300, 80}},        /* H */
    };
    wire_geometry_set_segments(&g, a_idx,  a_segs, 2);
    wire_geometry_set_segments(&g, n1_idx, n_segs, 1);

    char *text = circuit_io_serialize_ex(c, &g, NULL);
    check("serialize_ex: returned non-NULL", text != NULL);
    if (text) {
        check("serialize_ex: contains '# @wires'",
              strstr(text, "\n# @wires\n") != NULL);
        check("serialize_ex: contains 'net=a'",
              strstr(text, "# @  net=a\n")  != NULL);
        check("serialize_ex: contains 'net=n1'",
              strstr(text, "# @  net=n1\n") != NULL);
        check("serialize_ex: emits H segment with UTF-8 arrow",
              strstr(text, "h 100,50 \xe2\x86\x92 180,50") != NULL);
        check("serialize_ex: emits V segment",
              strstr(text, "v 180,50 \xe2\x86\x92 180,80") != NULL);
        free(text);
    }

    wire_geometry_release(&g);
    circuit_destroy(c);
}

static void test_round_trip_with_wires(void) {
    /* Build the same shape, serialise with geometry, reparse, compare. */
    circuit_t *c = circuit_create();
    circuit_add_input (c, "a");
    circuit_add_output(c, "y");
    component_t *n1 = gate_not_create("n1");
    circuit_add_component(c, n1, "a", NULL);
    snprintf(c->output_names[0], DOMAIN_NAME_LEN, "n1");

    wire_geometry_t g; wire_geometry_init(&g);
    int a_idx  = wire_geometry_get_or_create(&g, "a");
    int n1_idx = wire_geometry_get_or_create(&g, "n1");
    wire_segment_t a_segs[] = {
        {{100, 50}, {180, 50}},
        {{180, 50}, {180, 80}},
    };
    wire_segment_t n_segs[] = {
        {{240, 80}, {300, 80}},
    };
    wire_geometry_set_segments(&g, a_idx,  a_segs, 2);
    wire_geometry_set_segments(&g, n1_idx, n_segs, 1);

    char *text = circuit_io_serialize_ex(c, &g, NULL);
    check("round-trip: serialize_ex returned non-NULL", text != NULL);

    /* Re-parse via _ex into a fresh circuit + geometry. */
    char err[128] = {0};
    wire_geometry_t g2; wire_geometry_init(&g2);
    circuit_t *c2 = circuit_io_parse_ex(text, err, sizeof(err), &g2, NULL);
    check("round-trip: parse_ex returned non-NULL", c2 != NULL);
    free(text);

    if (c2) {
        check("round-trip: 2 nets restored",  g2.net_count == 2);
        int rt_a  = wire_geometry_find(&g2, "a");
        int rt_n1 = wire_geometry_find(&g2, "n1");
        check("round-trip: net 'a' found",   rt_a  >= 0);
        check("round-trip: net 'n1' found",  rt_n1 >= 0);
        const wire_net_geom_t *na = wire_geometry_net(&g2, rt_a);
        const wire_net_geom_t *nn = wire_geometry_net(&g2, rt_n1);
        check("round-trip: 'a' has 2 segs",  na && na->seg_count == 2);
        check("round-trip: 'n1' has 1 seg",  nn && nn->seg_count == 1);
        check("round-trip: a seg0 endpoints",
              na && na->segs[0].a.x == 100 && na->segs[0].a.y == 50
                 && na->segs[0].b.x == 180 && na->segs[0].b.y == 50);
        check("round-trip: a seg1 endpoints",
              na && na->segs[1].a.x == 180 && na->segs[1].a.y == 50
                 && na->segs[1].b.x == 180 && na->segs[1].b.y == 80);
        check("round-trip: n1 seg0 endpoints",
              nn && nn->segs[0].a.x == 240 && nn->segs[0].a.y == 80
                 && nn->segs[0].b.x == 300 && nn->segs[0].b.y == 80);
        circuit_destroy(c2);
    }
    wire_geometry_release(&g);
    wire_geometry_release(&g2);
    circuit_destroy(c);
}

static void test_parse_wires_ascii_arrow(void) {
    /* ASCII -> arrow alternate; mixed with the UTF-8 → in another net. */
    const char *src =
        "inputs: a\n"
        "outputs: y\n"
        "\n"
        "y = not(a)\n"
        "\n"
        "# @wires\n"
        "# @  net=a\n"
        "# @    h 0,0 -> 50,0\n"
        "# @  net=y\n"
        "# @    h 60,0 \xe2\x86\x92 100,0\n";
    char err[128] = {0};
    wire_geometry_t g; wire_geometry_init(&g);
    circuit_t *c = circuit_io_parse_ex(src, err, sizeof(err), &g, NULL);
    check("ascii arrow: parse_ex succeeds", c != NULL);
    check("ascii arrow: net a parsed",     wire_geometry_find(&g, "a") >= 0);
    check("ascii arrow: net y parsed",     wire_geometry_find(&g, "y") >= 0);
    int ai = wire_geometry_find(&g, "a");
    const wire_net_geom_t *na = wire_geometry_net(&g, ai);
    check("ascii arrow: net a has 1 seg",  na && na->seg_count == 1);
    wire_geometry_release(&g);
    if (c) circuit_destroy(c);
}

static void test_parse_no_wires_block(void) {
    /* Layout but no wires: parse_ex succeeds, geometry stays empty. */
    const char *src =
        "inputs: a, b\n"
        "outputs: y\n"
        "\n"
        "y = and(a, b)\n"
        "\n"
        "# @layout\n"
        "# @  y = 200, 100\n";
    char err[128] = {0};
    wire_geometry_t g; wire_geometry_init(&g);
    circuit_t *c = circuit_io_parse_ex(src, err, sizeof(err), &g, NULL);
    check("no-wires: parse_ex succeeds",         c != NULL);
    check("no-wires: geometry stays empty",       g.net_count == 0);
    check("no-wires: layout still parsed",
          c != NULL && c->components[0]->position.x == 200);
    wire_geometry_release(&g);
    if (c) circuit_destroy(c);
}

static void test_parse_ex_with_null_geom(void) {
    /* parse_ex with geom_out = NULL — wires block is skipped silently
       (legacy circuit_io_parse path). */
    const char *src =
        "inputs: a\n"
        "outputs: y\n"
        "\n"
        "y = not(a)\n"
        "\n"
        "# @wires\n"
        "# @  net=a\n"
        "# @    h 0,0 -> 50,0\n";
    char err[128] = {0};
    circuit_t *c = circuit_io_parse_ex(src, err, sizeof(err), NULL, NULL);
    check("parse_ex(geom=NULL): still parses circuit", c != NULL);
    if (c) circuit_destroy(c);
}

static void test_legacy_serialize_byte_identical(void) {
    /* Confirm circuit_io_serialize(c) == circuit_io_serialize_ex(c, NULL, NULL). */
    circuit_t *c = circuit_create();
    circuit_add_input(c, "a");
    circuit_add_output(c, "y");
    component_t *n1 = gate_not_create("n1");
    circuit_add_component(c, n1, "a", NULL);
    snprintf(c->output_names[0], DOMAIN_NAME_LEN, "n1");
    char *legacy = circuit_io_serialize(c);
    char *ex_null = circuit_io_serialize_ex(c, NULL, NULL);
    check("legacy serialize == _ex(c, NULL)",
          legacy && ex_null && strcmp(legacy, ex_null) == 0);
    free(legacy);
    free(ex_null);
    circuit_destroy(c);
}

/* ── supplement Phase 10: display_mode / display_name / pin_style round-trip ── */

/* Build inputs: D, CLK; outputs: Q. Output Q consumes a NOT(D). */
static circuit_t *make_meta_test_circuit(void) {
    circuit_t *c = circuit_create();
    circuit_add_input (c, "D");
    circuit_add_input (c, "CLK");
    circuit_add_output(c, "Q");
    component_t *n1 = gate_not_create("Q");
    circuit_add_component(c, n1, "D", NULL);
    return c;
}

static void test_serialize_meta_block(void) {
    circuit_t *c = make_meta_test_circuit();
    circuit_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.display_mode = 1;                                        /* external */
    snprintf(meta.display_name, sizeof(meta.display_name), "%s", "DFF");
    meta.input_styles[1] = 1;                                     /* CLK = clock */

    char *text = circuit_io_serialize_ex(c, NULL, &meta);
    check("meta-serialize: non-NULL", text != NULL);
    if (text) {
        check("meta-serialize: emits display_mode = external",
              strstr(text, "# @display_mode = external") != NULL);
        check("meta-serialize: emits display_name = DFF",
              strstr(text, "# @display_name = DFF") != NULL);
        check("meta-serialize: emits pin_style for CLK",
              strstr(text, "# @pin_style = __input:CLK : clock") != NULL);
        check("meta-serialize: no pin_style line for un-styled D",
              strstr(text, "__input:D :") == NULL);
        free(text);
    }
    circuit_destroy(c);
}

static void test_meta_block_omitted_when_defaults(void) {
    /* When meta is all zeros, the serializer should output nothing extra
       — keep legacy files clean. */
    circuit_t *c = make_meta_test_circuit();
    circuit_meta_t meta;
    memset(&meta, 0, sizeof(meta));    /* all defaults */
    char *text = circuit_io_serialize_ex(c, NULL, &meta);
    check("meta-default: serializer returns non-NULL", text != NULL);
    if (text) {
        check("meta-default: no # @display_mode line",
              strstr(text, "# @display_mode") == NULL);
        check("meta-default: no # @display_name line",
              strstr(text, "# @display_name") == NULL);
        check("meta-default: no # @pin_style line",
              strstr(text, "# @pin_style") == NULL);
        free(text);
    }
    circuit_destroy(c);
}

static void test_round_trip_meta(void) {
    circuit_t *c = make_meta_test_circuit();
    circuit_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.display_mode    = 1;
    snprintf(meta.display_name, sizeof(meta.display_name), "%s", "MyDFF");
    meta.input_styles[1] = 1;                                     /* CLK clock */
    meta.output_styles[0] = 2;                                    /* Q inverted */

    char *text = circuit_io_serialize_ex(c, NULL, &meta);
    check("rt-meta: serialize succeeds", text != NULL);

    char err[128] = {0};
    circuit_meta_t got;
    memset(&got, 0xCC, sizeof(got));
    /* Re-parse and pull the metadata back. We zero only after passing it
       (the parser only writes the fields it found, so a defensive
       caller-side memset(0) is part of the round-trip contract). */
    memset(&got, 0, sizeof(got));
    circuit_t *c2 = circuit_io_parse_ex(text, err, sizeof(err), NULL, &got);
    check("rt-meta: parse succeeds", c2 != NULL);
    free(text);

    if (c2) {
        check("rt-meta: display_mode preserved",      got.display_mode == 1);
        check("rt-meta: display_name preserved",
              strcmp(got.display_name, "MyDFF") == 0);
        check("rt-meta: input[1] (CLK) is clock",     got.input_styles[1] == 1);
        check("rt-meta: input[0] (D)   stays normal", got.input_styles[0] == 0);
        check("rt-meta: output[0] (Q)  is inverted",  got.output_styles[0] == 2);
        circuit_destroy(c2);
    }
    circuit_destroy(c);
}

static void test_meta_parse_legacy_file(void) {
    /* A circuit file with NO metadata block parses fine and meta stays
       at all-zero defaults. */
    const char *src =
        "inputs: a, b\n"
        "outputs: y\n"
        "\n"
        "y = and(a, b)\n";
    char err[128] = {0};
    circuit_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    circuit_t *c = circuit_io_parse_ex(src, err, sizeof(err), NULL, &meta);
    check("legacy file: parse succeeds",  c != NULL);
    check("legacy file: display_mode == 0",  meta.display_mode == 0);
    check("legacy file: display_name empty", meta.display_name[0] == '\0');
    check("legacy file: pin styles all NORMAL",
          meta.input_styles[0] == 0 && meta.output_styles[0] == 0);
    if (c) circuit_destroy(c);
}

static void test_meta_parse_unknown_style_ignored(void) {
    /* Unknown style word — parser silently ignores. */
    const char *src =
        "inputs: D, CLK\n"
        "outputs: Q\n"
        "\n"
        "Q = not(D)\n"
        "\n"
        "# @pin_style = __input:CLK : tristate\n";   /* unknown style */
    char err[128] = {0};
    circuit_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    circuit_t *c = circuit_io_parse_ex(src, err, sizeof(err), NULL, &meta);
    check("unknown style: parse still succeeds", c != NULL);
    check("unknown style: input[1] stays NORMAL", meta.input_styles[1] == 0);
    if (c) circuit_destroy(c);
}

static void test_meta_parse_unknown_pin_ignored(void) {
    /* Style references a non-existent pin — silently ignored. */
    const char *src =
        "inputs: a\n"
        "outputs: y\n"
        "\n"
        "y = not(a)\n"
        "\n"
        "# @pin_style = __input:GHOST : clock\n";
    char err[128] = {0};
    circuit_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    circuit_t *c = circuit_io_parse_ex(src, err, sizeof(err), NULL, &meta);
    check("unknown pin: parse still succeeds", c != NULL);
    int any_set = 0;
    for (int i = 0; i < DOMAIN_MAX_IO; i++) {
        if (meta.input_styles[i] != 0 || meta.output_styles[i] != 0) {
            any_set = 1; break;
        }
    }
    check("unknown pin: no styles set", !any_set);
    if (c) circuit_destroy(c);
}

static void test_meta_parse_after_layout_no_blank(void) {
    /* Metadata annotations sit immediately after a # @layout block with
       NO blank line between — the parser must still recognise them as
       single-line annotations, not as layout entries. */
    const char *src =
        "inputs: a\n"
        "outputs: y\n"
        "\n"
        "y = not(a)\n"
        "\n"
        "# @layout\n"
        "# @  y = 100, 100\n"
        "# @display_mode = external\n"
        "# @display_name = NoBlank\n";
    char err[128] = {0};
    circuit_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    circuit_t *c = circuit_io_parse_ex(src, err, sizeof(err), NULL, &meta);
    check("post-layout no-blank: parse succeeds",         c != NULL);
    check("post-layout no-blank: display_mode parsed",     meta.display_mode == 1);
    check("post-layout no-blank: display_name parsed",
          strcmp(meta.display_name, "NoBlank") == 0);
    check("post-layout no-blank: layout still applied",
          c && c->components[0]->position.x == 100);
    if (c) circuit_destroy(c);
}

static void test_meta_legacy_parse_ignores_meta(void) {
    /* legacy circuit_io_parse (no _ex) should still succeed on a file
       that contains metadata, just dropping the data on the floor. */
    const char *src =
        "inputs: a\n"
        "outputs: y\n"
        "\n"
        "y = not(a)\n"
        "\n"
        "# @display_mode = external\n"
        "# @display_name = Foo\n"
        "# @pin_style = __input:a : clock\n";
    char err[128] = {0};
    circuit_t *c = circuit_io_parse(src, err, sizeof(err));
    check("legacy parse with meta: still succeeds", c != NULL);
    if (c) circuit_destroy(c);
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
    /* Supplement Phase 7 */
    test_serialize_with_wires();
    test_round_trip_with_wires();
    test_parse_wires_ascii_arrow();
    test_parse_no_wires_block();
    test_parse_ex_with_null_geom();
    test_legacy_serialize_byte_identical();
    /* Supplement Phase 10 */
    test_serialize_meta_block();
    test_meta_block_omitted_when_defaults();
    test_round_trip_meta();
    test_meta_parse_legacy_file();
    test_meta_parse_unknown_style_ignored();
    test_meta_parse_unknown_pin_ignored();
    test_meta_parse_after_layout_no_blank();
    test_meta_legacy_parse_ignores_meta();
    printf("\n%d / %d passed\n", total - failures, total);
    return failures;
}
