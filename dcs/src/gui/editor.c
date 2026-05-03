#include "editor.h"
#include "parser.h"
#include "sim.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define BTN_H 40

/* ── sidebar layout ───────────────────────────────────────────── */

typedef struct { Rectangle r; const char *label; PlaceKind kind; } SidebarBtn;

static int sidebar_buttons(SidebarBtn *out) {
    SidebarBtn b[] = {
        { {10, 50,  90, BTN_H}, "AND",     PLACE_AND    },
        { {10, 100, 90, BTN_H}, "OR",      PLACE_OR     },
        { {10, 150, 90, BTN_H}, "NOT",     PLACE_NOT    },
        { {10, 220, 90, BTN_H}, "+ INPUT", PLACE_INPUT  },
        { {10, 270, 90, BTN_H}, "+ OUTPUT",PLACE_OUTPUT },
    };
    int n = (int)(sizeof(b) / sizeof(b[0]));
    memcpy(out, b, n * sizeof(SidebarBtn));
    return n;
}

/* ── helpers ──────────────────────────────────────────────────── */

static void status(EditorState *e, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->status, sizeof(e->status), fmt, ap);
    va_end(ap);
    e->status_until = GetTime() + 3.0;
}

/* Generate next unused name with given prefix, e.g. "in1", "g7". */
static void next_name(EditorState *e, const CanvasState *cs, PlaceKind kind,
                      char *out, int out_len) {
    const char *prefix; int *counter;
    switch (kind) {
        case PLACE_INPUT:  prefix = "in";  counter = &e->counter_in;   break;
        case PLACE_OUTPUT: prefix = "out"; counter = &e->counter_out;  break;
        default:           prefix = "g";   counter = &e->counter_gate; break;
    }
    for (;;) {
        snprintf(out, out_len, "%s%d", prefix, ++(*counter));
        int collision = 0;
        for (int i = 0; i < cs->node_count; i++)
            if (strcmp(cs->nodes[i].wire_name, out) == 0) { collision = 1; break; }
        if (!collision) return;
    }
}

void editor_init(EditorState *e, const CanvasState *cs, const char *file_path) {
    memset(e, 0, sizeof(*e));
    e->mode = MODE_IDLE;
    e->place_kind = PLACE_NONE;
    e->wire_src_node = -1;
    e->drag_node = -1;
    e->hover_node = -1;
    e->hover_wire = -1;
    /* Counter heuristic: start past the existing node count of each kind.
       The collision check in next_name() handles any remaining clashes. */
    for (int i = 0; i < cs->node_count; i++) {
        if (cs->nodes[i].kind == NODE_INPUT)  e->counter_in++;
        if (cs->nodes[i].kind == NODE_OUTPUT) e->counter_out++;
        if (cs->nodes[i].kind == NODE_GATE)   e->counter_gate++;
    }
    if (file_path) {
        strncpy(e->file_path, file_path, sizeof(e->file_path) - 1);
        e->file_path[sizeof(e->file_path) - 1] = '\0';
    }
}

/* ── input handling ───────────────────────────────────────────── */

static void place_at(EditorState *e, CanvasState *cs, Vector2 world) {
    char name[SIM_NAME_LEN];
    next_name(e, cs, e->place_kind, name, sizeof(name));
    switch (e->place_kind) {
        case PLACE_AND:    canvas_add_gate  (cs, GATE_AND, name, world); break;
        case PLACE_OR:     canvas_add_gate  (cs, GATE_OR,  name, world); break;
        case PLACE_NOT:    canvas_add_gate  (cs, GATE_NOT, name, world); break;
        case PLACE_INPUT:  canvas_add_input (cs, name, world); break;
        case PLACE_OUTPUT: canvas_add_output(cs, name, world); break;
        default: return;
    }
    status(e, "Placed %s", name);
}

/* Smart-rename when a wire's destination is an OUTPUT node, so the saved
   .dcs has the output's name as the wire name. */
static void smart_rename_for_wire(CanvasState *cs, int src_idx, int dst_idx) {
    CanvasNode *src = &cs->nodes[src_idx];
    CanvasNode *dst = &cs->nodes[dst_idx];
    if (dst->kind != NODE_OUTPUT) return;
    if (src->kind == NODE_GATE) {
        /* Producer adopts output's name. */
        strncpy(src->wire_name, dst->wire_name, SIM_NAME_LEN - 1);
        src->wire_name[SIM_NAME_LEN - 1] = '\0';
    } else if (src->kind == NODE_INPUT) {
        /* Passthrough: output adopts input's name. */
        strncpy(dst->wire_name, src->wire_name, SIM_NAME_LEN - 1);
        dst->wire_name[SIM_NAME_LEN - 1] = '\0';
    }
}

