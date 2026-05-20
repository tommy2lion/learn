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

/* Pull the next line. Returns 0 (line written, possibly empty after trim)
   or -1 at end of text. NOTE: leaves any leading '#' intact so the caller can
   distinguish `# @<annotation>` lines from regular comments. Inline comments
   on structural lines are stripped by the caller. */
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

/* Map a pin-style word to the persistent int code. Returns -1 if unknown
   (forward-compatible: the parser silently ignores it). */
static int pin_style_from_word(const char *w) {
    if (strcmp(w, "normal")   == 0) return 0;
    if (strcmp(w, "clock")    == 0) return 1;
    if (strcmp(w, "inverted") == 0) return 2;
    return -1;
}

static const char *pin_style_to_word(int style) {
    switch (style) {
        case 1:  return "clock";
        case 2:  return "inverted";
        default: return "normal";
    }
}

/* Parse "# @display_mode = internal" / "external". Tail is the text
   after "# @display_mode". Unknown values are silently ignored. */
static void parse_display_mode_line(const char *tail, circuit_meta_t *m) {
    const char *eq = strchr(tail, '=');
    if (!eq) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*s", (int)(sizeof(buf) - 1), eq + 1);
    str_trim(buf);
    if      (strcmp(buf, "internal") == 0) m->display_mode = 0;
    else if (strcmp(buf, "external") == 0) m->display_mode = 1;
}

/* Parse "# @display_name = <freeform>". Tail is the text after the
   "# @display_name" prefix. */
static void parse_display_name_line(const char *tail, circuit_meta_t *m) {
    const char *eq = strchr(tail, '=');
    if (!eq) return;
    char buf[DOMAIN_NAME_LEN];
    snprintf(buf, sizeof(buf), "%.*s", (int)(sizeof(buf) - 1), eq + 1);
    str_trim(buf);
    snprintf(m->display_name, sizeof(m->display_name), "%s", buf);
}

/* Parse "# @pin_style = __input:NAME : style" or "__output:NAME : style".
   Tail is the text after "# @pin_style". Looks up the named pin in the
   circuit and writes the corresponding style code into meta. Unknown
   pin names or style words are silently ignored. */
static void parse_pin_style_line(const circuit_t *c, const char *tail,
                                 circuit_meta_t *m) {
    const char *eq = strchr(tail, '=');
    if (!eq) return;
    char rhs[160];
    snprintf(rhs, sizeof(rhs), "%.*s", (int)(sizeof(rhs) - 1), eq + 1);
    str_trim(rhs);

    /* The pin-reference uses "__input:" or "__output:" — the colon there
       is part of the pin reference, so we look for the LAST colon, which
       separates the pin reference from the style word. */
    char *style_colon = strrchr(rhs, ':');
    if (!style_colon) return;
    *style_colon = '\0';
    char *pin_ref    = rhs;
    char *style_word = style_colon + 1;
    str_trim(pin_ref);
    str_trim(style_word);

    int style = pin_style_from_word(style_word);
    if (style < 0) return;

    if (strncmp(pin_ref, "__input:", 8) == 0) {
        const char *name = pin_ref + 8;
        for (int i = 0; i < c->input_count && i < DOMAIN_MAX_IO; i++) {
            if (strcmp(c->input_names[i], name) == 0) {
                m->input_styles[i] = style;
                return;
            }
        }
    } else if (strncmp(pin_ref, "__output:", 9) == 0) {
        const char *name = pin_ref + 9;
        for (int i = 0; i < c->output_count && i < DOMAIN_MAX_IO; i++) {
            if (strcmp(c->output_names[i], name) == 0) {
                m->output_styles[i] = style;
                return;
            }
        }
    }
    /* Unknown pin: silently ignored (forward-compatible). */
}

/* Parse one wires-block entry. Two forms are accepted:
     "net=<wire_name>"             — switch to a new (or existing) net.
     "h x1,y1 → x2,y2"            — append H segment to the current net.
     "v x1,y1 → x2,y2"            — append V segment to the current net.
   The arrow may also be written "->" (both are accepted; serializer emits →).
   `body` is the text after "# @" with leading whitespace; both "  net=..."
   and "    h ..." are valid. Unknown sub-formats are silently ignored
   (forward-compatible). */
