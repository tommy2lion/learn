# Step 2 Code Review

A walkthrough of the Step-2 codebase against the seven aspects in `step2-code-review-req.md`. The codebase under review is `dcs/src/{framework,domain,app,gui}/` and `dcs/cli/main.c` as of commit `9f04b0a`.

---

## 0. Executive summary

| Aspect | Verdict |
|---|---|
| 1. Coding-convention compliance | ✓ mostly — 4 minor `tagt_` violations |
| 2. Code safety | ⚠ several actionable findings (realloc-leak pattern; some unchecked allocations) |
| 3. Framework independence | ✓ all six layering invariants satisfied |
| 4. Responsibility / SoC | ✓ clean — one specific encapsulation slip in `dcs_app` |
| 5. Testability | ✓ for framework + domain (187 tests); ✗ app layer has zero tests |
| 6. Extensibility | ✓ minimal touch-points for new gates / widgets / backends; one structural cap (`DOMAIN_MAX_PINS_IN = 2`) |
| 7. Forward-requirement support (7-1 → 7-8) | mixed — see Section 7 |

**Overall:** the architecture is sound and the layering holds. The actionable items are bounded — a small protocol pass through the realloc/malloc-return sites, an app-layer test suite scaffold, and the four convention nits. None are blocking; all are good candidates for a Phase 3.0 hardening commit.

---

## 1. Coding-convention compliance

The conventions from `step2-refactor-design.md` §0 (and `step2-review-of-refactor-design.md`) are: `class`/`interface` macros expanding to `struct`; struct tags prefixed `tagt_<noun>`; type aliases `<noun>_t`; lowercase `snake_case` for all identifiers except macros (UPPER_SNAKE_CASE); header guards `DCS_<MODULE>_<FILE>_H`; static helpers in `.c`; first parameter is `self`.

### Findings

