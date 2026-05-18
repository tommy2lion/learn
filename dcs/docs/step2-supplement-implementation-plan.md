# Step 2 Supplement — Implementation Plan

A phase-by-phase, commit-by-commit execution plan for the work described in [`step2-supplement-req.md`](step2-supplement-req.md) and [`step2-supplement-design.md`](step2-supplement-design.md). Each phase is one focused commit. Phases land in order; the suite must stay green between every pair.

> **How to read this document.** Each phase entry lists: **Goal** · **Touches** (files created/modified) · **Steps** (the actionable sequence) · **Tests** (what to add) · **Done when** (the verification gate) · **Commit subject** (the commit message's first line). Code-level details (function signatures, data layouts, algorithms) are in the design doc; this plan refers to them rather than restating them.

---

## 0. Pre-flight

Do these once, before Phase 1.

1. **Re-read the design doc.** Sections referenced repeatedly below: §1 (wire_geometry layer), §2 (Z-router), §3 (junction dots), §4 (highlight), §5 (`.dcs` extensions), §6 (display modes), §8 (commit order).
2. **Establish the baseline.**
   - `cd dcs && make test` → expect all 213 tests pass.
   - `make` → both `dcs_cli.exe` and `dcs_gui.exe` build clean (zero warnings).
3. **Sanity-check the manual GUI.** Launch `dcs_gui.exe`, open `circuits/demo1-and3.dcs`, confirm wires render today as straight (diagonal) lines and clicking does nothing special. This is the visual baseline against which later phases are compared.
4. **Branch policy.** Stay on `main`; one commit per phase as before. Skip the auto-bumped `package.json` / `package-lock.json` in every commit (per project conventions). Smaller commits preferred when a phase splits cleanly.

---

## Cross-cutting invariants

These hold across every phase. Any commit that violates one is wrong, regardless of whether tests pass.

| Invariant | Why |
|---|---|
| All existing tests stay green; the suite grows monotonically. | The supplement is additive; nothing in domain or framework semantics is changing. |
| No `framework/*` file gains a `#include "../app/*"` or `"../domain/*"`. | Layering rule (req.md context). |
| No `domain/*` file gains a `#include "../app/*"` or any raylib/Win32 dependency. | Layering rule. |
| `prototype_version/` is untouched. | Frozen baseline (commit `4a24207`). |
| The CLI (`cli/main.c`, `dcs_cli.exe`) keeps emitting the same `.dcs` format it does today (no `# @layout`, no `# @wires`). | Decision in design §5.5 — GUI is the source of truth for visual data. |
| No diagonal wire segments remain in any renderer path after Phase 3 lands. | The single hardest-edge requirement (R1). |

---

## Phase 1 — `wire_geometry` skeleton

**Goal.** Introduce the data structures + lifecycle for per-circuit wire geometry, in the app layer. No rendering yet; no callers yet.

**Touches.**
- `src/app/wire_geometry.h` (new)
- `src/app/wire_geometry.c` (new)
- `test/test_wire_geometry.c` (new)
- `Makefile` — add the new `.c` and the new test binary

**Steps.**
1. Implement the struct layout from design §1.2: `wire_segment_t`, `wire_net_geom_t`, `wire_geometry_t`.
2. Implement `wire_geometry_init` / `wire_geometry_release` / `wire_geometry_find` / `wire_geometry_get_or_create` / `wire_geometry_set_segments` / `wire_geometry_net` (design §1.3).
3. `wire_geometry_set_segments` asserts the H/V invariant per segment (`assert(a.x == b.x || a.y == b.y)`); reject `(a == b)` zero-length segments.
4. Update `Makefile` to compile `wire_geometry.o` into the app object set and to build `test_wire_geometry`.

**Tests** (in `test/test_wire_geometry.c`).
- Init / release with zero, one, many nets.
- `get_or_create` returns same index for repeated names; distinct indices for distinct names.
- `set_segments` accepts pure-H / pure-V segments; rejects diagonal segments (`return -1` and leaves the net's segment list unchanged).
- `find` returns -1 for unknown names; correct index for known.
- Capacity growth: add 1, 2, …, 64 nets; no leaks (instrument with an internal counter or run under address-sanitizer mode if available).

**Done when.**
- `make test` shows the new `test_wire_geometry` binary running and all its cases passing.
- All previously-passing tests still pass.
- `wire_geometry.h` has zero includes of `raylib.h` / `windows.h` / `../framework/widgets/*`.

**Commit subject.** `Add wire_geometry skeleton (supplement Phase 1)`

---

## Phase 2 — Z-router

**Goal.** Turn a (producer-pin, consumer-pin) coordinate pair into a list of orthogonal segments.

**Touches.**
- `src/app/wire_geometry.h` — add `auto_route_wire` declaration.
- `src/app/wire_geometry.c` — implement.
- `test/test_wire_geometry.c` — extend.

**Steps.**
1. Implement `void auto_route_wire(wire_geometry_t *g, const char *wire_name, vec2_t producer_pin, vec2_t consumer_pin)` per design §2.2:
   - Three branches: horizontal-only, vertical-only, Z-shape (two bends).
   - Snap `mid_x` to the existing grid step (`GRID = 8.0f` — see `circuit_canvas_widget.c`).
2. The function appends to the net's segment list — it does **not** replace existing segments. (One net can have many consumers; each one's path is added in turn.)

**Tests.**
- Producer / consumer on the same row → one horizontal segment, correct endpoints.
- Producer / consumer on the same column → one vertical segment.
- Z-shape: producer at (100, 100), consumer at (300, 200) → three segments, midpoint column snapped to grid.
- Two consumers fed by the same producer: two `auto_route_wire` calls → net has 3 + 3 = 6 segments (or fewer if any consumer is co-linear).

**Done when.**
- New unit tests pass; the previously-added Phase-1 tests still pass.
- The router never produces a diagonal segment, verified by an assertion in `wire_geometry_set_segments` (already added in Phase 1).

**Commit subject.** `Add Z-router for orthogonal wire routing (supplement Phase 2)`

---

## Phase 3 — Renderer uses geometry

**Goal.** Wires render as orthogonal segments from the geometry sidecar. First user-visible change: no more diagonal wires.

**Touches.**
- `src/app/circuit_canvas_widget.h` — add `wire_geometry_t wires;` field (per design §1.4).
- `src/app/circuit_canvas_widget.c`:
  - `circuit_canvas_widget_create` — `wire_geometry_init(&cw->wires)` and seed routes for every existing connection in the circuit.
  - `circuit_canvas_widget_set_circuit` — release old geometry, init new, seed routes.
  - The wire-draw block at `circuit_canvas_widget.c:599-614` — replace the two `draw_line(a, b, …)` calls with a loop over the matching net's segments.
  - Destructor — `wire_geometry_release(&cw->wires)`.

**Steps.**
1. Add the field and lifecycle hooks first; verify the widget compiles and tests pass (no behaviour change yet — geometry exists but is unused).
2. Add a `seed_geometry_from_circuit(cw)` helper: for every component `c[i]`, for each non-empty `c->in_wires[p]`, call `auto_route_wire(&cw->wires, c->in_wires[p], producer_pin, consumer_pin)`. Same for outputs.
3. Swap the renderer. For each consumer in the wire-draw loop, look up the net by `wire_geometry_find(&cw->wires, wire_name)`; if found, iterate its segments and call `g->draw_line(seg.a, seg.b, 2.0f, COLOR_DARKGRAY)`. If not found (defensive), fall back to the old direct-line draw.

**Tests.**
- No new unit tests yet (renderer is GUI-visual). Existing tests stay green.
- Manual visual check: open `circuits/demo1-and3.dcs`, confirm wires are now L-shaped or Z-shaped, never diagonal.

**Done when.**
- `make test` is green.
- Manual visual confirms orthogonal rendering on at least two example circuits.

**Commit subject.** `Render wires via geometry segments (supplement Phase 3)`

---

## Phase 4 — Mutation hooks

**Goal.** Keep the geometry in sync as the user edits the circuit.

**Touches.**
- `src/app/circuit_canvas_widget.c` — touch the three mutation sites:
  - `connect_to_pin` (around line 374) — after the connection is committed, call `auto_route_wire`.
  - `disconnect_input` (around line 368) — if the now-disconnected wire has no remaining consumers, `wire_geometry` removes that net entirely.
  - Component deletion (cascade in `delete_node` around line 459) — for each wire that lost its producer, drop its net; for each wire that lost a consumer, re-route the remaining consumers (no shared trunks in v1, so this is just regenerate-from-scratch).
- Optional internal helper `static void reseat_wire_geometry(cw, const char *wire_name)` that erases the net and re-runs `auto_route_wire` for every current consumer of that wire — call it from the component-drag handler so wires touching a moved component stay attached.

**Steps.**
1. Add an `assert_geometry_consistent(cw)` debug-only helper that asserts every wire name referenced in `circuit_t.components[].in_wires` has a geometry entry, and vice-versa. Call it after each mutation (gated by `#ifndef NDEBUG`).
2. Wire the three mutation sites.
3. Component drag: at the end of every drag in `CMODE_DRAGGING`, call `reseat_wire_geometry` for every wire that touches the dragged component.

**Tests** (in a new `test/test_circuit_canvas_supplement.c`, using a stub `igraph_t` similar to `test_igraph.c`).
- Create a circuit, add a component, connect → geometry has the corresponding net with 1–3 segments.
- Drag the component → geometry's segment endpoints follow.
- Delete the producer of a wire → the net is gone from geometry.
- Delete one consumer of a fan-out wire → the net stays, with the orphan consumer's segments removed.

**Done when.**
- `make test` green; the new integration test file is part of the suite.
- `assert_geometry_consistent` never fires during manual editing sessions.

**Commit subject.** `Maintain wire geometry on circuit mutations (supplement Phase 4)`

---

## Phase 5 — Junction dots

**Goal.** Draw the black fan-out dots from `step2-supplement-demo2.jpg`.

**Touches.**
- `src/app/wire_geometry.h` — declare `int wire_geometry_junctions(const wire_net_geom_t *net, vec2_t *out, int max_out);`
- `src/app/wire_geometry.c` — implement per design §3.1.
- `src/app/circuit_canvas_widget.c` — after the wire-draw loop, iterate every net and draw `g->draw_circle(seg_endpoint, 3.0f, COLOR_BLACK)` at each derived junction.

**Steps.**
1. Implement `wire_geometry_junctions`:
   - Walk every segment endpoint; build `(point, count_at_point)` pairs.
   - A point is a junction iff `count ≥ 3`, **OR** `count == 2` and the two segments touching there are non-collinear.
   - Cap output at `max_out`; return actual count (so callers can detect truncation).
2. Add a small geometry helper `segments_collinear(seg_a, seg_b)` (pure-logic, no `igraph_t`).
3. Plug into the renderer after wires draw, before pin terminals.

**Tests.**
- Three segments meeting at a `+` → one junction.
- Three segments meeting at a `T` → one junction.
- Two collinear segments end-to-end → no junction.
- Two perpendicular segments meeting at a corner → one junction.
- A fan-out net (one producer, two consumers, Z-routed) → exactly one junction at the branch point.

**Done when.**
- Unit tests green; visual check on `demo2`-style circuit shows dots at fan-outs and **not** at unrelated crossings.

**Commit subject.** `Draw junction dots at wire fan-out points (supplement Phase 5)`

---

## Phase 6 — Click-to-highlight net

**Goal.** Left-click on any wire segment highlights its whole net.

**Touches.**
- `src/app/wire_geometry.h` — declare `int wire_geometry_pick(const wire_geometry_t *g, vec2_t world, float tol, const char **net_name_out);`
- `src/app/wire_geometry.c` — implement (point-to-segment distance, return closest match within `tol`).
- `src/app/circuit_canvas_widget.h` — add `char highlighted_net[DOMAIN_NAME_LEN];`
- `src/app/circuit_canvas_widget.c`:
  - Mouse-press handler: in `CMODE_IDLE`, after pin/component hit-tests fail, call `wire_geometry_pick` with `tol = 4.0f / cam_zoom`. Set `highlighted_net` accordingly; clear it if nothing was hit.
  - Renderer: draw each net's segments in `COLOR_ORANGE (0xFFA500FF)` at thickness 3.5 px when its name matches `highlighted_net`; draw junction dots and connected pin terminals in the same orange at 1.5× their normal radius.

**Steps.**
1. Implement and unit-test the pick helper first.
2. Add the highlight field and the helper `circuit_canvas_widget_set_highlight(self, name)` (NULL or `""` to clear).
3. Wire it into the mouse handler.
4. Add the colour/thickness branches in the renderer.

**Tests.**
- Unit: distance from a point to several known H and V segments — boundary cases (exact endpoint, exactly on segment, exactly `tol` away, just outside `tol`).
- Unit: pick across a multi-net circuit returns the correct name.
- Integration: a left-click whose world-space coords are on a known segment sets `highlighted_net`.
- Integration: a left-click on empty space clears `highlighted_net`.

**Done when.**
- New tests pass.
- Manual visual: clicking a wire colours the whole net (and only that net) orange; clicking elsewhere clears it.

**Commit subject.** `Add click-to-highlight wire net (supplement Phase 6)`

---

## Phase 7 — `# @wires` parser + serializer

**Goal.** Persist the routed geometry into `.dcs` files; restore it on load.

**Touches.**
- `src/domain/circuit_io.h` — declare the `_ex` entry points (design §5.4 signature).
- `src/domain/circuit_io.c` — implement; old entry points become wrappers.
- `src/app/dcs_app.c` — switch the load/save paths to call `_ex` with the widget's `wire_geometry_t`.
- `test/test_circuit_io.c` — round-trip tests for `# @wires`.

**Steps.**
1. Introduce a sibling state flag `in_wires` in the parser analogous to `in_layout` (`circuit_io.c:169`). Trigger by `# @wires`; parse `# @  net=<name>` and `# @    h x,y → x,y` / `v x,y → x,y` lines; populate `geom_out`.
2. Accept both `→` and `->` in the parser; emit `→` (UTF-8) in the serializer. The serializer must not break the existing `# @layout` block.
3. `_ex` entry points wrap the parser/serializer signatures (design §5.4). The non-`_ex` versions pass `NULL` for geom.
4. Update `dcs_app.c` open/save paths.

**Tests.**
- Round-trip: build a circuit, route some wires, serialise, parse with `_ex`, compare geometry → exact match.
- Backward compat: a `.dcs` file with `# @layout` but no `# @wires` parses successfully with `_ex` and yields an empty geometry (so the GUI auto-routes on first open — see Phase 8 hook).
- Forward compat: a `.dcs` file with unknown `# @something` annotations is preserved across parse-only (ignored, not errored).
- Existing 80 `circuit_io` tests stay green.

**Done when.**
- All round-trip tests pass.
- A user-arranged circuit saved and reopened in the GUI looks identical to before save.

**Commit subject.** `Persist wire geometry in .dcs files (supplement Phase 7)`

---

## Phase 8 — `display_mode` + helper + UI surfaces

**Goal.** Toggle between internal (schematic) and external (black-box) views. Three discoverable UX surfaces, one underlying state field.

**Touches.**
- `src/app/editor_state.h` — `typedef enum { DISPLAY_INTERNAL, DISPLAY_EXTERNAL } display_mode_t;`
- `src/app/circuit_canvas_widget.h` — add `display_mode_t display_mode;` and declare `void circuit_canvas_widget_set_display_mode(self, mode);`
- `src/app/circuit_canvas_widget.c` — implement the helper, branch the render path to `draw_external_view(g, cw)` when mode is `DISPLAY_EXTERNAL`.
- New `src/app/external_view.{h,c}` — start with the default rectangle renderer only (the metadata struct is added in Phase 9).
- `src/app/dcs_app.c` — register a **View → Black-box view** menu item (toggle); register `Ctrl+B`.
- `src/app/side_toolbar.{h,c}` — add a small toggle button at the bottom of the toolbar.

**Steps.**
1. Field + helper first; no UI yet. Verify build / tests still pass.
2. Implement the default rectangle renderer per design §6.2 (plain box; name centered; labeled I/O pins).
3. Wire the menu item — clicking calls `set_display_mode(self, !current)` plus an icon-state update.
4. Wire the keyboard shortcut.
5. Wire the sidebar button.

**Tests.**
- Manual visual: each entry point flips the view; the three entry points stay in sync (their check-state reflects the field).
- Integration unit test: calling `set_display_mode(self, DISPLAY_EXTERNAL)` switches the field, marks the canvas dirty, and a subsequent `draw` call routes through the external path (use a mock igraph counting which drawing primitives were called).

**Done when.**
- All three UX entry points work, all funnelling through the one helper.
- Switching to external view shows a clean rectangle with pin labels; switching back returns to the schematic.

**Commit subject.** `Add external/internal display mode toggle (supplement Phase 8)`

---

## Phase 9 — `external_view_metadata_t` + render hook + `PIN_STYLE_CLOCK`

**Goal.** Exercise the architectural seam from design §6.2 by shipping the hook plus one concrete custom pin style.

**Touches.**
- `src/app/external_view.h` — add `external_view_metadata_t`, `pin_style_t`, `external_render_fn`.
- `src/app/external_view.c` — extend the default renderer to honour `PIN_STYLE_CLOCK` (small inward-pointing triangle at the pin's box edge).
- `src/app/circuit_canvas_widget.{h,c}` — add `external_view_metadata_t external_meta;`. Default-initialise: `display_name = file_basename`, all pin styles `PIN_STYLE_NORMAL`, `render = NULL` (→ default renderer).
- `src/app/dcs_app.c` — on file open, populate `display_name` from the basename.

**Steps.**
1. Define the struct and enum.
2. Refactor the Phase-8 `draw_external_view` to dispatch via `meta->render` (falling back to `default_external_render` when NULL).
3. Implement `PIN_STYLE_CLOCK` in the default renderer.
4. Default-initialise the metadata struct on canvas create / circuit set.

**Tests.**
- Unit: feed the default renderer a metadata where input `[0]` is `PIN_STYLE_CLOCK`; assert it draws an extra triangle primitive that pin (mock igraph counts).
- Unit: feed it metadata where `render` is set to a stub function pointer; assert the stub is called instead of the default.

**Done when.**
- Hook compiles, tests pass, the canvas still defaults to the plain rectangle.
- Manual visual: temporarily flip the first input pin's style to `PIN_STYLE_CLOCK` in code; confirm the triangle appears at the right place. Revert before commit.

**Commit subject.** `Add external-view render hook and clock pin style (supplement Phase 9)`

---

## Phase 10 — Persistence for display mode + external-view metadata

**Goal.** `# @display_mode`, `# @display_name`, `# @pin_style` round-trip cleanly.

**Touches.**
- `src/domain/circuit_io.c` — extend the parser / serializer to handle the three annotations. Pass through via the `_ex` entry points (extend signature as needed — `external_view_metadata_t *` parameter, optional).
- `src/app/dcs_app.c` — pass the canvas's `external_meta` to/from `circuit_io_*_ex`.
- `test/test_circuit_io.c` — round-trip tests.

**Steps.**
1. Extend `circuit_io_parse_ex` and `circuit_io_serialize_ex` signatures (design §6.4). Old entry points still pass NULL.
2. Parser: an unknown style name in `# @pin_style` is silently ignored (forward-compatible).
3. Serializer: emit `# @display_mode` only when not the default; emit `# @display_name` only when not the file basename; emit `# @pin_style` lines only for pins with non-`NORMAL` style.

**Tests.**
- Round-trip a circuit set to external mode → reopens in external mode.
- Round-trip a circuit with one input pin marked clock-style → reopens with that pin still clock-styled.
- Old `.dcs` file with none of the three annotations → opens with defaults (no error, no warning).

**Done when.**
- All round-trip tests pass; the previously-set 213 + Phase 5–9 tests remain green.
- Edit-save-reopen cycle preserves every setting.

**Commit subject.** `Persist display mode and external-view metadata (supplement Phase 10)`

---

## Phase 11 — Visual style refresh

**Goal.** Bring the gate visuals closer to the IEC-style of the demo images (rectangle + `&` / `≥1`, NOT triangle, output bubbles).

**Touches.**
- `src/app/circuit_canvas_widget.c` — the per-component draw branch (around `draw_node`).
- Optionally a tiny new helper file `src/app/gate_visuals.{h,c}` if the per-gate draw logic gets long enough that inlining it cluttering `circuit_canvas_widget.c`.

**Steps.**
1. AND: rectangle + centered `&` glyph.
2. OR: rectangle + centered `≥1` glyph (with `OR` fallback if the font doesn't have the symbol).
3. NOT: right-pointing triangle + small unfilled bubble at the tip.
4. Component label moves outside the shape (above for now). The component's `name` is the externally-visible label.
5. Pin stubs: short (~6 px) line from each pin into the gate's body, so the wire never visually touches the gate outline directly.

**Tests.**
- No unit tests (pure visual). Manual visual: open the existing demo circuits, side-by-side with `step2-supplement-demo1.jpg`, confirm overall style is comparable.

**Done when.**
- All existing tests green.
- Manual visual approves the refresh against the demo images.

**Commit subject.** `Refresh schematic visual style toward IEC convention (supplement Phase 11)`

---

## Phase 12 — Manual bend-point editing (stretch)

**Goal.** Let the user drag a wire segment perpendicular to its axis to reshape the route. Persists like any other geometry change.

**Touches.**
- `src/app/circuit_canvas_widget.h` — add a new sub-state `CMODE_WIRE_EDIT` (or hang a `wire_edit_target` off `CMODE_DRAGGING`).
- `src/app/circuit_canvas_widget.c` — segment hover detection (cursor changes), drag handler, segment shift + neighbour-segment stretch.
- `src/app/wire_geometry.h/c` — declare `void wire_geometry_shift_segment(g, net_idx, seg_idx, float delta)` that performs the shift and adjusts adjacent segments to maintain connectivity.

**Steps.**
1. Implement `wire_geometry_shift_segment` first — pure-logic, unit-test against several segment-list shapes.
2. Add hover detection: in `CMODE_IDLE`, if mouse is within `tol` of a segment, change the cursor (E-W for vertical segments, N-S for horizontal).
3. Add the drag sub-mode: press-and-hold on a segment enters drag; drag perpendicular shifts the segment; release commits.
4. Endpoints (terminals at producer/consumer pins) are not draggable — they move only with components.

**Tests.**
- Unit: shift a middle segment, assert the two neighbours stretch correctly.
- Integration: simulate a press-drag-release sequence on a Z-route; assert the resulting geometry matches expectations.

**Done when.**
- Unit + integration tests green.
- Manual: the demo circuit's wires can be dragged into different shapes; save-reopen preserves the new shapes.

**Commit subject.** `Allow user to drag wire bend points (supplement Phase 12)`

---

## Phase 13 — Steiner-trunk router (stretch / nice-to-have)

**Goal.** A fan-out with N consumers draws one shared horizontal trunk plus N vertical drops (like `step2-supplement-demo2.jpg`), instead of N independent Z-paths.

**Touches.**
- `src/app/wire_geometry.c` — replace `auto_route_wire`'s per-consumer logic with a tree builder when the same `wire_name` has been seen multiple times in the same routing pass.
- Tests: new visual unit test verifying shared-trunk output for the multi-consumer case.

**Steps.**
1. Decide between (a) build a single trunk eagerly when a new consumer is added, (b) call `wire_geometry_rebuild_net(g, name)` from the mutation hooks instead of `auto_route_wire`. Option (b) is simpler and runs only on mutation. Prefer (b).
2. Implement: collect all consumer pins of the net, pick a trunk Y (median of pin Y's snapped to grid), emit horizontal trunk from producer X to max consumer X at that Y, plus a vertical drop per consumer from the trunk to that pin's Y.
3. Junction-dot derivation (§3.1) is unchanged — it picks up the new T-joins automatically.

**Done when.**
- Tests green; visual confirms fan-outs use shared trunks.
- The existing per-consumer Z-path tests are removed or replaced (their old output is no longer correct for >1 consumer).

**Commit subject.** `Use shared trunks for fan-out wire routing (supplement Phase 13)`

> **Optional.** If Phase 13 is skipped, the supplement still satisfies every numbered requirement in `step2-supplement-req.md`. It's a polish item, not a requirement.

---

## Definition of done (whole supplement)

The supplement is complete when **all** of the following hold:

1. Phases 1–11 are committed; the codebase contains no diagonal wire segments in any render path.
2. The full test suite (now ≈ 230–240 cases) is green; the baseline 213 are untouched in result.
3. Both binaries (`dcs_cli.exe`, `dcs_gui.exe`) build with zero warnings.
4. Manual visual: opening `circuits/demo1-and3.dcs` shows orthogonal wires; opening a fan-out circuit shows junction dots only at fan-out points; clicking a wire highlights the net orange; `Ctrl+B` toggles between the schematic and a rectangle-with-pins view; save-reopen preserves layout, wiring, mode, and any clock-pin annotations.
5. The CLI continues to produce `.dcs` files in the old format and they open correctly in the new GUI (auto-laid-out, auto-routed, dirty-flag set, save updates the file in place).
6. Phase 12 (bend-point editing) is **strongly recommended** before Step 3 begins — it's the user-visible flexibility the design promised. Phase 13 (Steiner trunks) is optional.

After this is done, the seven open questions in [`step3-plan.md`](step3-plan.md) become the next thing to look at.

---

## Risk register (per phase)

| Phase | Highest-risk failure mode | Mitigation |
|---|---|---|
| 1 | H/V invariant asserted at the wrong layer | Assert in `wire_geometry_set_segments`; the public API rejects bad input. Internal helpers can build invalid intermediate state if needed, but only via private functions that re-validate before returning. |
| 2–3 | Old `.dcs` files render with auto-routed paths that overlap components | Cosmetic only; user can drag bends (Phase 12) or re-save to commit the new layout. |
| 4 | Geometry / circuit drift after a complex mutation | `assert_geometry_consistent` (debug-only) catches drift the moment it happens. |
| 5 | Junction dots appear at unrelated crossings | Derivation is strictly per-net — different-net crossings cannot produce a dot by construction (§3.1). |
| 6 | Hit-tolerance feels wrong at extreme zoom | `tol = 4.0f / cam_zoom` keeps screen-pixel tolerance constant; same pattern as existing pin hit-tests. |
| 7 | Round-trip not byte-identical | Acceptable as long as round-trip is semantically equivalent (same parsed structure). The serializer's emission is deterministic; that's what the tests check. |
| 8 | Three UI entry points drift out of sync | They all read the same field, so check-state is derived, not duplicated. |
| 9 | Hook called too eagerly (e.g. each frame) and slows rendering | The hook is only consulted during the external-view draw, which renders one box; performance is irrelevant at this scale. |
| 10 | A `.dcs` file with unknown style names errors out | Parser silently ignores unknown styles; tested in Phase 10 step 2. |
| 11 | Symbol glyphs missing from raylib's default font | Fall back to ASCII (`AND`, `OR`, `NOT`); the rectangle / triangle outline still conveys the gate identity. |
| 12 | Drag UX feels brittle (segments don't follow cursor cleanly) | Unit-test the math first; integrate after the algebra is proven. |
| 13 | Stretch — skip if time-boxed. | — |

---

## Notes for the implementer

- Commit one phase at a time, with the suggested subject line as the first line. Body bullets describe what changed (files, lines), per established commit-message style in this repo.
- Skip the auto-bumped `package.json` / `package-lock.json` from each commit (per `CLAUDE.md`-equivalent project conventions).
- The GUI is manually tested by the user — when a phase has a "manual visual" gate, surface the explicit thing to check, but do **not** claim the phase complete until the user confirms.
- If a phase grows beyond ~300 lines of diff, look for a sub-split. The Step-2 refactor commits set the expectation: tight, focused, easy to review.
- Refer back to the design doc for any signature or algorithm question. Don't re-derive — re-read.