static void parse_wires_entry(wire_geometry_t *geom, const char *body,
                              int *current_net_idx) {
    char buf[MAX_LINE];
    snprintf(buf, sizeof(buf), "%s", body);
    str_trim(buf);
    if (!buf[0]) return;

    /* "net=<name>" — start a new net (or re-select an existing one). */
    if (strncmp(buf, "net=", 4) == 0) {
        char name[DOMAIN_NAME_LEN];
        /* Bound the source explicitly so gcc doesn't flag a possible truncation
           (snprintf already truncates safely — but %.*s makes the bound visible). */
        snprintf(name, sizeof(name), "%.*s",
                 (int)(sizeof(name) - 1), buf + 4);
        str_trim(name);
        if (!name[0]) { *current_net_idx = -1; return; }
        *current_net_idx = wire_geometry_get_or_create(geom, name);
        return;
    }

    /* "h x1,y1 → x2,y2" / "v ..." — append one segment to the current net. */
    if ((buf[0] == 'h' || buf[0] == 'v')
        && (buf[1] == ' ' || buf[1] == '\t')) {
        if (*current_net_idx < 0) return;
        char dir = buf[0];
        float x1, y1, x2, y2;
        char arrow[8] = {0};
        const char *p = buf + 1;
        while (isspace((unsigned char)*p)) p++;
        int n = sscanf(p, "%f , %f %4s %f , %f",
                       &x1, &y1, arrow, &x2, &y2);
        if (n != 5) return;
        if (strcmp(arrow, "\xe2\x86\x92") != 0   /* UTF-8 → */
         && strcmp(arrow, "->") != 0) return;
        if (dir == 'h' && y1 != y2) return;
        if (dir == 'v' && x1 != x2) return;
        wire_segment_t seg;
        seg.a.x = x1; seg.a.y = y1;
        seg.b.x = x2; seg.b.y = y2;
        wire_geometry_append_segments(geom, *current_net_idx, &seg, 1);
        return;
    }
    /* unknown form: ignored */
}

/* Parse one layout-block entry: "  name = x, y" (already stripped of "# @").
   Looks the name up in components / inputs / outputs and writes the position.
   Unknown names are silently ignored (forward-compatible). */
static void parse_layout_entry(circuit_t *c, const char *body) {
    char buf[MAX_LINE];
    snprintf(buf, sizeof(buf), "%s", body);
    str_trim(buf);
    char *eq = strchr(buf, '=');
    if (!eq) return;
    *eq = '\0';
    char *name = buf;     str_trim(name);
    char *vals = eq + 1;  str_trim(vals);
    if (!*name || !*vals) return;

    char *comma = strchr(vals, ',');
    if (!comma) return;
    *comma = '\0';
    char *xs = vals;       str_trim(xs);
    char *ys = comma + 1;  str_trim(ys);
    float x = (float)atof(xs);
    float y = (float)atof(ys);

    if (strncmp(name, "__input:", 8) == 0) {
        const char *iname = name + 8;
        for (int i = 0; i < c->input_count; i++)
            if (strcmp(c->input_names[i], iname) == 0) {
                c->input_positions[i] = (vec2_t){x, y};
                return;
            }
    } else if (strncmp(name, "__output:", 9) == 0) {
        const char *oname = name + 9;
        for (int i = 0; i < c->output_count; i++)
            if (strcmp(c->output_names[i], oname) == 0) {
                c->output_positions[i] = (vec2_t){x, y};
                return;
            }
    } else {
        for (int i = 0; i < c->component_count; i++)
            if (strcmp(c->components[i]->name, name) == 0) {
                c->components[i]->position = (vec2_t){x, y};
                return;
            }
    }
    /* unknown name: ignore */
}

/* ── public: parse ───────────────────────────────────────────────── */

circuit_t *circuit_io_parse(const char *text, char *err_out, int err_len) {
    return circuit_io_parse_ex(text, err_out, err_len, NULL, NULL);
}

