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

/* ── inputs-panel layout (under the sidebar) ──────────────────── */

#define IPANEL_TOGGLE_H  24
#define IPANEL_TOGGLE_GAP 4
#define IPANEL_MAX_VISIBLE_TOGGLES 5

static int panel_top(int screen_h)    { return screen_h - EDITOR_STATUS_H - EDITOR_PANEL_H; }
static int panel_bottom(int screen_h) { return screen_h - EDITOR_STATUS_H; }

static Rectangle ipanel_steps_minus(int screen_h) {
    int by = panel_bottom(screen_h);
    return (Rectangle){ 8, by - 78, 22, 22 };
}
static Rectangle ipanel_steps_plus(int screen_h) {
    int by = panel_bottom(screen_h);
    return (Rectangle){ EDITOR_SIDEBAR_W - 30, by - 78, 22, 22 };
}
static Rectangle ipanel_run_btn(int screen_h) {
    int by = panel_bottom(screen_h);
    return (Rectangle){ 8, by - 44, EDITOR_SIDEBAR_W - 16, 36 };
}
static Rectangle ipanel_toggle(int idx, int screen_h) {
    int py = panel_top(screen_h);
    return (Rectangle){ 8, py + 22 + idx * (IPANEL_TOGGLE_H + IPANEL_TOGGLE_GAP),
                        EDITOR_SIDEBAR_W - 16, IPANEL_TOGGLE_H };
}

/* ── canvas region (for main.c scissor) ───────────────────────── */

Rectangle editor_canvas_rect(int screen_w, int screen_h) {
    return (Rectangle){
        EDITOR_SIDEBAR_W,
        EDITOR_HEADER_H,
        screen_w  - EDITOR_SIDEBAR_W,
        screen_h  - EDITOR_HEADER_H - EDITOR_STATUS_H - EDITOR_PANEL_H
    };
}

void editor_fit_camera(Camera2D *cam, const CanvasState *cs,
                       int screen_w, int screen_h) {
    Rectangle r = editor_canvas_rect(screen_w, screen_h);
    Vector2 canvas_center = { r.x + r.width * 0.5f, r.y + r.height * 0.5f };
    cam->offset = canvas_center;
    cam->zoom = 1.0f;
    cam->rotation = 0.0f;

    if (cs->node_count == 0) {
        cam->target = (Vector2){ 0, 0 };
        return;
    }

    float min_x = cs->nodes[0].pos.x, max_x = min_x;
    float min_y = cs->nodes[0].pos.y, max_y = min_y;
    for (int i = 1; i < cs->node_count; i++) {
        if (cs->nodes[i].pos.x < min_x) min_x = cs->nodes[i].pos.x;
        if (cs->nodes[i].pos.x > max_x) max_x = cs->nodes[i].pos.x;
        if (cs->nodes[i].pos.y < min_y) min_y = cs->nodes[i].pos.y;
        if (cs->nodes[i].pos.y > max_y) max_y = cs->nodes[i].pos.y;
    }
    cam->target = (Vector2){ (min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f };
}

/* ── status / helpers ─────────────────────────────────────────── */

static void status(EditorState *e, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->status, sizeof(e->status), fmt, ap);
    va_end(ap);
    e->status_until = GetTime() + 3.0;
}

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

/* ── input-toggle helpers ─────────────────────────────────────── */

static Signal toggle_get(const EditorState *e, const char *name) {
    for (int i = 0; i < e->toggle_count; i++)
        if (strcmp(e->toggles[i].name, name) == 0) return e->toggles[i].value;
    return SIG_LOW;
}

