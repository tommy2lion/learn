# Step 2 — Supplement Design

A design proposal implementing the requirements in [`step2-supplement-req.md`](step2-supplement-req.md). Sits on top of the current Step-2 codebase; introduces no new architectural layer; reuses the existing framework / domain / app split.

All six open questions in the requirements doc have been answered by the user; the "Decision" boxes in each section restate the resolution and explain how the design reflects it. The summary table is in §12.

**Architectural framing (user note, see req §"Why these requirements are raised now").** Although these requirements look like incremental polish, they are architectural in nature — promoting wires to first-class renderable objects and introducing the external/internal view duality both touch the seam between domain and app. Implementing them now, before Step 3's chipset and shape-DSL work, avoids re-cutting that seam twice.

---

## 0. Recap of what exists today

- **Net identity is already implicit.** Every input pin that consumes the same wire name (`component.in_wires[p]` in `src/domain/component.h:39`) sits on the same electrical net. The producer's `name` *is* the net's identifier. We therefore do **not** need a new `net_t` domain struct; we promote the existing wire-name string to first-class status by adding a small geometry sidecar.

- **Wires have no geometry today.** The renderer computes each consumer edge on the fly: `draw_line(producer_out_pin, consumer_in_pin, 2.0f, COLOR_DARKGRAY)` at `src/app/circuit_canvas_widget.c:604` and `:613`. There is one straight line per `(consumer, pin)`, drawn diagonally when the two endpoints are not co-linear.

- **Component positions persist** via the `# @layout` annotation block, parsed in `circuit_io.c:116` and emitted at `circuit_io.c:322`. Wire geometry has no equivalent yet.

- **There is no concept of "current display mode."** The canvas always draws the full schematic.

These four observations frame the four design areas below: a wire-geometry layer (§1), an orthogonal router (§2), a net-aware renderer that draws junction dots and highlights (§3, §4), a serializer extension (§5), and a display-mode toggle (§6).

---

## 1. Wire-geometry layer

### 1.1 Where it lives

A new module `src/app/wire_geometry.{h,c}` in the **app layer** — *not* in domain.

**Rationale.** The geometry has zero effect on simulation correctness; it is a visual property of the schematic. Keeping it out of `circuit_t` preserves the domain's purity (no headless test ever needs to know about a wire's bend points) and matches the layering rule that domain has no UI concerns. The geometry is reconstructed when a `.dcs` file is loaded and is consulted only by `circuit_canvas_widget` and by `circuit_io_serialize`.