**Compliant** for: `class`/`interface` macros (used everywhere — no bare `struct` keyword in declarations); header guards (every `.h` conforms); naming (no Hungarian, no camelCase, no `m_x` member-prefix anywhere); `self` convention (every method's first parameter); API hygiene (no public symbols are accidentally `static`; no internal helpers leak into headers); macros (UPPER_SNAKE_CASE).

**Violations** — four `tagt_` / `_t` mismatches:

| File:line | Identifier | Note |
|---|---|---|
| `cli/main.c:18` | `InputSpec` | No `tagt_` tag, also CamelCase. CLI-internal type. |
| `src/app/input_panel.h:12` | `input_toggle_t` | Has `_t` alias but missing `tagt_` tag. **Public header — most worth fixing.** |
| `src/app/dcs_app.c:133` | `stim_ctx_t` | File-scope helper struct. |
| `src/app/side_toolbar.c:13` | `stb_item_t` | File-scope const-array element type. |

**Recommendation.** All four are one-line fixes (rewrite as `class tagt_<noun> { ... }; typedef class tagt_<noun> <noun>_t;`). The `input_panel.h` one is in a public header and should be brought to convention; the other three are file-scope and merely inconsistent rather than risky.

---

## 2. Code safety

Ranked findings from a focused safety audit across `src/` and `cli/main.c`:

### High severity

1. **Realloc-leak pattern.** Several places assign `realloc(...)` directly back without checking the return:
   - `src/framework/widgets/panel.c:68` — `nn = realloc(self->children, …)`; if NULL, `self->children` still points at old block, but downstream code already updated `self->child_cap` mentally; iteration would overflow.
   - `src/domain/circuit.c:46-49` — same pattern in `push_wire`'s realloc.
   - Similar in `src/domain/circuit.c` for `components` realloc.

   **Fix:** save into a temporary, NULL-check, then commit. Standard pattern.

2. **`auto_layout` malloc returns unchecked.** `src/app/circuit_canvas_widget.c:87,98,99` — `depths`, `col_total`, `col_idx` all allocated via `calloc` / `malloc`; on failure the loops dereference NULL.

   **Fix:** check each allocation, fall back to a no-op layout (every widget at origin) on OOM rather than crash.

3. **`simulation_run` ignores `waveform_add_track` errors.** `src/domain/simulation.c:21-22` — adds tracks unconditionally; if any allocation fails partway, downstream `waveform_set_value` calls at lines 33-34 use indices that may exceed `track_count`. The bounds-check inside `waveform_set_value` saves us from a crash, but the simulation silently produces a partial waveform with no diagnostic.

   **Fix:** check each `waveform_add_track` return; on failure either bail with a status callback or shrink the run.

4. **Stale-pointer race in `input_panel`.** `src/app/dcs_app.c:55-56,68-69` patches the `input_panel`'s `circuit` pointer through a direct field cast (`*(const circuit_t **)&((input_panel_t *)…)->circuit = c;`) instead of via a setter. This works *today* because we always destroy the old circuit and then patch immediately, but it bypasses encapsulation and would break if `input_panel` ever cached anything derived from the circuit.

   **Fix:** add `input_panel_set_circuit()` to the public API and call it; remove the cast.

### Medium severity

5. **Bit-packing overflow in `wire_at`.** `src/app/circuit_canvas_widget.c:409` packs `(component_index << 8) | (pin_index & 0xFF)` into a 32-bit `node_ref_t.index`. When `component_count > 256`, indices silently truncate. A circuit that big is unrealistic today, but the encoding is fragile and inscrutable.

   **Fix:** add a `pin` field to `node_ref_t`, or document the encoding with a runtime assertion.

6. **Unbounded loop in `next_name`.** `src/app/circuit_canvas_widget.c:299-303` — generator increments a counter and re-checks; theoretically spins forever if every prefix-N name is taken.

   **Fix:** cap retries at e.g. 10000 and return failure; status-message on exhaustion.

7. **`menu_add_item` silently drops items past 16.** `src/framework/widgets/menu.c` — returns `void`; no caller feedback.

   **Fix:** return `int` (added index, or -1 on overflow), or assert.

8. **`circuit_io_serialize` cap is a static estimate.** `src/domain/circuit_io.c` — buffer sized as `4096 + N*256 + (in+out)*96`. Adversarially-long names + many components could overflow. `snprintf` truncates silently.

   **Fix:** track bytes written vs cap; either grow the buffer (realloc-up) or fail explicitly.

9. **`waveform_set_value` is bounds-checked but `t->values` may be NULL.** `src/domain/waveform.c` — when `step_count == 0`, `values` stays NULL; calls are gated by `step >= 0 && step < self->step_count`, so the NULL is never dereferenced, but the design is fragile.

   **Fix:** assert `step_count > 0` before tracks are added, or initialize `values` even for zero-step (a 0-byte alloc is fine on most C runtimes; just avoid the special case).

### Low / advisory

10. `src/cli/main.c:50-55` — fixed `toks[64]`; > 64 comma-separated tokens truncate. CLI-only, low concern.
11. `src/domain/circuit.c:119` — `c->vt->evaluate(...)` with no NULL guard on `vt`. In practice all `gate_*_create` functions set it; defensive `if (!c->vt) continue;` would harden against partial-construction bugs.
12. `src/app/circuit_canvas_widget.c` — `component_depth` is recursive with memoization but no cycle detection; the parser today rejects feedback loops, so circuits are guaranteed acyclic. If chipsets are added (Step 3) and recursive instantiation is allowed, add cycle detection.

### Summary

The destroy chains are clean (no use-after-free, ownership is unambiguous: domain → circuit owns components/wires, framework → panel owns children, app → dcs_app owns frame which owns root which owns everything). The actionable theme is **error-path hardening**, not structural risk.

---

## 3. Framework independence

The architecture defines six layering invariants (see `step2-refactor-design.md` §A.1):

1. `domain/*` never includes `framework/widgets/`, `framework/graphics/`, `framework/platform/`, or `app/*`.
2. `framework/*` never includes `domain/*` or `app/*`.
3. Only `framework/graphics/graph_raylib.c` includes `raylib.h`.
4. Only `framework/platform/platform_windows.c` includes `windows.h` / `commdlg.h`.
5. App / CLI / GUI entry points may include anything.
6. The framework should compile in isolation given a stub `iplatform_t*` and `igraph_t*`.

### Findings

All six invariants hold. Counts:

- `raylib.h`: 1 inclusion — `src/framework/graphics/graph_raylib.c:3` ✓
- `windows.h` / `commdlg.h`: 1 inclusion — `src/framework/platform/platform_windows.c:10-11` ✓
- Domain → framework includes: only `src/domain/component.h` includes `framework/core/oo.h` and `framework/core/rect.h`. Both are *interface-free* utility headers (the `class`/`interface` macros and `vec2_t`/`rect_t` types). This is the design's stated intent: `oo.h` and `rect.h` are shared types. ✓
- Framework → domain or app: zero includes. ✓
- App → both: yes, as expected.

One observation worth noting (not a violation):

- `src/framework/core/message.h` includes `src/framework/graphics/igraph.h` to reference `igraph_key_t` and `igraph_mouse_btn_t` in its event payloads. This is an *interface* dependency, not a concrete one — `igraph.h` is the abstraction. Coupling event types to input enums is intentional and matches §A.5's design.

### Conclusion

The framework is architecturally independent. It could be packaged as a static library (`libdcs_framework.a`) today: clients link it together with concrete impls of `iplatform_create()` and `graph_create()`, plus their app code. No domain-layer or app-layer source is reachable from any framework translation unit.

---

## 4. Responsibility assignment & separation of concerns

### Layering

The three-layer split (framework / domain / app) is upheld in source. Each layer's responsibilities:

- **Domain** (`src/domain/`): pure logic — circuits, components, simulation, .dcs I/O. Zero UI dependencies (verified). The CLI consumes this layer directly; the GUI consumes it via the app layer.
- **Framework** (`src/framework/`): generic widget framework. Depends only on `iplatform` and `igraph` interfaces. Contains widget primitives (button, label, panel, splitter, menu, …), focus / quit / event dispatch, and the `frame_t` event loop.
- **App** (`src/app/`): DCS-specific composition. Subclasses widgets where needed (`circuit_canvas_widget`, `timing_canvas_widget`, `side_toolbar`, `input_panel`, `divider_widget`); composes them via `dcs_app`. This is the only layer that knows about both other layers.

### Specific encapsulation observation

- `src/app/dcs_app.c:55-56,68-69` patches `input_panel.circuit` via a *direct field cast* (`*(const circuit_t **)&((input_panel_t *)app->input_panel)->circuit = c`) instead of a setter. The panel's struct is technically transparent (defined in the header), but the cast is a code smell — it bypasses the public API, relies on field layout, and would silently break if `input_panel_t` ever caches derived state. Trivial to fix: add `void input_panel_set_circuit(input_panel_t *self, const circuit_t *c)`.

### Camera-implementation duplication

`src/framework/widgets/canvas_widget.{h,c}` provides a generic pannable/zoomable canvas with its own camera + scissor logic. `src/app/circuit_canvas_widget` does **not** extend it; instead it carries its own duplicate camera state (`cam_target`, `cam_offset`, `cam_zoom`, `panning`) plus its own pan/zoom event handling.

The reason was pragmatic: the editor modes (placing, wiring, dragging, marquee) and the camera state interact tightly inside `handle_event`, and overriding the framework canvas's vtable would have forced awkward composition. But the result is two copies of the camera math (and two copies of `screen_to_world`). When Step 3 lands the shape-DSL + renderer (which will likely benefit from a shared camera abstraction), this is worth revisiting.

### Dependency Inversion

Where it counts (graphics + platform), code depends on `igraph_t*` and `iplatform_t*` — the interface types — not on `graph_raylib_t` or `platform_windows_t`. The concrete implementations are picked at link time only. Adding a Cairo backend or a Linux platform impl is a matter of replacing one file each.

The component hierarchy follows the same pattern: callers see `component_t*` and dispatch through `component_vt_t`. `gate_and_create` / `gate_or_create` / `gate_not_create` are opaque factories.

### Conclusion

Clean. One field-cast smell to fix (3-line change), one duplicate-camera observation that's defensible today and revisitable in Step 3.

---

## 5. Testability and mock-friendliness

### Test counts

| Layer | Suite | Tests |
|---|---|---|
| iplatform | `test/test_iplatform.c` | 8 |
| igraph (offline) | `test/test_igraph.c` | 39 |
| widget framework | `test/test_widgets.c` | 32 |
| circuit + component | `test/test_circuit.c` | 43 |
| circuit_io (incl. layout block) | `test/test_circuit_io.c` | 80 |
| CLI integration | `test/test_cli.sh` | 11 |
| **Total** | | **213** |

The domain (123) and framework (79) layers have strong coverage. **The app layer has zero tests.**

### Mock-friendliness

- **`igraph_t`**: pure vtable struct. A mock (e.g., a no-op draw + scripted input) is easy to produce. `test_igraph.c` already validates the vtable structurally.
- **`iplatform_t`**: same shape — pure vtable. Trivial to mock for headless tests (e.g., file dialogs returning canned paths, time fixed).
- **Domain types**: take no UI deps. `test_circuit.c` and `test_circuit_io.c` already exercise them headlessly.
- **Framework widgets**: `widget_t` vtable is mock-friendly. `test_widgets.c` exercises hit-test, child enumeration, button click flow, focus manager — all without opening a window.

### App-layer gap

`circuit_canvas_widget`, `dcs_app`, `side_toolbar`, `input_panel`, `divider_widget` — none have unit tests. Several pieces of logic are complete unit-test candidates:

- `circuit_canvas_widget`'s hit-testing (`hit_node`, `hit_output_pin`, `hit_input_pin`, `wire_at`)
- mode transitions (PLACING → IDLE, WIRING → IDLE, DRAGGING multi-select delta movement)
- `auto_layout` topological-depth correctness
- smart-rename-on-OUTPUT-wire
- `selection_add_in_rect` / `selection_clear` / `remove_selection`

All of these are pure functions over `circuit_t` and the canvas state. They don't need a real `igraph_t`. Adding `test/test_circuit_canvas.c` against a mock igraph would close the gap.

### Conclusion

Architecture is highly mockable; the gap is pure follow-through. Adding ~30-50 app-layer tests is a one-commit chore.

---

## 6. Extensibility

A "what would I do to add X?" walkthrough:

| Change | Touch points | Effort |
|---|---|---|
| New graphics backend (e.g., Cairo) | Add `src/framework/graphics/graph_cairo.c` providing `graph_create()` + the same vtable. Update Makefile to swap the source. | Small |
| New platform (e.g., Linux) | Add `src/framework/platform/platform_linux.c` (skeleton already exists). Implement zenity-based dialogs, POSIX file I/O, `clock_gettime`-based time. | Small |
| New primitive component (e.g., NAND, XOR) | Add `src/domain/gate_<x>.c` with vtable + factory. Register a parser keyword in `circuit_io.c`'s `parse_gate_expr`. Add a sidebar entry in `side_toolbar`. | Small (3 spots) |
| New widget kind | Subclass `widget_t`: provide a vtable (draw, handle_event, destroy, optional child_count/child_at). Add to factory or direct constructor. | Small |
| Renderer change for an existing component | Edit the relevant case in `circuit_canvas_widget.c::draw_node`. (Open-coded; a shape DSL would push this onto the component, but currently it's centralized.) | Small |

### Structural caps to watch

- `DOMAIN_MAX_PINS_IN = 2` is hard-coded in `component.h` and `circuit_canvas_widget.c`. Chipsets and many useful gates need >2 inputs. Refactor: change `char in_wires[2][NAME_LEN]` to dynamic (`char (*in_wires)[NAME_LEN]` allocated by the constructor based on `pin_count_in`). Touches: `component.h`, all `gate_*_create`, `circuit_add_component`, `circuit_evaluate`, `circuit_canvas_widget`'s pin-position math.
- `DOMAIN_MAX_IO = 16` (max external inputs/outputs per circuit) — fine for current scale; bump if needed.
- `MAX_SELECTION = 64`, `EDITOR_MAX_TOGGLES = 16`, `EDITOR_MAX_TRACKS = 32`, `EDITOR_MAX_STEPS = 64` — all bounded but generous; revisit if a circuit grows into hundreds of components.

### Dependency-Inversion check

App and framework code reference `igraph_t*` and `iplatform_t*` exclusively. Concrete impls are picked at link time. The sole exception is the GUI/CLI entry point (`gui/main.c`, `cli/main.c`), which is correct — that's where the wiring happens.

### Conclusion

Extensibility is good. The only structural cap that would meaningfully bite Step 3 is `DOMAIN_MAX_PINS_IN`; everything else is either runtime-configurable or trivially loosened.

---

## 7. Extensibility review against Step-3 future requirements

Each item below: **what's needed**, **what the current architecture already supports**, and **rough scope**.

### 7-1. ANSI/IEEE standard symbols (shape DSL)

**Needed:** a description language ("line, arc, semicircle, full circle" with the gate's centre as origin) parsed once per component type and rendered via igraph's drawing primitives.

**Today:** components are drawn with hard-coded boxes in `circuit_canvas_widget::draw_node` (rectangle + uppercase label). No shape system.

**Architecture impact:** Add `src/domain/shape.h/c` (or `src/framework/graphics/shape.h/c`) — a small AST + interpreter that takes igraph + a transform and emits draw calls. Add a `shape_t *(*shape)(void)` accessor to `component_vt_t` (or static const member). Update `draw_node` to interpret the shape. **Medium scope** (~300-500 LOC across a few files; easy to test in isolation).

### 7-2. Simulation package (extend 7-1 to user components)

**Needed:** the same shape system used for primitives must be usable for any custom component (e.g., chipset rendering at the system level). Plus a registry mapping component types → shapes.

**Architecture impact:** if 7-1 is built right, 7-2 is just "make it data-driven" — read shapes from a definition file (`.dcsc`?) instead of compiled-in. **Small marginal scope** on top of 7-1.

### 7-3. Chipsets and physical package

**Needed:** a `chipset_t` component subtype that wraps an internal sub-`circuit_t`. A chipset has an external pin layout (the physical-package pinout, e.g., 14-pin DIP for 74LS08), distinct from its simulation-package shape (a rectangle with labelled pins). Chipset definitions live in `.dcs` (or `.dcsc`) files; instantiations reference them by name.

**Architecture impact:**
- Domain types: extend `component_t` with a chipset subclass holding a sub-circuit. `component_vt_t::evaluate` for chipsets recurses into the sub-circuit.
- Variable pin counts (precondition): see §6 — `DOMAIN_MAX_PINS_IN = 2` blocks this.
- File format: chipsets become a new top-level `.dcs` shape, possibly with a different extension.
- Parser/serializer: `parse_gate_expr` needs to look up named components in a registry, not just `and`/`or`/`not`.
- Canvas rendering: a chipset draws as a labelled rectangle at the simulation level; expanding into the physical-package view is a separate viewer.

**Largest of the Step-3 items.** Touches every layer.

### 7-4. CLK clock generator primitive

**Needed:** a primitive component that emits a toggling signal each step. Stateful — current primitives are pure functions of inputs.

**Architecture impact:** add a stateful component slot. Either:
- Augment `component_vt_t` with a `step(self)` method called once per simulation step, separate from `evaluate`. CLK's `step` toggles internal state; `evaluate` returns it.
- Or: give `component_t` a small per-instance state buffer the vtable can use.

Either is a contained change, **medium scope**. Sets the pattern for future stateful components (flip-flops, registers).

### 7-5. Real circuit elements (resistors, capacitors, LEDs, …)

**Needed:** non-Boolean signals (real-valued voltage/current). Different from digital simulation; needs SPICE-like solving for real circuits, or simplified linear models.

**Architecture impact:** very large; effectively a parallel domain layer. The user has explicitly deferred this to "Step 3+" and asked only that the current design *accommodate* its future addition.

What that means concretely: ensure `signal_t` isn't baked into too many headers. Today it's everywhere — `pin_count_in/out`, `evaluate(signal_t *in, signal_t *out)`, `waveform.values`. A future analog domain would either:
- introduce a parallel `analog_signal_t` and a parallel set of types, or
- generalize `signal_t` to a tagged-union of digital + analog values.

**Recommendation for now:** keep `signal_t` but document it as "digital signal" in `component.h` so the eventual split is unambiguous. **Design-only at Step 3**, no code change.

### 7-6. Toolbar icons (miniature ANSI/IEEE shapes + text labels)

**Needed:** the side toolbar's AND / OR / NOT buttons render miniature versions of the gate shapes alongside a text label.

**Architecture impact:** trivial once 7-1 lands — `side_toolbar::stb_draw` calls into the shape interpreter at small scale per button. **Small.**

### 7-7. Custom input sequences (e.g., `1010110`)

**Needed:** the input panel accepts a bit string per input, used as the stimulus pattern instead of the current static toggle.

**Architecture impact:** purely UI (input panel widget) plus a small change to `dcs_app::stim_callback` to read the per-input pattern instead of `input_panel_value`. The simulation's stimulus_fn_t signature already supports per-step values. **Small.**

Note: needs the input_box widget that was deferred from Phase 2.3, OR a simpler always-visible text edit per input row.

### 7-8. File-type planning

**Needed:** decide whether to differentiate file types and how to organize shapes / simulations / waveforms.

**Discussion (no decision yet):**

| Possible suffix | Holds |
|---|---|
| `.dcs` | a circuit definition (today's role) — kept |
| `.dcsc` | component definition (chipset internals + shape) |
| `.dcss` | simulation file — circuit reference + input sequences (binds 7-7's stimulus to a circuit) |
| `.dcsw` | waveform export (post-simulation, for separate viewing) |

**Open trade-off:** put shapes in the component file (`.dcsc`) or inline in the same `.dcs`. The user notes the inline option also allows a single gate type to have multiple alternate shapes (rich representations).

This needs explicit discussion before committing to a format. Currently planned: address it as part of Step-3 phase 3.7.

---

## 8. What changes when

A summary mapping of findings → action items, used as input for `step3-plan.md`:

- **Convention nits (§1):** four `tagt_` fixes — Phase 3.0.
- **Safety hardening (§2):** realloc-leak pattern, `auto_layout` allocations, `simulation_run` error propagation, `input_panel_set_circuit` setter — Phase 3.0.
- **App-layer tests (§5):** scaffold `test_circuit_canvas.c` and a few targeted suites — Phase 3.0.
- **Camera consolidation (§4):** revisit when shape DSL lands (3.1) — opportunistic.
- **`DOMAIN_MAX_PINS_IN` refactor (§6):** required precondition for chipsets — Phase 3.3.
- **Forward requirements (§7):** see `step3-plan.md` for phasing.

The codebase is in good shape to start Step 3 without major rework.