static void toggle_flip(EditorState *e, const char *name) {
    for (int i = 0; i < e->toggle_count; i++) {
        if (strcmp(e->toggles[i].name, name) == 0) {
            e->toggles[i].value = (e->toggles[i].value == SIG_HIGH) ? SIG_LOW : SIG_HIGH;
            return;
        }
    }
    if (e->toggle_count < EDITOR_MAX_TOGGLES) {
        InputToggle *t = &e->toggles[e->toggle_count++];
        snprintf(t->name, SIM_NAME_LEN, "%s", name);
        t->value = SIG_HIGH; /* first click flips from default LOW to HIGH */
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
    e->steps = 8;
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

/* ── canvas → circuit (shared by save and run) ────────────────── */

static Circuit *canvas_to_circuit(const CanvasState *cs, char *err_out, int err_len) {
    Circuit *c = circuit_new();
    if (!c) {
        if (err_out) snprintf(err_out, err_len, "out of memory");
        return NULL;
    }

    /* 1. Inputs */
    for (int i = 0; i < cs->node_count; i++) {
        if (cs->nodes[i].kind != NODE_INPUT) continue;
        if (circuit_add_input(c, cs->nodes[i].wire_name) != 0) {
            if (err_out) snprintf(err_out, err_len,
                                  "duplicate input '%s'", cs->nodes[i].wire_name);
            circuit_free(c); return NULL;
        }
    }

    /* 2. Gates in topological order */
    size_t added_n = (cs->node_count > 0) ? (size_t)cs->node_count : 1;
    int *added = calloc(added_n, sizeof(int));
    if (!added) { circuit_free(c); return NULL; }
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
                free(added); circuit_free(c); return NULL;
            }
            added[i] = 1;
            progress = 1;
        }
    }
    for (int i = 0; i < cs->node_count; i++) {
        if (cs->nodes[i].kind == NODE_GATE && !added[i]) {
            if (err_out) snprintf(err_out, err_len,
                                  "gate '%s' has unconnected inputs or is in a cycle",
                                  cs->nodes[i].wire_name);
            free(added); circuit_free(c); return NULL;
        }
    }

    /* 3. Outputs (must reference an existing wire) */
    for (int i = 0; i < cs->node_count; i++) {
        if (cs->nodes[i].kind != NODE_OUTPUT) continue;
        int found = 0;
        for (int j = 0; j < cs->node_count && !found; j++) {
            if (cs->nodes[j].kind == NODE_OUTPUT) continue;
            if (strcmp(cs->nodes[j].wire_name, cs->nodes[i].wire_name) == 0) found = 1;
        }
        if (!found) {
            if (err_out) snprintf(err_out, err_len,
                                  "output '%s' is not connected to any wire",
                                  cs->nodes[i].wire_name);
            free(added); circuit_free(c); return NULL;
        }
        if (circuit_add_output(c, cs->nodes[i].wire_name) != 0) {
            if (err_out) snprintf(err_out, err_len,
                                  "cannot add output '%s'", cs->nodes[i].wire_name);
            free(added); circuit_free(c); return NULL;
        }
    }

    free(added);
    return c;
}