> **Decision (req Q#2 — confirmed by user).** Nets stay implicit; we do **not** introduce a `net_t` domain struct. **Net membership is computed on demand** by grouping segments and consumer pins by the shared producer wire-name. Geometry is a per-circuit sidecar in the app layer.

### 1.2 Data structures (sketch)

```
// wire_geometry.h  (app layer, no raylib)

typedef struct {
    vec2_t a, b;                       // endpoints; one of (a.x==b.x) or (a.y==b.y)
} wire_segment_t;

class tagt_wire_net_geom {
    char            wire_name[DOMAIN_NAME_LEN];   // = producer component's name
    wire_segment_t *segs;                          // owned
    int             seg_count, seg_cap;
};
typedef class tagt_wire_net_geom wire_net_geom_t;

class tagt_wire_geometry {
    wire_net_geom_t *nets;                         // one per producer wire-name
    int              net_count, net_cap;
};
typedef class tagt_wire_geometry wire_geometry_t;
```

Invariants:

- Every segment is purely horizontal (`a.y == b.y`) or purely vertical (`a.x == b.x`). Enforced by an assertion in the segment-add helper.
- For each wire in `circuit_t` (every component's output-wire name + every input that drives an output) there is **at most one** `wire_net_geom_t`. Missing entries trigger auto-routing (§2.3).
- The geometry struct is owned by the canvas widget and freed alongside it. It is rebuilt from scratch when the circuit is replaced (`circuit_canvas_widget_set_circuit`).

### 1.3 API surface

```
void wire_geometry_init   (wire_geometry_t *g);
void wire_geometry_release(wire_geometry_t *g);

// O(net_count); -1 if not found.
int  wire_geometry_find   (const wire_geometry_t *g, const char *wire_name);

// Returns net index (existing or new). Negative on OOM.
int  wire_geometry_get_or_create(wire_geometry_t *g, const char *wire_name);

// Replace the segment list for one net. Validates H/V invariant; returns 0/-1.
int  wire_geometry_set_segments(wire_geometry_t *g, int net_idx,
                                const wire_segment_t *segs, int count);

// Iteration helpers (read-only).
const wire_net_geom_t *wire_geometry_net(const wire_geometry_t *g, int idx);
```

### 1.4 Coupling to the circuit

`circuit_canvas_widget_t` gains one field:

```
wire_geometry_t wires;     // sibling to existing `circuit_t *circuit`
```

When the canvas mutates connectivity (`connect_to_pin`, `disconnect_input`, component deletion that orphans wires) it must:

1. For new connections: call `auto_route_wire(net_name, producer_pin, consumer_pin)` to seed segments.
2. For deletions / disconnects: if no consumer remains for a given net, discard that net's geometry (so the next reconnection re-routes from scratch).

This is centralised in three new static helpers in `circuit_canvas_widget.c`; the existing code paths get short call-outs at the four mutation sites already listed in the file.

---

## 2. Orthogonal routing

### 2.1 When routing runs

Routing computes new segments only at three moments:

1. **New wire created** by the user in `CMODE_WIRING` (or by a paste / future undo).
2. **Endpoint moved** — when a component the wire is connected to is dragged, every wire touching that component is re-routed (a single component move can dirty many nets).
3. **Wire-geometry block missing on load** — for backwards-compatibility with old `.dcs` files (and human-authored files that don't bother with `# @wires`).

Routing does **not** run on every frame. Once a wire is laid, the segments persist verbatim — including user-tweaked bend points — until one of the three triggers fires.

> **Decision (req Q#1 — confirmed by user: "allow users to drag the bend point").** Compute the route **once on creation**, persist it, allow the user to drag bend points (lands in Phase 2 of the implementation; see §8). Re-route only when an endpoint moves. This preserves user intent across edit sessions and is what the demo style requires.

### 2.2 Router algorithm (initial: Z-router)

For one connection from producer-pin `S` to consumer-pin `D`:

```
if S.y == D.y:                # straight horizontal
    emit one segment S → D
elif S.x == D.x:              # straight vertical
    emit one segment S → D
else:                         # Z-shape: two bends, midpoint column
    mid_x = S.x + (D.x - S.x) / 2     # snapped to grid (8 px)
    emit S → (mid_x, S.y)
    emit (mid_x, S.y) → (mid_x, D.y)
    emit (mid_x, D.y) → D
```

A Z-router is preferred over a simple L-router because it produces visibly cleaner schematics when the producer and consumer have several gates between them vertically — exactly the layout of `step2-supplement-demo2.jpg`.

No obstacle avoidance in v1. Wires may visually cross components or other wires. This is acceptable because:

- The demo images themselves contain many crossings (electrically unconnected ones); the convention is "dot = connect, no-dot = crossover".
- The user can drag bend points to clean up (Phase 2).

A future polish can plug in an A\*-on-grid router with component-rectangle obstacles, behind the same `auto_route_wire` entry point.

### 2.3 Fan-out: one producer, many consumers

A net often drives several inputs (see the `G_S` fan-out in `demo2.jpg`). The current data model stores one consumer-side `in_wires[pin]` per consumer; the geometry layer reflects that natively by storing **all the consumer routes for one net inside the same `wire_net_geom_t.segs[]`**.

The router emits, per consumer, the Z-route from `S` to that consumer. The renderer (§3) then collapses any segments that overlap when drawing — and §4 picks up shared endpoints as candidates for junction dots.

A more sophisticated variant — building a Steiner tree with shared trunks like in `demo2.jpg` — is deferred to Phase 3 of the implementation (after the dump-everything-as-Z-paths version ships and we can see how often it produces visual clutter on real circuits).

### 2.4 Manual bend-point editing

In Phase 2 of the implementation:

- Hovering a vertical segment shows an east-west drag cursor (move segment horizontally); hovering a horizontal segment shows a north-south cursor.
- Dragging a segment shifts it perpendicular to its axis. The two adjacent segments stretch to maintain connectivity.
- The endpoints (the producer pin and consumer pin) are *not* draggable from the wire — they move only when the component moves.

This drag UX lives entirely in `circuit_canvas_widget.c` as a new `CMODE_WIRE_EDIT` (or reuses `CMODE_DRAGGING` with a different sub-target).

---

## 3. Junction dots

### 3.1 Derivation rule

For every net's segment list, compute the set of all segment endpoints (each segment contributes 2 points). For each unique point `P`:

```
count(P) = number of segment-endpoints that exactly equal P, across this net
```

A junction dot is drawn at `P` iff:

- `count(P) >= 3`, **OR**
- `count(P) == 2` and the two segments meeting at `P` are **not collinear** (i.e. one is H and one is V; both forming a corner).

Collinear meetings (two H segments end-to-end, or two V segments end-to-end) are visual no-ops and get no dot.

**Crossovers are never dots.** A horizontal segment of net `A` that passes geometrically through a point on a vertical segment of net `B` does **not** trigger a dot. Junction dots are *per-net* — derived only from endpoints of segments belonging to the same `wire_net_geom_t`. This matches the convention in `demo2.jpg`.

### 3.2 Rendering

After all wire segments are drawn (§4.2), iterate every net's junction points and call:

```
g->draw_circle(g->self, junction_pt, DOT_R, COLOR_BLACK);
```

with `DOT_R = 3.0f` (scaled with `cam_zoom`, so it stays visible at small zoom and not grotesque at large zoom).

### 3.3 Producer/consumer pin endpoints

The pin terminals (producer's output pin, consumer's input pin) are *always* one end of at least one segment, so they show up naturally in the junction analysis. In practice they only end up with a dot when they're a fan-out trunk (`count >= 3`). Otherwise they look like a clean pin tip, consistent with the demos.

---

## 4. Net highlight on click

### 4.1 Hit-test

A new helper:

```
int wire_geometry_pick(const wire_geometry_t *g, vec2_t world, float tol_world,
                       const char **net_name_out);
```

For every segment of every net, compute the distance from `world` to the segment; return the net name if the closest distance is `< tol_world`. `tol_world` is `4.0f / cam_zoom` so the click tolerance stays at ≈4 screen px.

### 4.2 Render order

The current canvas draw routine is, in order: components → wires → wiring-preview → marquee. We extend this to:

```
1. components
2. wires:
     for each net N:
         color = (N.name == highlighted_net) ? COLOR_BLUE : COLOR_DARKGRAY
         thickness = (N.name == highlighted_net) ? 3.5f : 2.0f
         draw all of N's segments at (color, thickness)
3. junction dots
     for each net N:
         dot_color = (N.name == highlighted_net) ? COLOR_BLUE : COLOR_BLACK
         draw N's junctions
4. pin terminals
     (highlighted-net pins drawn slightly larger / blue)
5. wiring-preview, marquee
```

### 4.3 Selection-state field on the canvas

```
char highlighted_net[DOMAIN_NAME_LEN];   // "" = no highlight
```

A new helper `circuit_canvas_widget_set_highlight(self, wire_name)` clears or sets it. Click handler logic:

```
if (mouse_left_press and mode == CMODE_IDLE):
    if (hit_component or hit_pin) -> existing handling
    else if (wire_geometry_pick(...) returns net):
        set highlight to that net
    else:
        clear highlight
```

> **Decision (req Q#6 — confirmed by user: "highlighted in bold and bright colors, pin terminals emphasized").** Highlight color is **orange** (`COLOR_ORANGE`, `0xFFA500FF` — bright, distinct from `COLOR_BLUE` which the in-progress wiring preview already uses, and from `COLOR_DARKGRAY` which is the default wire colour). Stroke is **thickened** from 2.0 → 3.5 px. Pin terminals on the highlighted net are drawn 1.5× larger in the same orange. Single-net highlight at a time; clicking another net replaces it; clicking on empty space clears it.

---

## 5. `.dcs` file format extensions

Two new annotation blocks, both backward-compatible (older parsers will ignore them).

### 5.1 `# @wires` block — per-net geometry

```
# @wires
# @  net=g1
# @    h 100,200 → 180,200
# @    v 180,200 → 180,260
# @    h 180,260 → 240,260
# @  net=g2
# @    h 100,300 → 240,300
```

Grammar (one entry per net):

```
"net=" wire_name
  for each segment of this net:
    ("h" | "v") space x1 "," y1 space "→" space x2 "," y2
```

The `→` separator (or `->`, both accepted) and the leading `h`/`v` tag are redundant with the coordinates but exist as a sanity check for human readers and a fast-fail signal for malformed files. The serializer always emits `→`; the parser accepts either.

Coordinates are floats (matching `vec2_t`), printed with `%g` like the existing `# @layout` block.

> **Decision (req Q#3 — user delegated the choice; difficulty flagged for CLI-generated files).** Add a **separate** `# @wires` block rather than overloading `# @layout`. Layout (component positions) and wiring geometry are different concerns; keeping them separate makes hand-editing safer and lets a circuit have one without the other. Named nets appear in the file via their producer wire-name (`net=<wire_name>`). See §5.5 for how this works when the CLI produces files without geometry.

### 5.2 `# @display_mode` annotation — persisted view setting

```
# @display_mode = internal
```

Single-line, valid values `internal` or `external`. Absent → defaults to `internal` for backwards compatibility.

### 5.3 Parser / serializer wiring

- `circuit_io_parse` (`circuit_io.c:161`) gains a `parse_wires_entry` analogous to `parse_layout_entry`, with a separate `in_wires` state flag triggered by `# @wires`. Output goes into a new optional `wire_geometry_t` parameter (added to `circuit_io_parse`'s signature, default-NULL — see §5.4).
- `circuit_io_serialize` (`circuit_io.c:289`) gains parallel emission of `# @wires` whenever the supplied geometry is non-empty, and `# @display_mode` whenever the supplied mode is non-default.

### 5.4 Signature changes (minimal-impact form)

To avoid forcing every domain test to pass a geometry pointer, we keep the existing entry points and add overloads:

```
// circuit_io.h — existing entry points unchanged
circuit_t *circuit_io_parse    (const char *text, char *err_out, int err_len);
char      *circuit_io_serialize(const circuit_t *c);

// new (app-facing) extended entry points
circuit_t *circuit_io_parse_ex (const char *text, char *err_out, int err_len,
                                wire_geometry_t *geom_out,    // optional
                                int *display_mode_out);        // optional
char      *circuit_io_serialize_ex(const circuit_t *c,
                                   const wire_geometry_t *geom,  // optional
                                   int display_mode);
```

The existing entry points become thin wrappers that pass `NULL` / default. Domain tests stay green; only the app layer touches the `_ex` versions.

### 5.5 CLI-generated files: lazy auto-layout + auto-route on first open

The headless CLI builds `.dcs` files from algebraic descriptions and has no notion of pixel coordinates. Such files arrive at the GUI with neither `# @layout` nor `# @wires`. The user flagged this as the hardest part of the format question — addressed here:

When `circuit_canvas_widget_set_circuit` is called and the supplied `wire_geometry_t` is empty / NULL:

1. The widget runs the existing `auto_layout` (topological-depth column placement) to populate component positions — same code path already exercised by Phase 2.6 in the current Step-2 codebase.
2. For each component-input / output-input wire in the loaded circuit, the widget calls `auto_route_wire(producer_pin, consumer_pin)` (§2.2) to seed the geometry.
3. The widget marks the canvas "dirty" so the user knows the file is no longer byte-identical to disk (existing dirty-flag mechanism in `dcs_app.c`).
4. On the next save, `circuit_io_serialize_ex` emits both `# @layout` and `# @wires`; reopening the file is then a no-op visually.

This means **the CLI does not need to learn anything about geometry**. It continues to emit the structural lines (`inputs:`, `outputs:`, `<name> = <gate>(...)`) it produces today. The GUI is the source of truth for the visual layer, and a CLI-built file becomes a fully-laid-out GUI file after one open + save cycle.

The same logic also covers two related cases for free:

- A hand-written `.dcs` file with neither annotation block.
- A file with `# @layout` but no `# @wires` (e.g. created before this supplement landed).

In both cases the existing pieces of layout are preserved and only the missing pieces are auto-generated.

---

## 6. Display modes

### 6.1 Storage

A new enum in `editor_state.h`:

```
typedef enum { DISPLAY_INTERNAL, DISPLAY_EXTERNAL } display_mode_t;
```

Stored on `circuit_canvas_widget_t` as `display_mode_t display_mode;`. Default `DISPLAY_INTERNAL`. Persisted by the serializer (§5.2).

### 6.2 External-view rendering — with an architectural hook

When `display_mode == DISPLAY_EXTERNAL`, the canvas dispatches through a **renderer hook** so the visual can be replaced per circuit / component later:

```
// new in src/app/external_view.h
typedef void (*external_render_fn)(igraph_t *g,
                                   const circuit_t *c,
                                   const external_view_metadata_t *meta,
                                   rect_t box);   // world-space target rect

class tagt_external_view_metadata {
    char         display_name[DOMAIN_NAME_LEN];   // shown inside the box; default = file basename
    pin_style_t  input_styles [DOMAIN_MAX_IO];    // per-input render hint, default PIN_STYLE_NORMAL
    pin_style_t  output_styles[DOMAIN_MAX_IO];    // per-output render hint
    external_render_fn render;                    // NULL → use default_external_render
};

typedef enum {
    PIN_STYLE_NORMAL,    // plain stub line + label
    PIN_STYLE_CLOCK,     // triangle notch inside the box at this pin (D-FF style; reserved)
    PIN_STYLE_INVERTED,  // small bubble at the pin (reserved for NAND/NOR primitives)
} pin_style_t;
```

The **default** renderer (`default_external_render`, supplied at startup whenever `meta->render == NULL`) draws exactly what was previously described:

- A plain rectangle, sized from the I/O pin count.
- Display name centered inside, in a larger font.
- Input pins on the left edge, output pins on the right, each with its name.

Per-pin `pin_style_t` overrides are honoured by the default renderer **for `PIN_STYLE_NORMAL` and `PIN_STYLE_CLOCK` only** in this supplement. The clock style is implemented now (a small inward-pointing triangle drawn at the pin's box-edge), even though no Step-2 primitive sets it — because the user explicitly cited D flip-flop clock arrows as a target use case (`req §R5`). The architectural seam is then exercised once and proven correct.

`PIN_STYLE_INVERTED` and any other styles are reserved (defined in the enum, ignored by the default renderer) until Step 3.

Custom whole-shape rendering (replacing the entire box with, e.g., a triangle for an inverter or a D-FF-shaped capsule) is what the **`render` function pointer** enables. Setting it lives in Step 3 (Phase 3.1, the shape DSL) — but the hook ships now so Step 3 doesn't have to retrofit it.

The `circuit_t` itself does **not** gain an `external_view_metadata_t` field — that would push UI concerns into the domain. Instead, the metadata sits on `circuit_canvas_widget_t` as `external_view_metadata_t external_meta;`, parallel to `display_mode`. Persistence (§6.4) carries it through the file.

Hit-testing in external mode is disabled (no editing); only camera pan / zoom and the mode-toggle controls are accepted.

> **Decision (req Q#4 — user: "rectangles in most cases, retain custom shapes; arrows like D-FF clock signals").** Default rectangle + name + labeled pins. **Architectural hook** is in place via `external_render_fn` and per-pin `pin_style_t`; `PIN_STYLE_CLOCK` is implemented in the default renderer as a proof-of-concept. Full whole-shape custom rendering and additional pin styles defer to Step 3 Phase 3.1 — but the seam is cut now, not later.

### 6.3 Toggle UX — easy *and* extensible

Three entry points, all driving the same single `display_mode_t` field — so future view modes can plug in without touching multiple UI sites:

1. **Menu item** under **View** → "Black-box view" (toggleable, checkmark when active). Wired via the existing `menu_add_item`.
2. **Keyboard shortcut**: `Ctrl+B` (B for Black-box). Easy mnemonic, doesn't clash with existing app shortcuts.
3. **Sidebar toggle button** at the bottom of the side toolbar — a small icon labelled `[ ]` / `[■]` indicating the current mode. The user's hand is already on the toolbar for placement; flipping the view from there is the fastest path during editing.

All three entry points call one helper:

```
void circuit_canvas_widget_set_display_mode(circuit_canvas_widget_t *self, display_mode_t mode);
```

which updates the field, marks the canvas dirty (so save persists it), and triggers a redraw. Any future fourth UI entry point — a context-menu item, a multi-tab tab-bar control, an embedded thumbnail — uses the same helper. No fan-out of behaviour across UI sites.

The toggle is **per circuit** (per file), not app-global. The current single-circuit GUI stores it on `circuit_canvas_widget_t`; if Step 3 introduces multi-tab editing each tab carries its own widget instance, and the mode comes along automatically.

> **Decision (req Q#5 — user delegated, asking for "easy operation + flexible scalability").** Three entry points (menu, shortcut, sidebar) all funnelling through `circuit_canvas_widget_set_display_mode`. Single state field, single behaviour-trigger function, multiple discoverable UX surfaces. Adding a fourth surface later requires zero refactoring.

### 6.4 Persisting external-view metadata

The `external_view_metadata_t` defined in §6.2 is persisted via a small additional set of `.dcs` annotation lines, parallel to `# @layout` / `# @wires`:

```
# @display_mode = internal
# @display_name = HalfAdder
# @pin_style    = __input:CLK : clock
```

- `# @display_name` overrides the file-basename default. Optional.
- `# @pin_style` lines bind a pin name (using the same `__input:` / `__output:` prefixes the existing `# @layout` block uses) to a `pin_style_t` value. Unknown style names are ignored (forward-compatible — Step 3 can add `inverted`, `tristate`, etc. without breaking older builds).
- The whole-shape `render` function pointer is **not** persisted in v1 (it would require a shape-DSL serialization, which is Step 3 work). The file always rebinds `render = NULL` (default rectangle); the pin-style overrides drive any per-pin customisation visible today.

`circuit_io_parse_ex` / `circuit_io_serialize_ex` are extended to round-trip these three annotations alongside `# @wires`.

### 6.5 What is **not** in this supplement

The user can switch a circuit's view to external, but no other circuit yet *embeds* this circuit as a sub-component. The actual instantiation-as-a-box (and recursive evaluation) is Step 3 Phase 3.6 work. Doing the renderer + storage now means Step 3 only needs to add the instantiation and evaluation machinery, not the visualization.

A full shape DSL (so a circuit can declare an arbitrary geometric symbol — D-FF capsule, op-amp triangle, custom IEEE-shape) is also still Step 3 work (Phase 3.1). The hook for it is the `external_render_fn` function pointer in §6.2; Step 3 fills it in.

---

## 7. Visual-style refresh

Bring the existing per-component render code in `circuit_canvas_widget.c` closer to the IEC-style shown in the demos:

- **AND gate** — rectangle with `&` glyph centered. (Currently a rectangle with the component's name inside.) Add a thin output pin stub.
- **OR gate** — rectangle with `≥1` glyph (IEC convention) or `OR` text fallback.
- **NOT gate** — triangle pointing right with a small bubble at the output.
- **Bubble** — small unfilled circle (radius ≈4 px) at any output where the gate is "inverting" — applies to NOT; reserved for NAND/NOR in Step 3.
- **Component label** — drawn above or to the side of the shape (not inside), so the IEC glyph stays readable. The component's `name` becomes the externally-visible signal label.

No shape DSL yet; this is still hardcoded per-component rendering. The DSL is Phase 3.1. The visual refresh just gets the render-code style in line with the demos so the screenshots match the design.

---

## 8. Phasing (proposed commit order)

Small, independently-buildable commits — one feature per commit, all keep the 213-test suite green:

| # | Topic | Scope | Tests added |
|---|---|---|---|
| 1 | `wire_geometry.{h,c}` skeleton in app layer | data structures + init/release/get_or_create/set_segments | `test_wire_geometry.c` (unit) |
| 2 | Z-router (`auto_route_wire`) | one connection → segments | unit |
| 3 | Renderer uses geometry | replace `draw_line(a,b)` at canvas:604,613 with segment iteration | visual; existing tests pass |
| 4 | Mutation hooks | new wire / drag / delete trigger re-route or geometry-discard | integration test exercising the mutation paths |
| 5 | Junction dots | derivation + render | unit (junction-derivation logic is pure) |
| 6 | Click-to-highlight net | `wire_geometry_pick`, render branch, click handler | unit (pick) + integration (handler) |
| 7 | `# @wires` parser + serializer | `_ex` entry points; existing round-trip tests still green | new round-trip tests for `# @wires` |
| 8 | `display_mode` + `set_display_mode` helper + default-rectangle renderer | field on canvas; menu item, `Ctrl+B`, sidebar button — all calling the one helper | manual visual check (GUI only) |
| 9 | `external_view_metadata_t` + render hook + `PIN_STYLE_CLOCK` | architectural seam from §6.2 + clock-pin proof-of-concept | unit test of the default renderer with normal vs clock pin styles (mock igraph) |
| 10 | `# @display_mode` / `# @display_name` / `# @pin_style` parser + serializer | round-trip | round-trip tests for all three annotations |
| 11 | Visual-style refresh (IEC glyphs, bubbles) | per-gate render code touch-up | manual visual check |
| 12 | Manual bend-point editing | new sub-mode in `circuit_canvas_widget` | manual + a hit-test unit test |
| 13 | Steiner-trunk router (optional / nice-to-have) | replace per-consumer Z-paths with shared trunks | manual visual check |

Items 12 and 13 are stretch — they can slide into Step 3 if time-boxed. The minimum that satisfies all six numbered requirements is items 1–11.

---

## 9. Test plan

- **Unit (domain unchanged):** all 43 + 80 existing circuit / circuit_io tests stay green. The `_ex` entry points get their own tests for round-tripping `# @wires` and `# @display_mode`.
- **Unit (app):** new `test_wire_geometry.c` covers init/release, segment validation (H/V invariant), router output for the three Z-router cases (horiz, vert, Z), junction derivation, and `wire_geometry_pick` distance math.
- **Integration:** `test_circuit_canvas_supplement.c` (mock igraph) exercises:
  - create wire → geometry exists with valid Z-segments.
  - drag component → wires touching it re-route.
  - delete producer component → orphan-net geometry cleared.
  - click on a wire segment → highlighted_net set to that net.
  - click outside → highlighted_net cleared.
  - toggle display mode → external-view render branch reached.
- **Manual visual:** open `circuits/demo1-and3.dcs` and a few existing demos in `dcs_gui.exe`, confirm orthogonal routing, dots at fan-outs, click-to-highlight works, `Ctrl+B` switches view, save → reload → geometry preserved.

The current "213 tests" baseline expands to roughly 230–240 after this work.

---

## 10. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Router produces ugly Z-paths that overlap components on dense circuits | Phase 2 manual bend-drag lets the user clean up; an A\* router can replace `auto_route_wire` later without touching anything else. |
| Two-level data store (logical wires in `circuit_t`, geometry in `wire_geometry_t`) drifts out of sync | Mutation hooks (§2.1) are concentrated in three sites in `circuit_canvas_widget.c`; an `assert_geometry_consistent` debug helper iterates `circuit_t.components[].in_wires` and asserts every wire name has a geometry entry (cheap, run in `_DEBUG` builds only). |
| Old `.dcs` files have no `# @wires` block → ugly straight-line auto-routes on first open | Auto-router runs lazily on load when a wire has no geometry; first save then persists the result. After one save/load cycle the file matches the new format. |
| Hit-test on segments is wrong at extreme zoom | `tol_world = 4.0f / cam_zoom` keeps screen-pixel tolerance constant; existing tests for pin hit-testing use the same pattern. |
| Backward compatibility with the headless CLI | The CLI continues to use `circuit_io_parse` / `circuit_io_serialize` (without `_ex`); it never sees geometry. Verified by the 11 existing CLI tests. |

---

## 11. Out of scope (still deferred to Step 3)

- ANSI/IEEE shape DSL and toolbar icons (Phases 3.1, 3.2).
- Variable pin counts (Phase 3.3) — chipsets with >2 inputs still impossible.
- Recursive evaluation of nested DCS sub-components (Phase 3.6) — the *external* render exists but no other circuit can yet *use* it.
- Real circuit elements / analog signals (Phase 3.5, 3.8).
- A\*-based obstacle-avoiding router; Steiner-tree trunk routing.

---

## 12. Summary of decisions

All six open questions from the requirements doc have been resolved. The final answers below drive the implementation:

| req Q | Topic | Resolution | Where it lands in this design |
|---|---|---|---|
| Q1 | Routing strategy | Route once on creation, persist, **let users drag bend points**. Re-route only when a component endpoint moves. | §2.1, §2.4; Phase-2 commit in §8. |
| Q2 | Net data model | **Compute on demand** — net identity = producer wire-name string. No new domain struct. | §1.1, §1.2; §3.1, §4.1. |
| Q3 | Wire-geometry syntax | New **`# @wires`** block separate from `# @layout`. CLI-built files (no geometry) get auto-laid-out + auto-routed by the GUI on first open. | §5.1, §5.5. |
| Q4 | External-mode shape | **Rectangle** default + name + labeled pins. **Architectural hook** (`external_render_fn` + per-pin `pin_style_t`) is in place now; `PIN_STYLE_CLOCK` ships as the proof-of-concept. Full shape DSL → Step 3 Phase 3.1. | §6.2, §6.4. |
| Q5 | Mode toggle UX | **Three entry points** (View menu item, `Ctrl+B`, sidebar toggle button) all funnelling through one `set_display_mode` helper. Easy *and* extensible. | §6.3. |
| Q6 | Highlight visuals | **Orange** color (`0xFFA500FF`) + thickened stroke (2.0 → 3.5 px) + enlarged 1.5× pin terminals on the highlighted net. Single-net highlight at a time. | §4.2, §4.3. |

The user's overall framing — that these are architectural concerns worth resolving before Step 3, not Step-3 incremental work — is reflected in the design choices: a renderer hook (Q4) instead of a hardcoded rectangle, a uniform `set_display_mode` helper (Q5) instead of duplicated wiring, and a persistent geometry sidecar (Q1+Q3) instead of recomputed-every-frame paths. None of these add complexity disproportionate to the requirement, and all three pre-empt the most invasive churn Step 3 would otherwise force.