void editor_update(EditorState *e, CanvasState *cs, Camera2D *cam) {
    Vector2 mouse_screen = GetMousePosition();
    Vector2 mouse_world  = GetScreenToWorld2D(mouse_screen, *cam);
    int over_sidebar = mouse_screen.x < EDITOR_SIDEBAR_W;
    int over_header  = mouse_screen.y < EDITOR_HEADER_H;

    /* ESC always cancels current mode */
    if (IsKeyPressed(KEY_ESCAPE)) {
        e->mode = MODE_IDLE;
        e->place_kind = PLACE_NONE;
        e->wire_src_node = -1;
        e->drag_node = -1;
    }

    /* Sidebar button click → arm placement */
    if (over_sidebar && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        SidebarBtn btns[8];
        int n = sidebar_buttons(btns);
        for (int i = 0; i < n; i++) {
            if (CheckCollisionPointRec(mouse_screen, btns[i].r)) {
                e->mode = MODE_PLACING;
                e->place_kind = btns[i].kind;
                status(e, "Click canvas to place %s (ESC to cancel)", btns[i].label);
                return;
            }
        }
    }

    /* Pan with middle-mouse drag */
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 d = GetMouseDelta();
        cam->target.x -= d.x / cam->zoom;
        cam->target.y -= d.y / cam->zoom;
    }

    /* Zoom toward cursor with scroll wheel (canvas only) */
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && !over_sidebar) {
        Vector2 mw = GetScreenToWorld2D(mouse_screen, *cam);
        cam->offset = mouse_screen;
        cam->target = mw;
        cam->zoom += wheel * 0.1f * cam->zoom;
        if (cam->zoom < 0.1f) cam->zoom = 0.1f;
        if (cam->zoom > 5.0f) cam->zoom = 5.0f;
    }

    /* Recompute hover (canvas only) */
    e->hover_node = -1;
    e->hover_wire = -1;
    if (!over_sidebar && !over_header) {
        e->hover_node = canvas_node_at(cs, mouse_world);
        if (e->hover_node < 0) e->hover_wire = canvas_wire_at(cs, mouse_world);
    }

    /* Mode-specific handling */
    if (e->mode == MODE_PLACING) {
        if (!over_sidebar && !over_header && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            place_at(e, cs, mouse_world);
            /* stay in placing mode for repeated placement; ESC to exit */
        }
        return;
    }

    if (e->mode == MODE_WIRING) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            e->mode = MODE_IDLE; e->wire_src_node = -1; return;
        }
        if (!over_sidebar && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int pin = 0;
            int dst = canvas_input_pin_at(cs, mouse_world, &pin);
            if (dst >= 0 && dst != e->wire_src_node) {
                /* Replace any existing wire on this destination pin */
                for (int i = 0; i < cs->wire_count; i++)
                    if (cs->wires[i].dst_node == dst && cs->wires[i].dst_pin == pin) {
                        canvas_remove_wire(cs, i); break;
                    }
                /* Adjust src index if it shifted due to removal — none happened above
                   because remove can only shift indices > the removed; src/dst are unaffected
                   if removed wire was elsewhere. We're safe. */
                if (canvas_add_wire(cs, e->wire_src_node, dst, pin) == 0) {
                    smart_rename_for_wire(cs, e->wire_src_node, dst);
                    status(e, "Wired");
                }
                e->mode = MODE_IDLE; e->wire_src_node = -1;
            }
        }
        return;
    }

    if (e->mode == MODE_DRAGGING) {
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            e->mode = MODE_IDLE; e->drag_node = -1;
        } else if (e->drag_node >= 0 && e->drag_node < cs->node_count) {
            cs->nodes[e->drag_node].pos.x = mouse_world.x - e->drag_offset.x;
            cs->nodes[e->drag_node].pos.y = mouse_world.y - e->drag_offset.y;
        }
        return;
    }

    /* IDLE — left/right click handling on canvas */
    if (!over_sidebar && !over_header) {
        /* Right-click: delete hovered node or wire */
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            if (e->hover_node >= 0) {
                canvas_remove_node(cs, e->hover_node);
                status(e, "Node deleted");
                e->hover_node = -1;
            } else if (e->hover_wire >= 0) {
                canvas_remove_wire(cs, e->hover_wire);
                status(e, "Wire deleted");
                e->hover_wire = -1;
            }
            return;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            /* Output pin → start wiring */
            int src = canvas_output_pin_at(cs, mouse_world);
            if (src >= 0) {
                e->mode = MODE_WIRING;
                e->wire_src_node = src;
                status(e, "Click an input pin to wire (ESC/right-click to cancel)");
                return;
            }
            /* Input pin → disconnect existing wire (if any) */
            int pin = 0;
            int dst = canvas_input_pin_at(cs, mouse_world, &pin);
            if (dst >= 0) {
                for (int i = 0; i < cs->wire_count; i++)
                    if (cs->wires[i].dst_node == dst && cs->wires[i].dst_pin == pin) {
                        canvas_remove_wire(cs, i);
                        status(e, "Disconnected");
                        return;
                    }
                return;
            }
            /* Node body → start dragging */
            if (e->hover_node >= 0) {
                e->mode = MODE_DRAGGING;
                e->drag_node = e->hover_node;
                e->drag_offset.x = mouse_world.x - cs->nodes[e->hover_node].pos.x;
                e->drag_offset.y = mouse_world.y - cs->nodes[e->hover_node].pos.y;
                return;
            }
        }
    }

    /* Ctrl+S → save */
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) &&
        IsKeyPressed(KEY_S)) {
        char err[256] = {0};
        if (editor_save(cs, e->file_path, err, sizeof(err)) == 0)
            status(e, "Saved to %s", e->file_path);
        else
            status(e, "Save failed: %s", err);
    }
}