int editor_save(const CanvasState *cs, const char *path, char *err_out, int err_len) {
    if (!path || !*path) {
        if (err_out) snprintf(err_out, err_len, "no file path set");
        return -1;
    }
    Circuit *c = canvas_to_circuit(cs, err_out, err_len);
    if (!c) return -1;
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

/* ── RUN: simulate N steps and fill tracks ───────────────────── */

static void run_simulation(EditorState *e, const CanvasState *cs) {
    char err[256] = {0};
    Circuit *c = canvas_to_circuit(cs, err, sizeof(err));
    if (!c) { e->track_count = 0; status(e, "Run failed: %s", err); return; }

    int n = e->steps;
    if (n < 1) n = 1;
    if (n > EDITOR_MAX_STEPS) n = EDITOR_MAX_STEPS;

    int n_in  = c->input_count;
    int n_out = c->output_count;
    int total = n_in + n_out;
    if (total > EDITOR_MAX_TRACKS) total = EDITOR_MAX_TRACKS;

    /* set up track names + storage pointers */
    for (int i = 0; i < total; i++) {
        const char *name = (i < n_in) ? c->input_names[i] : c->output_names[i - n_in];
        strncpy(e->tracks[i].signal_name, name, SIM_NAME_LEN - 1);
        e->tracks[i].signal_name[SIM_NAME_LEN - 1] = '\0';
        e->tracks[i].values     = e->track_values[i];
        e->tracks[i].step_count = n;
    }
    e->track_count = total;

    /* simulate */
    for (int step = 0; step < n; step++) {
        for (int i = 0; i < n_in; i++) {
            Signal v = toggle_get(e, c->input_names[i]);
            circuit_set_input(c, c->input_names[i], v);
        }
        circuit_run(c);
        for (int i = 0; i < n_in && i < total; i++)
            e->track_values[i][step] = circuit_get_wire(c, c->input_names[i]);
        for (int i = 0; i < n_out && (n_in + i) < total; i++)
            e->track_values[n_in + i][step] = circuit_get_output(c, c->output_names[i]);
    }

    e->has_run = 1;
    circuit_free(c);
    status(e, "Ran %d step%s", n, n == 1 ? "" : "s");
}

/* ── update ───────────────────────────────────────────────────── */

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

static void smart_rename_for_wire(CanvasState *cs, int src_idx, int dst_idx) {
    CanvasNode *src = &cs->nodes[src_idx];
    CanvasNode *dst = &cs->nodes[dst_idx];
    if (dst->kind != NODE_OUTPUT) return;
    if (src->kind == NODE_GATE) {
        strncpy(src->wire_name, dst->wire_name, SIM_NAME_LEN - 1);
        src->wire_name[SIM_NAME_LEN - 1] = '\0';
    } else if (src->kind == NODE_INPUT) {
        strncpy(dst->wire_name, src->wire_name, SIM_NAME_LEN - 1);
        dst->wire_name[SIM_NAME_LEN - 1] = '\0';
    }
}

/* Returns 1 if the click was consumed by the input panel. */
static int handle_input_panel_click(EditorState *e, CanvasState *cs, Vector2 mouse) {
    int sh = GetScreenHeight();

    /* RUN button */
    if (CheckCollisionPointRec(mouse, ipanel_run_btn(sh))) {
        run_simulation(e, cs);
        return 1;
    }
    /* Steps - / + */
    if (CheckCollisionPointRec(mouse, ipanel_steps_minus(sh))) {
        if (e->steps > 1) e->steps--;
        return 1;
    }
    if (CheckCollisionPointRec(mouse, ipanel_steps_plus(sh))) {
        if (e->steps < EDITOR_MAX_STEPS) e->steps++;
        return 1;
    }
    /* Per-input toggle */
    int idx = 0;
    for (int i = 0; i < cs->node_count && idx < IPANEL_MAX_VISIBLE_TOGGLES; i++) {
        if (cs->nodes[i].kind != NODE_INPUT) continue;
        if (CheckCollisionPointRec(mouse, ipanel_toggle(idx, sh))) {
            toggle_flip(e, cs->nodes[i].wire_name);
            return 1;
        }
        idx++;
    }
    return 0;
}

void editor_update(EditorState *e, CanvasState *cs, Camera2D *cam) {
    Vector2 mouse_screen = GetMousePosition();
    Vector2 mouse_world  = GetScreenToWorld2D(mouse_screen, *cam);
    int sh = GetScreenHeight();

    int over_left  = mouse_screen.x < EDITOR_SIDEBAR_W;
    int over_top   = mouse_screen.y < EDITOR_HEADER_H;
    int over_panel = mouse_screen.y >= panel_top(sh) && mouse_screen.y < panel_bottom(sh);
    int over_status= mouse_screen.y >= panel_bottom(sh);
    int over_canvas = !over_left && !over_top && !over_panel && !over_status;

    int over_sidebar      = over_left && !over_top && !over_panel && !over_status;
    int over_inputs_panel = over_left && over_panel;

    /* ESC always cancels current mode and clears selection */
    if (IsKeyPressed(KEY_ESCAPE)) {
        e->mode = MODE_IDLE;
        e->place_kind = PLACE_NONE;
        e->wire_src_node = -1;
        e->drag_node = -1;
        canvas_clear_selection(cs);
    }

    /* Ctrl+A → select all */
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) &&
        IsKeyPressed(KEY_A)) {
        canvas_select_all(cs);
        int n = canvas_selection_count(cs);
        status(e, "Selected %d node%s", n, n == 1 ? "" : "s");
    }

    /* Delete → remove all selected */
    if (IsKeyPressed(KEY_DELETE) && canvas_selection_count(cs) > 0) {
        int n = canvas_selection_count(cs);
        canvas_remove_selected(cs);
        status(e, "Deleted %d node%s", n, n == 1 ? "" : "s");
        if (e->mode == MODE_DRAGGING) { e->mode = MODE_IDLE; e->drag_node = -1; }
    }

    /* Sidebar buttons — clicking the active button toggles the mode off */
    if (over_sidebar && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        SidebarBtn btns[8];
        int n = sidebar_buttons(btns);
        for (int i = 0; i < n; i++) {
            if (!CheckCollisionPointRec(mouse_screen, btns[i].r)) continue;
            if (e->mode == MODE_PLACING && e->place_kind == btns[i].kind) {
                e->mode = MODE_IDLE;
                e->place_kind = PLACE_NONE;
                status(e, "Cancelled");
            } else {
                e->mode = MODE_PLACING;
                e->place_kind = btns[i].kind;
                status(e, "Click canvas to place %s (right-click to cancel)", btns[i].label);
            }
            return;
        }
    }

    /* Inputs panel buttons */
    if (over_inputs_panel && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (handle_input_panel_click(e, cs, mouse_screen)) return;
    }

    /* Pan with middle-mouse drag */
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 d = GetMouseDelta();
        cam->target.x -= d.x / cam->zoom;
        cam->target.y -= d.y / cam->zoom;
    }

    /* Zoom toward cursor with scroll wheel (canvas only) */
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && over_canvas) {
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
    if (over_canvas) {
        e->hover_node = canvas_node_at(cs, mouse_world);
        if (e->hover_node < 0) e->hover_wire = canvas_wire_at(cs, mouse_world);
    }

    /* Mode-specific handling */
    if (e->mode == MODE_PLACING) {
        /* Right-click anywhere cancels placement */
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            e->mode = MODE_IDLE;
            e->place_kind = PLACE_NONE;
            status(e, "Cancelled");
            return;
        }
        /* Left-click on canvas: place ONE node, then return to IDLE.
           To place several, click the sidebar button again. */
        if (over_canvas && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            place_at(e, cs, mouse_world);
            e->mode = MODE_IDLE;
            e->place_kind = PLACE_NONE;
        }
        return;
    }

    if (e->mode == MODE_WIRING) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            e->mode = MODE_IDLE; e->wire_src_node = -1; return;
        }
        if (over_canvas && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int pin = 0;
            int dst = canvas_input_pin_at(cs, mouse_world, &pin);
            if (dst >= 0 && dst != e->wire_src_node) {
                for (int i = 0; i < cs->wire_count; i++)
                    if (cs->wires[i].dst_node == dst && cs->wires[i].dst_pin == pin) {
                        canvas_remove_wire(cs, i); break;
                    }
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
            /* Compute the new position the dragged node should have so its
               grab point stays under the cursor. */
            Vector2 new_pos = {mouse_world.x - e->drag_offset.x,
                               mouse_world.y - e->drag_offset.y};
            Vector2 d = {new_pos.x - cs->nodes[e->drag_node].pos.x,
                         new_pos.y - cs->nodes[e->drag_node].pos.y};
            if (cs->nodes[e->drag_node].selected) {
                /* Move the whole selection by the same delta. */
                for (int i = 0; i < cs->node_count; i++) {
                    if (cs->nodes[i].selected) {
                        cs->nodes[i].pos.x += d.x;
                        cs->nodes[i].pos.y += d.y;
                    }
                }
            } else {
                cs->nodes[e->drag_node].pos = new_pos;
            }
        }
        return;
    }

    if (e->mode == MODE_MARQUEE) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            e->mode = MODE_IDLE; /* cancel marquee */
            return;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            float x0 = (e->marquee_start.x < mouse_world.x) ? e->marquee_start.x : mouse_world.x;
            float y0 = (e->marquee_start.y < mouse_world.y) ? e->marquee_start.y : mouse_world.y;
            float x1 = (e->marquee_start.x > mouse_world.x) ? e->marquee_start.x : mouse_world.x;
            float y1 = (e->marquee_start.y > mouse_world.y) ? e->marquee_start.y : mouse_world.y;
            Rectangle r = { x0, y0, x1 - x0, y1 - y0 };
            int additive = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            canvas_select_in_rect(cs, r, additive);
            int n = canvas_selection_count(cs);
            if (n > 0) status(e, "Selected %d node%s", n, n == 1 ? "" : "s");
            e->mode = MODE_IDLE;
        }
        return;
    }

    /* IDLE — left/right click on canvas */
    if (over_canvas) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            if (e->hover_node >= 0) {
                /* If the hovered node is part of the selection, delete the
                   whole selection; otherwise just delete this one node. */
                if (cs->nodes[e->hover_node].selected) {
                    int n = canvas_selection_count(cs);
                    canvas_remove_selected(cs);
                    status(e, "Deleted %d node%s", n, n == 1 ? "" : "s");
                } else {
                    canvas_remove_node(cs, e->hover_node);
                    status(e, "Node deleted");
                }
                e->hover_node = -1;
            } else if (e->hover_wire >= 0) {
                canvas_remove_wire(cs, e->hover_wire);
                status(e, "Wire deleted");
                e->hover_wire = -1;
            }
            return;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int src = canvas_output_pin_at(cs, mouse_world);
            if (src >= 0) {
                e->mode = MODE_WIRING;
                e->wire_src_node = src;
                status(e, "Click an input pin to wire (ESC/right-click to cancel)");
                return;
            }
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
            if (e->hover_node >= 0) {
                /* Click on a node: if it's NOT part of the current selection,
                   replace the selection with just this node. Then start drag. */
                if (!cs->nodes[e->hover_node].selected) {
                    canvas_clear_selection(cs);
                    cs->nodes[e->hover_node].selected = 1;
                }
                e->mode = MODE_DRAGGING;
                e->drag_node = e->hover_node;
                e->drag_offset.x = mouse_world.x - cs->nodes[e->hover_node].pos.x;
                e->drag_offset.y = mouse_world.y - cs->nodes[e->hover_node].pos.y;
                return;
            }
            /* Click on empty canvas → start marquee selection.
               Without Shift, the existing selection is cleared first. */
            if (!IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT))
                canvas_clear_selection(cs);
            e->mode = MODE_MARQUEE;
            e->marquee_start = mouse_world;
            return;
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

    /* R → also runs (handy keyboard shortcut) */
    if (IsKeyPressed(KEY_R) &&
        !IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_RIGHT_CONTROL)) {
        run_simulation(e, cs);
    }

    /* F → fit the view to the current circuit */
    if (IsKeyPressed(KEY_F) &&
        !IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_RIGHT_CONTROL)) {
        editor_fit_camera(cam, cs, GetScreenWidth(), GetScreenHeight());
        status(e, "View fit");
    }
}

