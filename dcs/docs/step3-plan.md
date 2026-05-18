# Step 3 — Plan

This is a **plan**, not a design. It scopes Step 3, orders the work, and surfaces decisions that need a human before code is touched. A `step3-design.md` will follow once the planning questions are settled.

The work has three sources:

1. Items deferred from Step 2 (called out in commits as "Phase 2.7 polish" or "design-only").
2. Hardening items surfaced by the code review in `step2-code-review.md`.
3. The eight forward-looking requirements (7-1 → 7-8) from `step2-code-review-req.md`.

---

## 0. Context

Step 2 produced a layered codebase: a generic widget framework (`src/framework/`), a pure circuit-logic domain (`src/domain/`), and a DCS-specific application layer (`src/app/`). The CLI uses domain only; the GUI uses all three. As of commit `9f04b0a`, 213 tests pass; layer-independence invariants hold; the GUI feature-parities the prototype with several improvements (auto-fit camera, draggable dividers, layout block in `.dcs`).

Step 3 introduces visual polish (ANSI/IEEE gate shapes), domain extensions (chipsets, CLK, eventual real circuit elements), and UX improvements (custom input sequences, toolbar icons). It also ties up Step 2's loose ends.

---

## 1. Carry-over from Step 2

These were called out in Step 2 commits as deferred polish, not bugs:

| Item | Origin | Status |
|---|---|---|
| True State pattern for editor modes | Phase 2.5 deferred — currently enum + switch in `circuit_canvas_widget.c::ccw_handle_event` | Architectural cleanup; small scope |
| Object factories (`framework_factory_t`, `app_factory_t`) | Phase 2.3/2.5 deferred — currently direct constructors | Worth the indirection only when we have multiple impls or need test mocks; revisit when we do |
| `input_box` widget | Phase 2.3 part 2 deferred | Needed for 7-7 (custom input sequences); promote to Phase 3.0 or 3.5 |
| Partial refresh / dirty rectangles | Phase 2.7 polish | No clear performance need yet; keep deferred |
| Focus indicators | Phase 2.7 polish | UX nicety; keep deferred |
| Linux platform impl (zenity dialogs) | Phase 2.7 polish | Out of scope for current Windows-first work; keep deferred |
| Undo/redo via Command pattern | Phase 2.7 polish | Substantial UX feature; defer to Step 4 |
| Splitter integration in DCS | Phase 2.5 chose bespoke `divider_widget` | Works; revisit only if widget-tree integration becomes useful |

**Plan:** the State-pattern refactor and the `input_box` widget are useful enablers for Step 3. The factories and partial refresh stay deferred. Linux/undo are outside Step 3's scope.

---

## 2. Code-review hardening

Imported from `step2-code-review.md` §2 (safety findings) and §1 (convention violations). Bundled into one early phase so we start Step 3 from a clean slate.

| Topic | Where | Approach |
|---|---|---|
| Realloc-leak pattern | `panel.c`, `circuit.c`, anywhere doing `x = realloc(x, …)` | Save into a temporary, NULL-check, then commit |
| Unchecked malloc in `auto_layout` | `circuit_canvas_widget.c:87,98,99` | NULL-check; on OOM, place every node at origin |
| `waveform_add_track` errors swallowed | `simulation.c:21-22` | Propagate error to caller's status callback |
| `input_panel.circuit` field cast | `dcs_app.c:55-56,68-69` | Add `input_panel_set_circuit()` setter |
| Bit-packing in `wire_at` | `circuit_canvas_widget.c:409` | Store `pin` in a separate field on `node_ref_t` |
| `next_name` unbounded loop | `circuit_canvas_widget.c:299-303` | Cap at 10 000 retries; report failure |
| `menu_add_item` silent drop | `menu.c` | Return `int` (added index, or -1) |
| `circuit_io_serialize` static cap | `circuit_io.c` | Track bytes vs cap; grow buffer or fail explicitly |
| Convention nits | 4 sites in `cli/main.c` and `src/app/` | Add `tagt_` prefixes + `_t` aliases |
| App-layer tests | New `test/test_circuit_canvas.c` etc. | ~30-50 tests covering hit-test, modes, auto_layout, smart-rename |

All of the above is one small commit per topic, no design ambiguity.

---

## 3. Forward requirements (7-1 → 7-8)

For each item: a one-paragraph statement of need, current architectural support, what's required, and a rough scope (S = a day or so, M = a week-ish, L = multiple weeks).

### 7-1. ANSI/IEEE shape DSL  &nbsp; `M`