/* ── drawing ──────────────────────────────────────────────────── */

void editor_draw(const EditorState *e, const CanvasState *cs, Camera2D cam,
                 int screen_w, int screen_h) {
    Vector2 mp = GetMousePosition();

    /* Sidebar background */
    DrawRectangle(0, 0, EDITOR_SIDEBAR_W, screen_h, (Color){50, 50, 60, 255});
    DrawText("DCS", 32, 14, 22, (Color){200, 200, 200, 255});

    /* Sidebar buttons */
    SidebarBtn btns[8];
    int n = sidebar_buttons(btns);
    for (int i = 0; i < n; i++) {
        int active = (e->mode == MODE_PLACING && e->place_kind == btns[i].kind);
        Color bg = active                                ? (Color){80, 130, 180, 255}
                  : CheckCollisionPointRec(mp, btns[i].r)? (Color){100, 100, 110, 255}
                                                         : (Color){80, 80, 90, 255};
        DrawRectangleRec(btns[i].r, bg);
        DrawRectangleLinesEx(btns[i].r, 1, LIGHTGRAY);
        int tw = MeasureText(btns[i].label, 14);
        DrawText(btns[i].label,
                 btns[i].r.x + (int)btns[i].r.width  / 2 - tw / 2,
                 btns[i].r.y + (int)btns[i].r.height / 2 - 7,
                 14, RAYWHITE);
    }

    /* Wiring rubber-band line */
    if (e->mode == MODE_WIRING && e->wire_src_node >= 0 && e->wire_src_node < cs->node_count) {
        Vector2 src_pin    = canvas_node_output_pin(&cs->nodes[e->wire_src_node]);
        Vector2 src_screen = GetWorldToScreen2D(src_pin, cam);
        DrawLineEx(src_screen, mp, 2.0f, (Color){80, 130, 180, 255});
    }

    /* Header */
    DrawRectangle(EDITOR_SIDEBAR_W, 0, screen_w - EDITOR_SIDEBAR_W,
                  EDITOR_HEADER_H, (Color){200, 200, 200, 255});
    DrawText(TextFormat("File: %s", e->file_path[0] ? e->file_path : "(none)"),
             EDITOR_SIDEBAR_W + 10, 8, 16, BLACK);

    /* Status bar */
    if (e->status[0] && GetTime() < e->status_until) {
        DrawRectangle(EDITOR_SIDEBAR_W, screen_h - EDITOR_STATUS_H,
                      screen_w - EDITOR_SIDEBAR_W, EDITOR_STATUS_H,
                      (Color){40, 40, 50, 255});
        DrawText(e->status, EDITOR_SIDEBAR_W + 10, screen_h - 18, 14, RAYWHITE);
    }
}

/* ── save: CanvasState → Circuit → serialize → write ──────────── */