/* ── drawing ──────────────────────────────────────────────────── */

void editor_draw_world_overlay(const EditorState *e, Camera2D cam) {
    if (e->mode != MODE_MARQUEE) return;
    Vector2 m_now = GetScreenToWorld2D(GetMousePosition(), cam);
    float x0 = (e->marquee_start.x < m_now.x) ? e->marquee_start.x : m_now.x;
    float y0 = (e->marquee_start.y < m_now.y) ? e->marquee_start.y : m_now.y;
    float x1 = (e->marquee_start.x > m_now.x) ? e->marquee_start.x : m_now.x;
    float y1 = (e->marquee_start.y > m_now.y) ? e->marquee_start.y : m_now.y;
    Rectangle r = { x0, y0, x1 - x0, y1 - y0 };
    DrawRectangleRec(r,        (Color){80, 130, 180,  50});
    DrawRectangleLinesEx(r, 1.5f, (Color){80, 130, 180, 220});
}

static void draw_inputs_panel(const EditorState *e, const CanvasState *cs, int sw, int sh) {
    (void)sw;
    int py = panel_top(sh);

    /* panel background */
    DrawRectangle(0, py, EDITOR_SIDEBAR_W, EDITOR_PANEL_H, (Color){36, 36, 44, 255});
    DrawText("INPUTS", 8, py + 4, 13, (Color){200, 200, 200, 255});

    Vector2 mp = GetMousePosition();

    /* Input toggles */
    int idx = 0, total_inputs = 0;
    for (int i = 0; i < cs->node_count; i++)
        if (cs->nodes[i].kind == NODE_INPUT) total_inputs++;

    for (int i = 0; i < cs->node_count && idx < IPANEL_MAX_VISIBLE_TOGGLES; i++) {
        if (cs->nodes[i].kind != NODE_INPUT) continue;
        Rectangle r = ipanel_toggle(idx, sh);
        Signal v = toggle_get(e, cs->nodes[i].wire_name);
        Color bg = (v == SIG_HIGH) ? (Color){70, 150, 90, 255}
                                   : (Color){150, 70, 70, 255};
        if (CheckCollisionPointRec(mp, r))
            bg = (Color){bg.r + 20, bg.g + 20, bg.b + 20, 255};
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, BLACK);
        char label[80];
        snprintf(label, sizeof(label), "%s = %d",
                 cs->nodes[i].wire_name, v == SIG_HIGH ? 1 : 0);
        DrawText(label, (int)r.x + 6, (int)r.y + 5, 13, RAYWHITE);
        idx++;
    }
    if (total_inputs > IPANEL_MAX_VISIBLE_TOGGLES) {
        Rectangle r = ipanel_toggle(IPANEL_MAX_VISIBLE_TOGGLES, sh);
        DrawText(TextFormat("+%d more", total_inputs - IPANEL_MAX_VISIBLE_TOGGLES),
                 (int)r.x + 6, (int)r.y + 5, 12, GRAY);
    }
    if (total_inputs == 0) {
        DrawText("(no inputs)", 12, py + 30, 12, GRAY);
    }

    /* Steps row */
    Rectangle minus_r = ipanel_steps_minus(sh);
    Rectangle plus_r  = ipanel_steps_plus(sh);
    DrawText("Steps", 8, (int)minus_r.y - 16, 12, (Color){180, 180, 180, 255});
    DrawRectangleRec(minus_r, (Color){80, 80, 90, 255});
    DrawRectangleLinesEx(minus_r, 1, LIGHTGRAY);
    DrawText("-", (int)(minus_r.x + minus_r.width / 2 - 3), (int)(minus_r.y + 3), 16, RAYWHITE);
    DrawRectangleRec(plus_r, (Color){80, 80, 90, 255});
    DrawRectangleLinesEx(plus_r, 1, LIGHTGRAY);
    DrawText("+", (int)(plus_r.x + plus_r.width / 2 - 4), (int)(plus_r.y + 3), 16, RAYWHITE);
    char step_lbl[16]; snprintf(step_lbl, sizeof(step_lbl), "%d", e->steps);
    int tw = MeasureText(step_lbl, 16);
    DrawText(step_lbl, (int)(EDITOR_SIDEBAR_W / 2 - tw / 2),
             (int)(minus_r.y + 3), 16, RAYWHITE);

    /* RUN button */
    Rectangle run_r = ipanel_run_btn(sh);
    Color run_bg = CheckCollisionPointRec(mp, run_r) ? (Color){80, 180, 110, 255}
                                                     : (Color){50, 140, 80, 255};
    DrawRectangleRec(run_r, run_bg);
    DrawRectangleLinesEx(run_r, 2, BLACK);
    int rtw = MeasureText("RUN", 20);
    DrawText("RUN",
             (int)(run_r.x + run_r.width / 2 - rtw / 2),
             (int)(run_r.y + run_r.height / 2 - 10),
             20, RAYWHITE);
}