**Need.** Replace the rectangle-with-uppercase-label rendering of gates with proper ANSI/IEEE shapes (e.g., AND's flat-back-with-curved-front, OR's concave-back-with-pointed-front, NOT's triangle-with-bubble), described by a small DSL of lines, arcs, and circles centred on the gate origin.

**Current.** `circuit_canvas_widget.c::draw_node` has a hard-coded switch over `component_kind_t`. No shape system.

**Required.** A new `shape_t` ADT with parser + interpreter:
- AST: a sequence of primitives (`line(x1,y1,x2,y2)`, `arc(cx,cy,r,a0,a1)`, `circle(cx,cy,r)`).
- Interpreter: takes `igraph_t*`, an origin transform, and a colour; emits draw calls.
- Parser: text → AST. Shapes can live in C source for primitives, or be loaded from `.dcsc` files (see 7-8).

A `shape_t *(*shape)(void)` accessor on `component_vt_t` (or static const member). `draw_node` dispatches to the interpreter.

### 7-2. Simulation package (extend 7-1 to user components) &nbsp; `S` (on top of 7-1)

**Need.** The shape system must also work for user-defined components (chipsets), not just primitives.

**Current.** N/A.

**Required.** Once 7-1's shape DSL exists, this is just "make it data-driven": load shapes from definition files. Marginal extra scope above 7-1.

### 7-3. Chipsets and physical package &nbsp; `L`

**Need.** A `chipset_t` component wrapping an internal sub-circuit (e.g., 74LS08 = 4 NAND gates with shared power/ground). Two views:

- **Simulation package:** how the chipset appears in a parent circuit (a labelled rectangle with named pins).
- **Physical package:** the pinout diagram (DIP-14, etc.) — a separate, smaller diagram for documentation.

**Current.** `component_t` is a flat single-output type with `DOMAIN_MAX_PINS_IN = 2`. No subclass hierarchy beyond the primitives.

**Required.**
1. Variable pin counts (precondition — also blocks 7-4).
2. A new `chipset_t` subtype storing an internal `circuit_t*`.
3. Recursive evaluation in `circuit_evaluate`.
4. A component registry (chipset name → factory). The parser looks up unknown gate names there before erroring.
5. File format extension — chipsets defined in `.dcs` or `.dcsc` files; main circuits reference them by name.
6. Power/ground pins — likely via a special pin tag; semantically inert in pure-digital sim, but reserved.
7. Physical-package view — a separate widget or modal; not blocking other work.

The largest item by far. Touches every layer.

### 7-4. CLK clock generator primitive &nbsp; `M`

**Need.** A primitive component that toggles each simulation step. The first stateful primitive (current primitives are pure).

**Current.** `component_vt_t::evaluate(self, in, out)` is pure: same inputs always produce the same outputs.

**Required.** Either:
- Add a `step(self)` vtable method called once per simulation step, separate from `evaluate`. CLK's `step` toggles internal state held on the component instance.
- Or: give `component_t` a small per-instance state buffer the vtable can use freely.

Either choice generalizes nicely to flip-flops, registers, and counters (Step 4+).

### 7-5. Real circuit elements (R, L, C, LED, crystal) &nbsp; `Design only at Step 3`

**Need.** Eventually support analog signals so resistors, capacitors, etc. can sit in the same canvas as digital gates.

**Current.** `signal_t` is `uint8_t` representing 0/1/UNDEF. Hard-coded across `circuit_evaluate`, `simulation_run`, `waveform`, the timing canvas, and the input panel.

**Required.** This is a Step 4+ build. For Step 3, just two design-time obligations:

- Document `signal_t` in `component.h` as "digital signal" so the eventual split is unambiguous.
- Avoid bolting features into Step 3 that would preclude an analog domain (e.g., don't model wire values as `int8_t` if they could become floats).

No code change at Step 3 beyond a comment.

### 7-6. Toolbar icons &nbsp; `S` (after 7-1)

**Need.** Sidebar buttons (AND, OR, NOT, …, eventually CLK) render miniature versions of their ANSI/IEEE shapes alongside the text label.

**Current.** Buttons are text-only.

**Required.** Trivial once 7-1 is in. Each button calls the shape interpreter at small scale into a sub-rect of the button's bounds. A few extra lines in `side_toolbar::stb_draw`.

### 7-7. Custom input sequences &nbsp; `S`

**Need.** The input panel offers per-input bit strings ("1010110") to use as stimulus, instead of (or alongside) the current 0/1 toggle.

**Current.** `input_panel` has a per-input toggle; `dcs_app::stim_callback` reads it. The simulation's `stimulus_fn_t` signature already accepts a `(step, input_idx)` pair, so per-step values are supported by the engine.

**Required.**
- UI: add an editable bit-string per input row in `input_panel`. Either inline single-line text edit (needs the deferred `input_box` widget) or a popup dialog.
- Stimulus callback: read the per-input string at index `step % strlen(string)`.

Pairs naturally with 7-8 (a "simulation file" format that binds the bit strings to a circuit so they can be saved).

### 7-8. File type planning &nbsp; `Discussion-level`

**Need.** Decide whether to differentiate file types and where shapes / simulations / waveforms live.

**Working proposal** (open for discussion):

| Suffix | Content |
|---|---|
| `.dcs` | circuit definition (today's role; kept) |
| `.dcsc` | component / chipset definition: shape + sub-circuit + pin layout |
| `.dcss` | simulation: references a `.dcs`, adds per-input bit-string stimulus, optional initial state |
| `.dcsw` | waveform export: post-simulation traces for offline viewing / analysis |

**Open question:** Are shapes inline in the component file, or separated into a third "shape file"? Inline is simpler; separation allows a single component to ship multiple alternate visual representations (the user explicitly noted this). My default would be **inline-with-an-`@shape` annotation block** — same pattern as the layout block from Phase 2.6 — and add separate-file support later if needed.

This decision drives the parser refactor in 7-1 and 7-3, so it has to land before those phases.

---

## 4. Proposed phasing

Steps mirror Step 2's structure: small commits per phase, tests stay green after each.

| Phase | Scope | Depends on |
|---|---|---|
| **3.0** | Code-review hardening: realloc / malloc-checks, `input_panel_set_circuit`, app-layer tests, convention nits, error propagation in simulation | — |
| **3.1** | Shape DSL + interpreter; ANSI/IEEE shapes for AND/OR/NOT; `circuit_canvas_widget` renders via interpreter | 3.0 |
| **3.2** | Toolbar icons using miniature shapes (7-6) | 3.1 |
| **3.3** | Variable pin count refactor: dynamic `in_wires` per component; touches domain types, parser, canvas, simulation | 3.0 |
| **3.4** | CLK primitive (7-4); establishes the stateful-component pattern (per-instance state + `step()` method) | 3.3 |
| **3.5** | Custom input sequences in `input_panel` (7-7); requires `input_box` widget if going inline | 3.0 (input_box) |
| **3.6** | Chipsets (7-3): chipset definitions, instantiation, recursive evaluation, simulation-package shape; **physical-package view in a sub-phase 3.6b** | 3.3, 3.1 |
| **3.7** | File-type plan (7-8); decide and apply `.dcsc`/`.dcss`/`.dcsw` (or stay on `.dcs` with annotation blocks) | 3.6 (drives format) |
| **3.8** | Real-circuit elements (7-5) — design-only: document `signal_t` semantics, ensure interfaces don't preclude analog | — |

**Roughly:** 3.0 is housekeeping and can land in days. 3.1–3.5 are independent-ish and can interleave. 3.6 is the largest single block. 3.7 falls out of 3.6's needs. 3.8 is just docs.

---

## 5. Open questions for the user

These need answers before `step3-design.md` can be written. Suggested defaults are noted but the user should confirm.

1. **Chipset nesting depth.** Can a chipset contain other chipsets, recursively? Suggested default: **yes, unbounded** (with cycle detection on load). The implementation cost of "1 level only" vs unbounded is similar.

2. **Where shapes live.** Inline annotation block in `.dcs`/`.dcsc` (like the Phase 2.6 layout block), or sidecar shape file? Suggested default: **inline** (same pattern as `# @layout`). Multi-shape per component handled by named alternates: `# @shape:default ...`, `# @shape:compact ...`.

3. **CLK frequency configurability.** Just toggle-per-step, or configurable period (toggle every N steps)? Suggested default: **configurable period via a chipset-style component parameter**, since flip-flops and counters will need similar per-instance config soon.

4. **Variable pin counts: dynamic alloc vs static bump.** Make `in_wires` a heap-allocated `char (*)[NAME_LEN]` with `pin_count_in` slots, or just bump `DOMAIN_MAX_PINS_IN` to e.g. 16 and stay static? Suggested default: **dynamic**, because chipsets may have wildly varying pin counts and a static cap of 16 is arbitrary.

5. **Real-circuit signal type.** When analog support arrives, are signals plain `float`/`double`, or does each pin carry its own physical type (voltage vs current vs binary)? Suggested default for **Step 3 design only**: signals are pin-typed, with a small enum of physical kinds. Implementation deferred to Step 4.

6. **Custom-input UI choice.** Inline editable bit-string per input row (needs `input_box` widget), or a "Stimulus…" button per input opening a popup? Suggested default: **inline editable**, because it surfaces stimulus in the main view where users want it. Justifies finally building `input_box`.

7. **Toolbar icons + label or just icons?** Suggested default: **both**, with the label below or beside the icon — keeps discoverability for users not already fluent in ANSI/IEEE shapes.

---

Once the open questions are settled, `step3-design.md` will fill in the concrete types, file formats, and per-phase code locations.