int editor_save(const CanvasState *cs, const char *path,
                char *err_out, int err_len) {
    if (!path || !*path) {
        if (err_out) snprintf(err_out, err_len, "no file path set");
        return -1;
    }

    Circuit *c = circuit_new();
    if (!c) { if (err_out) snprintf(err_out, err_len, "out of memory"); return -1; }

    /* 1. Add inputs */
    for (int i = 0; i < cs->node_count; i++) {
        if (cs->nodes[i].kind != NODE_INPUT) continue;
        if (circuit_add_input(c, cs->nodes[i].wire_name) != 0) {
            if (err_out) snprintf(err_out, err_len,
                                  "duplicate or invalid input '%s'", cs->nodes[i].wire_name);
            circuit_free(c); return -1;
        }
    }

    /* 2. Add gates in topological order. A gate is "ready" when all its input
       wires' source nodes are already added (inputs start ready). */
    size_t added_n = (cs->node_count > 0) ? (size_t)cs->node_count : 1;
    int *added = calloc(added_n, sizeof(int));
    if (!added) { circuit_free(c); return -1; }
    for (int i = 0; i < cs->node_count; i++)
        if (cs->nodes[i].kind == NODE_INPUT) added[i] = 1;

    int progress = 1;
    while (progress) {
        progress = 0;
        for (int i = 0; i < cs->node_count; i++) {
            if (added[i] || cs->nodes[i].kind != NODE_GATE) continue;
            int ic = cs->nodes[i].input_count;
            int pin_src[2] = {-1, -1};
            for (int wi = 0; wi < cs->wire_count; wi++) {
                if (cs->wires[wi].dst_node == i && cs->wires[wi].dst_pin < ic)
                    pin_src[cs->wires[wi].dst_pin] = cs->wires[wi].src_node;
            }
            int ready = 1;
            for (int p = 0; p < ic; p++)
                if (pin_src[p] < 0 || !added[pin_src[p]]) { ready = 0; break; }
            if (!ready) continue;
            const char *in1 = cs->nodes[pin_src[0]].wire_name;
            const char *in2 = (ic >= 2) ? cs->nodes[pin_src[1]].wire_name : NULL;
            if (circuit_add_gate(c, cs->nodes[i].gate_type,
                                 cs->nodes[i].wire_name, in1, in2) != 0) {
                if (err_out) snprintf(err_out, err_len,
                                      "cannot add gate '%s' (duplicate name?)",
                                      cs->nodes[i].wire_name);
                free(added); circuit_free(c); return -1;
            }
            added[i] = 1;
            progress = 1;
        }
    }

    /* Any gate not added means an unconnected input or a cycle. */
    for (int i = 0; i < cs->node_count; i++) {
        if (cs->nodes[i].kind == NODE_GATE && !added[i]) {
            if (err_out) snprintf(err_out, err_len,
                                  "gate '%s' has unconnected inputs or is in a cycle",
                                  cs->nodes[i].wire_name);
            free(added); circuit_free(c); return -1;
        }
    }

    /* 3. Add outputs (must reference an existing wire) */
    for (int i = 0; i < cs->node_count; i++) {
        if (cs->nodes[i].kind != NODE_OUTPUT) continue;
        /* Validate: this OUTPUT's wire_name must match an input or a gate's out_wire. */
        int found = 0;
        for (int j = 0; j < cs->node_count && !found; j++) {
            if (cs->nodes[j].kind == NODE_OUTPUT) continue;
            if (strcmp(cs->nodes[j].wire_name, cs->nodes[i].wire_name) == 0) found = 1;
        }
        if (!found) {
            if (err_out) snprintf(err_out, err_len,
                                  "output '%s' is not connected to any wire",
                                  cs->nodes[i].wire_name);
            free(added); circuit_free(c); return -1;
        }
        if (circuit_add_output(c, cs->nodes[i].wire_name) != 0) {
            if (err_out) snprintf(err_out, err_len,
                                  "cannot add output '%s'", cs->nodes[i].wire_name);
            free(added); circuit_free(c); return -1;
        }
    }

    free(added);

    /* 4. Serialize and write */
    char *text = serialize_circuit(c);
    circuit_free(c);
    if (!text) { if (err_out) snprintf(err_out, err_len, "serialize failed"); return -1; }

    FILE *f = fopen(path, "wb");
    if (!f) {
        if (err_out) snprintf(err_out, err_len, "cannot open '%s' for writing", path);
        free(text); return -1;
    }
    fwrite(text, 1, strlen(text), f);
    fclose(f);
    free(text);
    return 0;
}