circuit_t *circuit_io_parse_ex(const char *text, char *err_out, int err_len,
                               wire_geometry_t *geom_out,
                               circuit_meta_t  *meta_out) {
    if (err_out && err_len > 0) err_out[0] = '\0';

    circuit_t *c = circuit_create();
    if (!c) return fail(NULL, err_out, err_len, 0, "out of memory");

    int pos = 0, lineno = 0;
    int saw_inputs = 0, saw_outputs = 0;
    int in_layout = 0;                       /* set after `# @layout` until blank/structural line */
    int in_wires  = 0;                       /* set after `# @wires`  until blank/structural line */
    int wires_current_net = -1;               /* index in geom_out of the active net */
    char line[MAX_LINE];

    while (next_line(text, &pos, line, MAX_LINE) == 0) {
        lineno++;
        if (!line[0]) { in_layout = 0; in_wires = 0; continue; }

        /* ── annotation / comment lines ──────────────────────────── */
        if (line[0] == '#') {
            /* "# @layout" enters layout mode (subsequent "# @  ..." lines are entries) */
            if (strncmp(line, "# @layout", 9) == 0
                && (line[9] == '\0' || isspace((unsigned char)line[9]))) {
                in_layout = 1;
                in_wires  = 0;
                continue;
            }
            /* "# @wires" enters wires mode (Phase 7). Subsequent "# @ ..." lines
               are net headers ("net=<name>") or segment entries ("h ..." / "v ..."). */
            if (strncmp(line, "# @wires", 8) == 0
                && (line[8] == '\0' || isspace((unsigned char)line[8]))) {
                in_layout = 0;
                in_wires  = 1;
                wires_current_net = -1;
                continue;
            }
            /* ── Phase-10 single-line annotations. They unconditionally
               terminate any prior @-block (so they may sit immediately
               after a # @layout or # @wires block with or without a
               blank line in between). Tail is `line + N` where N is the
               length of "# @<keyword>". */
            if (strncmp(line, "# @display_mode", 15) == 0
                && (line[15] == '\0' || line[15] == ' '
                    || line[15] == '\t' || line[15] == '=')) {
                if (meta_out) parse_display_mode_line(line + 15, meta_out);
                in_layout = 0; in_wires = 0;
                continue;
            }
            if (strncmp(line, "# @display_name", 15) == 0
                && (line[15] == '\0' || line[15] == ' '
                    || line[15] == '\t' || line[15] == '=')) {
                if (meta_out) parse_display_name_line(line + 15, meta_out);
                in_layout = 0; in_wires = 0;
                continue;
            }
            if (strncmp(line, "# @pin_style", 12) == 0
                && (line[12] == '\0' || line[12] == ' '
                    || line[12] == '\t' || line[12] == '=')) {
                if (meta_out) parse_pin_style_line(c, line + 12, meta_out);
                in_layout = 0; in_wires = 0;
                continue;
            }
            /* "# @  name = x, y" while in layout mode → position entry */
            if (in_layout && strncmp(line, "# @", 3) == 0) {
                parse_layout_entry(c, line + 3);
                continue;
            }
            /* wires-block sub-line; ignored unless caller wants geometry */
            if (in_wires && geom_out && strncmp(line, "# @", 3) == 0) {
                parse_wires_entry(geom_out, line + 3, &wires_current_net);
                continue;
            }
            /* unknown annotation or plain comment: ignore (forward-compatible) */
            continue;
        }

        /* ── structural line: strip any inline comment, then parse ── */
        in_layout = 0;
        in_wires  = 0;
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';
        str_trim(line);
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

/* True if any component / input / output position is non-zero. */
static int has_layout_data(const circuit_t *c) {
    for (int i = 0; i < c->component_count; i++) {
        vec2_t p = c->components[i]->position;
        if (p.x != 0 || p.y != 0) return 1;
    }
    for (int i = 0; i < c->input_count; i++) {
        vec2_t p = c->input_positions[i];
        if (p.x != 0 || p.y != 0) return 1;
    }
    for (int i = 0; i < c->output_count; i++) {
        vec2_t p = c->output_positions[i];
        if (p.x != 0 || p.y != 0) return 1;
    }
    return 0;
}

char *circuit_io_serialize(const circuit_t *c) {
    return circuit_io_serialize_ex(c, NULL, NULL);
}

/* True if any net in geom has at least one segment. */
static int has_any_wires(const wire_geometry_t *geom) {
    if (!geom) return 0;
    for (int i = 0; i < geom->net_count; i++) {
        if (geom->nets[i].seg_count > 0) return 1;
    }
    return 0;
}

/* True if meta has anything worth emitting. The serializer omits the
   block entirely when every field is at its default. */
static int has_any_meta(const circuit_t *c, const circuit_meta_t *meta) {
    if (!meta) return 0;
    if (meta->display_mode != 0)   return 1;
    if (meta->display_name[0])      return 1;
    for (int i = 0; i < c->input_count && i < DOMAIN_MAX_IO; i++)
        if (meta->input_styles[i] != 0) return 1;
    for (int i = 0; i < c->output_count && i < DOMAIN_MAX_IO; i++)
        if (meta->output_styles[i] != 0) return 1;
    return 0;
}

char *circuit_io_serialize_ex(const circuit_t *c,
                              const wire_geometry_t *geom,
                              const circuit_meta_t  *meta) {
    /* Estimate extra capacity for the optional # @wires block. */
    int wires_extra = 0;
    if (has_any_wires(geom)) {
        wires_extra += 32;     /* "\n# @wires\n" header */
        for (int i = 0; i < geom->net_count; i++) {
            wires_extra += 96;
            wires_extra += geom->nets[i].seg_count * 72;
        }
    }
    /* Loose upper bound for the metadata annotations. */
    int meta_extra = has_any_meta(c, meta)
                   ? 64 + DOMAIN_NAME_LEN + 96 * (c->input_count + c->output_count)
                   : 0;
    int cap = 4096 + c->component_count * 256
            + (c->input_count + c->output_count) * 96
            + wires_extra + meta_extra;
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

    /* Optional layout-annotation block (Phase 2.6). Skipped when every
       position is zero — preserves the prototype's compact format for
       freshly-built circuits and keeps round-trips stable. */
    if (has_layout_data(c)) {
        pos += snprintf(buf + pos, cap - pos, "\n# @layout\n");
        for (int i = 0; i < c->component_count; i++) {
            vec2_t p = c->components[i]->position;
            pos += snprintf(buf + pos, cap - pos, "# @  %s = %g, %g\n",
                            c->components[i]->name, p.x, p.y);
        }
        for (int i = 0; i < c->input_count; i++) {
            vec2_t p = c->input_positions[i];
            pos += snprintf(buf + pos, cap - pos, "# @  __input:%s = %g, %g\n",
                            c->input_names[i], p.x, p.y);
        }
        for (int i = 0; i < c->output_count; i++) {
            vec2_t p = c->output_positions[i];
            pos += snprintf(buf + pos, cap - pos, "# @  __output:%s = %g, %g\n",
                            c->output_names[i], p.x, p.y);
        }
    }

    /* Optional wires-annotation block (supplement Phase 7). Skipped when geom
       is NULL or every net is empty — keeps the legacy single-arg serializer
       output byte-identical to what it produced before this phase landed. */
    if (has_any_wires(geom)) {
        pos += snprintf(buf + pos, cap - pos, "\n# @wires\n");
        for (int i = 0; i < geom->net_count; i++) {
            const wire_net_geom_t *n = &geom->nets[i];
            if (n->seg_count == 0) continue;
            pos += snprintf(buf + pos, cap - pos, "# @  net=%s\n", n->wire_name);
            for (int s = 0; s < n->seg_count; s++) {
                const wire_segment_t *seg = &n->segs[s];
                char dir = (seg->a.y == seg->b.y) ? 'h' : 'v';
                pos += snprintf(buf + pos, cap - pos,
                                "# @    %c %g,%g \xe2\x86\x92 %g,%g\n",
                                dir,
                                seg->a.x, seg->a.y,
                                seg->b.x, seg->b.y);
            }
        }
    }

    /* Optional metadata block (supplement Phase 10). Emits only the fields
       that differ from their defaults so legacy files stay clean. */
    if (has_any_meta(c, meta)) {
        pos += snprintf(buf + pos, cap - pos, "\n");
        if (meta->display_mode != 0) {
            pos += snprintf(buf + pos, cap - pos,
                            "# @display_mode = external\n");
        }
        if (meta->display_name[0]) {
            pos += snprintf(buf + pos, cap - pos,
                            "# @display_name = %s\n", meta->display_name);
        }
        for (int i = 0; i < c->input_count && i < DOMAIN_MAX_IO; i++) {
            if (meta->input_styles[i] == 0) continue;
            pos += snprintf(buf + pos, cap - pos,
                            "# @pin_style = __input:%s : %s\n",
                            c->input_names[i],
                            pin_style_to_word(meta->input_styles[i]));
        }
        for (int i = 0; i < c->output_count && i < DOMAIN_MAX_IO; i++) {
            if (meta->output_styles[i] == 0) continue;
            pos += snprintf(buf + pos, cap - pos,
                            "# @pin_style = __output:%s : %s\n",
                            c->output_names[i],
                            pin_style_to_word(meta->output_styles[i]));
        }
    }
    return buf;
}