void editor_draw(const EditorState *e, const CanvasState *cs, Camera2D cam,
                 int screen_w, int screen_h) {
    Vector2 mp = GetMousePosition();
    int py = panel_top(screen_h);

    /* Sidebar background (top portion only — stops above the inputs panel) */
    DrawRectangle(0, 0, EDITOR_SIDEBAR_W, py, (Color){50, 50, 60, 255});
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

    /* Wiring rubber-band */
    if (e->mode == MODE_WIRING && e->wire_src_node >= 0 && e->wire_src_node < cs->node_count) {
        Vector2 src_pin    = canvas_node_output_pin(&cs->nodes[e->wire_src_node]);
        Vector2 src_screen = GetWorldToScreen2D(src_pin, cam);
        DrawLineEx(src_screen, mp, 2.0f, (Color){80, 130, 180, 255});
    }

    /* Bottom panel: inputs + waveform */
    draw_inputs_panel(e, cs, screen_w, screen_h);
    Rectangle wave_bounds = {
        EDITOR_SIDEBAR_W, py,
        screen_w - EDITOR_SIDEBAR_W, EDITOR_PANEL_H
    };
    DrawRectangleRec(wave_bounds, (Color){250, 250, 252, 255});
    waveform_draw(e->tracks, e->track_count, wave_bounds);

    /* Header */
    DrawRectangle(EDITOR_SIDEBAR_W, 0, screen_w - EDITOR_SIDEBAR_W,
                  EDITOR_HEADER_H, (Color){200, 200, 200, 255});
    DrawText(TextFormat("File: %s", e->file_path[0] ? e->file_path : "(none)"),
             EDITOR_SIDEBAR_W + 10, 8, 16, BLACK);

    /* Status bar */
    int sb_y = screen_h - EDITOR_STATUS_H;
    DrawRectangle(0, sb_y, screen_w, EDITOR_STATUS_H, (Color){40, 40, 50, 255});
    if (e->status[0] && GetTime() < e->status_until) {
        DrawText(e->status, 10, sb_y + 6, 14, RAYWHITE);
    }
}
