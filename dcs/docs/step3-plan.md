# Step 3 — Plan

This is a **plan**, not a design. It scopes Step 3, orders the work,
and surfaces decisions that need a human before code is touched. A
`step3-design.md` will follow once the planning questions are settled.

**Single working source.** Every unfinished Step 2 requirement is folded
into §1 below (the carry-over audit originally lived in
[`step2-end-unfinished-req.md`](step2-end-unfinished-req.md); that file
is retained as a historical artefact). Step 3 phasing in §3 references
items by their **U-N** identifier so cross-referencing is unambiguous.

---

## 0. Context

Step 2 produced a layered codebase: a generic widget framework
(`src/framework/`), a pure circuit-logic domain (`src/domain/`), and a
DCS-specific application layer (`src/app/`). The CLI uses domain only;
the GUI uses all three. As of the refinement work's last commit
(`aa24041`, 683 tests passing), the GUI feature-parities the prototype
with significant improvements: auto-fit camera, draggable dividers,
orthogonal wires with Steiner trunks, click-to-highlight nets, dual
display modes (schematic / black-box), undo/redo, save-on-close prompt,
tamper-signed binaries, F1 help dialog, version-stamped CLI.

Step 3 has three sources of work:

1. **Carry-over items from Step 2** (40 items — see §1).
2. **Forward-looking requirements 7-1 → 7-8** from
   [`step2-code-review-req.md`](step2-code-review-req.md) — already
   absorbed as U-21..U-28 in §1, with rich detail preserved in §2.
3. **Design questions** that need user answers before
   `step3-design.md` can be written (§4).

---

## 1. Step 2 carry-over (40 items)

Each item is identified by a stable `U-N` number for cross-reference.
Status legend:

- ⏸ Deferred — explicitly named for later
- 🟡 Partial — some progress; remainder gated
- ❓ Status uncertain — needs re-audit during Step 3 planning

### 1.1 Code-quality residue (5 items)

Most code-review findings were silently resolved during the supplement
+ refinement work. These five remain.

| # | Item | Source |
|---|---|---|
| **U-1** | `input_toggle_t` (in `input_panel.h:15`) is missing the `tagt_` prefix — last remaining naming-convention violation from `step2-code-review.md` §1 | code review §1 |
| **U-2** | `circuit_io_serialize` cap is a fixed estimate (`4096 + N·256 + (in+out)·96`); long names + many components could overflow with no growth path | code review §2 medium |
| **U-3** | `waveform_set_value` / `waveform_get_track` have a `t->values == NULL when step_count == 0` fragile path (bounds-check passes, but the code reads awkwardly) | code review §2 medium |
| **U-4** | `circuit_canvas_widget` carries its own camera + scissor logic in parallel with `framework/widgets/canvas_widget.c` — two camera implementations live side-by-side | code review §4 |
| **U-5** | **App-layer test coverage gap.** Canvas widget got `test_circuit_canvas_supplement` (130) + `test_help_dialog` (16) but `dcs_app`, `side_toolbar`, `input_panel`, `divider_widget`, `timing_canvas_widget` still have **zero tests** | code review §5 |

### 1.2 Framework polish — originally tagged "Phase 2.7" (7 items)

| # | Item | Source |
|---|---|---|
| **U-6** | **Partial refresh / dirty-region invalidation.** `widget_t::dirty` field exists, never read; full-frame redraw every tick | refactor-design §A.4 + §8 |
| **U-7** | **Linux platform impl.** `platform_linux.c` is stubs only — no zenity-backed dialogs, file dialogs return 0 | refactor-design §A.2 |
| **U-8** | **Object factories** (`framework_factory_t`, `app_factory_t`) — centralised allocation, testable mocks; direct constructors used today | refactor-design §A.6 + refactor-plan req #13 |
| **U-9** | **State pattern for editor modes.** Today: `canvas_mode_t` enum + `switch` in `ccw_handle_event`. Each mode should be its own class with `enter / handle_event / exit` | refactor-design §B.2 |
| **U-10** | **`input_box` widget** — generic single-line text input; never built. Prerequisite for R-1 part 2 (display-name edit), R-7 custom input sequences, future chipset parameter entry | refactor-design §A.4 |
| **U-11** | **Splitter widget integration.** `framework/widgets/splitter_t` exists but DCS uses a bespoke `divider_widget`; consolidate | refactor-design §A.4 |
| **U-12** | **Focus indicators / Tab traversal** — visual focus ring; framework `focus_manager` exists but `focus_manager_set` is never called | refactor-design §A.5 |

### 1.3 Focus model decision (1 item)

| # | Item | Source |
|---|---|---|
| **U-13** | **Framework focus dispatch is dead code (R-18).** Interim global routing already in place (`a797909`, `cea5d4b`). Two end-states open: (a) retire focus, keep all keys global (~30 lines deletion); (b) wire focus up at click/Tab boundaries (~50–100 lines + Tab traversal). The choice becomes mandatory when the first text-input widget (U-10) lands. | review-of-refactor §A.5 follow-up; refinement R-18 |

### 1.4 Refinement R-* still open (7 items)

| # | Item | Status | Gate |
|---|---|---|---|
| **U-14** | **R-1 part 2** — In-GUI display-name editing | 🟡 Part 1 (visibility) shipped in `74162b3` | U-10 + U-13 |
| **U-15** | **R-3** — Pin stubs that poke outside the gate body so wires terminate ~6 px before the outline | ⏸ | Phase 3.3 (pin geometry refactor) |
| **U-16** | **R-4** — `circuits/dff_stub.dcs` is a fake D-FF; build a real one | ⏸ | Phase 3.4 (sequential primitives) |
| **U-17** | **R-6** — Vertical pin orientation (inputs from top/bottom) | ⏸ | Phase 3.1 (shape DSL) |
| **U-18** | **R-7 full** — Network type classification + per-pin orientation case enumeration | 🟡 Auto-align partial shipped (`ec00095`, `7ffdf09`) | Inherits U-17's gate |
| **U-19** | **R-9** — Feedback circuits (output → earlier-stage input); parser rejects today | ⏸ | Sequential evaluator (Phase 3.4 follow-up) |
| **U-20** | **R-17** — GUI HTTP endpoint so an AI can POST a generated `.dcs` and have the GUI auto-open it | ⏸ User-tagged "Step 3 or 4" | HTTP/IPC layer |

### 1.5 Forward-looking requirements 7-1 → 7-8 (8 items)

These are kept here as cross-references — full detail is preserved in
§2 below.

| # | Item | §2 anchor |
|---|---|---|
| **U-21** | **7-1** ANSI/IEEE shape DSL | §2.1 |
| **U-22** | **7-2** Simulation package (shape system for user components) | §2.2 |
| **U-23** | **7-3** Chipsets + physical package | §2.3 |
| **U-24** | **7-4** CLK clock generator primitive | §2.4 |
| **U-25** | **7-5** Real circuit elements (R, L, C, LED) | §2.5 |
| **U-26** | **7-6** Toolbar icons | §2.6 |
| **U-27** | **7-7** Custom input sequences | §2.7 |
| **U-28** | **7-8** File-type planning | §2.8 |

### 1.6 Implementation-summary findings (4 items)

| # | Item | Source |
|---|---|---|
| **U-29** | **Two-pass renderer fallback** — wires without routed geometry draw as diagonal direct lines (acceptable today, can tighten) | impl-summary Phase 3 design-note |
| **U-30** | **Phase-12 manual bend-drag vs Phase-13 Steiner trunk trade-off** — multi-consumer nets have non-draggable trunk segments | Phase 13 + R-8 commits |
| **U-31** | **Auto-align threshold (8 px) hard-coded** — reasonable today, may want a setting later | Phase 13-E |
| **U-32** | **`DOMAIN_MAX_PINS_IN = 2` per-gate input cap** — blocks multi-input gates (NAND-4, NOR-4) and chipsets | code review §6; refinement summary |

### 1.7 Refinement-summary callouts (2 items)

| # | Item | Source |
|---|---|---|
| **U-33** | **Per-mutation undo command types** — richer descriptions than the generic "edit" label (e.g. "Undid wire connection", "Undid 3-node delete"). Snapshot-based undo from Stage 9b covers correctness; this is a UX polish | refinement-summary §3.4 |
| **U-34** | **Ctrl+C copy selection as image / OLE object** — bitmap/SVG/EMF/OLE feasibility sketch; PrintScreen fallback noted as v1 escape hatch | candidate idea (originally §6.1 of pre-merge plan) |

### 1.8 Pre-release polish discovered during audit (4 items)

| # | Item | Why |
|---|---|---|
| **U-35** | **No CHANGELOG / RELEASES doc.** Version stamp (R-15) is in place but there's no human-readable record of what each version contains | Pre-release polish |
| **U-36** | **No installation / quickstart guide.** README is a design-history trail — no "how do I install + run this" for an external user | Pre-release |
| **U-37** | **Demo / example circuit gallery.** `circuits/` has scattered fixtures but no curated tour | Pre-release; helpful for teachers / new users |
| **U-38** | **No automated CI / GitHub Actions pipeline.** Build + test runs locally only | Pre-release polish |

### 1.9 Late additions (user-reported 2026-05-21)

| # | Item | Source / source location |
|---|---|---|
| **U-39** | **External-IO pin cap = 16 too low.** `DOMAIN_MAX_IO = 16` in `src/domain/component.h:9` caps a circuit at 16 external inputs AND 16 external outputs. User-proposed bump to **~256** for realistic future use (chipset definitions, eventual CPU-scale designs). Distinct from U-32 (the per-gate input cap); both could share a dynamic-array refactor. | `src/domain/component.h:9` |
| **U-40** | **Wire auto-router does NOT avoid component bodies.** Visible on a user-built XOR (constructed from `not_a` / `not_b` / `a_and_not_b` / `not_a_and_b` / OR — no XOR primitive in DCS): auto-routed wires from a NOT gate's output cross diagonally THROUGH the OTHER NOT gate's body. Two possible improvements: (a) component-avoidance in `auto_route_wire` (likely sufficient); (b) crossing-aware placement in `auto_layout` (deeper algorithmic upgrade). Visible UX glitch — worth fixing before any v1 demo. | `issues/202605211856-issue-xor-gate-layout-wireline-cross-not-gate.png`; `src/domain/wire_geometry.c::auto_route_wire`; `src/app/circuit_canvas_widget.c::auto_layout` |

---

## 2. Forward-looking requirements 7-1 → 7-8 (detail)

For each item: one-paragraph statement of need, current architectural
support, what's required, and rough scope (S = a day or so, M = a
week-ish, L = multiple weeks).

### 2.1 ANSI/IEEE shape DSL — U-21 &nbsp; `M`

**Need.** Replace the rectangle-with-uppercase-label rendering of gates
with proper ANSI/IEEE shapes (AND's flat-back-with-curved-front, OR's
concave-back-with-pointed-front, NOT's triangle-with-bubble), described
by a small DSL of lines, arcs, and circles centred on the gate origin.

**Current.** `circuit_canvas_widget.c::draw_node` has a hard-coded
switch over `component_kind_t`. No shape system.

**Required.** A new `shape_t` ADT with parser + interpreter:

- AST: a sequence of primitives (`line(x1,y1,x2,y2)`, `arc(cx,cy,r,a0,a1)`, `circle(cx,cy,r)`).
- Interpreter: takes `igraph_t*`, an origin transform, and a colour; emits draw calls.
- Parser: text → AST. Shapes can live in C source for primitives, or be loaded from `.dcsc` files (see 2.8).
- A `shape_t *(*shape)(void)` accessor on `component_vt_t` (or static const member). `draw_node` dispatches to the interpreter.

### 2.2 Simulation package — U-22 &nbsp; `S` (on top of 2.1)

**Need.** The shape system must also work for user-defined components
(chipsets), not just primitives.

**Required.** Once 2.1's shape DSL exists, this is "make it
data-driven": load shapes from definition files. Marginal extra scope.

### 2.3 Chipsets and physical package — U-23 &nbsp; `L`

**Need.** A `chipset_t` component wrapping an internal sub-circuit
(e.g., 74LS08 = 4 NAND gates with shared power/ground). Two views:

- **Simulation package:** how the chipset appears in a parent circuit
  (a labelled rectangle with named pins).
- **Physical package:** the pinout diagram (DIP-14, etc.) — a separate,
  smaller diagram for documentation.

**Current.** `component_t` is a flat single-output type with
`DOMAIN_MAX_PINS_IN = 2`. No subclass hierarchy beyond the primitives.

**Required.**

1. Variable pin counts (precondition — U-32 — also blocks 2.4).
2. A new `chipset_t` subtype storing an internal `circuit_t*`.
3. Recursive evaluation in `circuit_evaluate`.
4. A component registry (chipset name → factory). Parser looks up
   unknown gate names there before erroring.
5. File format extension — chipsets defined in `.dcs` or `.dcsc`
   files; main circuits reference them by name.
6. Power/ground pins — likely via a special pin tag; semantically
   inert in pure-digital sim, but reserved.
7. Physical-package view — a separate widget or modal; not blocking
   other work.

The largest item by far. Touches every layer.

### 2.4 CLK clock generator primitive — U-24 &nbsp; `M`

**Need.** A primitive component that toggles each simulation step. The
first stateful primitive (current primitives are pure).

**Current.** `component_vt_t::evaluate(self, in, out)` is pure.

**Required.** Either:

- Add a `step(self)` vtable method called once per simulation step,
  separate from `evaluate`. CLK's `step` toggles internal state held
  on the component instance.
- Or: give `component_t` a small per-instance state buffer the vtable
  can use freely.

Either choice generalises nicely to flip-flops, registers, and counters
(Step 4+).

### 2.5 Real circuit elements (R, L, C, LED, crystal) — U-25 &nbsp; `Design-only at Step 3`

**Need.** Eventually support analog signals so resistors, capacitors,
etc. can sit in the same canvas as digital gates.

**Current.** `signal_t` is `uint8_t` representing 0/1/UNDEF. Hard-coded
across `circuit_evaluate`, `simulation_run`, `waveform`, the timing
canvas, and the input panel.

**Required.** Step 4+ build. For Step 3, just two design-time
obligations:

- Document `signal_t` in `component.h` as "digital signal" so the
  eventual split is unambiguous.
- Avoid bolting features into Step 3 that would preclude an analog
  domain (e.g., don't model wire values as `int8_t` if they could
  become floats).

No code change at Step 3 beyond a comment.

### 2.6 Toolbar icons — U-26 &nbsp; `S` (after 2.1)

**Need.** Sidebar buttons (AND, OR, NOT, …, eventually CLK) render
miniature versions of their ANSI/IEEE shapes alongside the text label.

**Required.** Trivial once 2.1 is in. Each button calls the shape
interpreter at small scale into a sub-rect of the button's bounds.

### 2.7 Custom input sequences — U-27 &nbsp; `S`

**Need.** Input panel offers per-input bit strings ("1010110") as
stimulus, instead of (or alongside) the current 0/1 toggle.

**Current.** `input_panel` has a per-input toggle;
`dcs_app::stim_callback` reads it. The simulation's `stimulus_fn_t`
signature already accepts a `(step, input_idx)` pair, so per-step
values are supported by the engine.

**Required.**

- UI: editable bit-string per input row in `input_panel`. Either
  inline single-line text edit (needs U-10's `input_box` widget) or
  a popup dialog.
- Stimulus callback: read the per-input string at index
  `step % strlen(string)`.

Pairs naturally with 2.8 (a "simulation file" format that binds the
bit strings to a circuit so they can be saved).

### 2.8 File-type planning — U-28 &nbsp; `Discussion-level`

**Need.** Decide whether to differentiate file types and where shapes
/ simulations / waveforms live.

**Working proposal** (open for discussion):

| Suffix | Content |
|---|---|
| `.dcs` | circuit definition (today's role; kept) |
| `.dcsc` | component / chipset definition: shape + sub-circuit + pin layout |
| `.dcss` | simulation: references a `.dcs`, adds per-input bit-string stimulus, optional initial state |
| `.dcsw` | waveform export: post-simulation traces for offline viewing / analysis |

**Open question.** Are shapes inline in the component file or separated
into a shape file? Inline is simpler; separation allows a single
component to ship multiple alternate visual representations. Default
proposal: **inline-with-an-`@shape` annotation block** — same pattern
as the layout block from Phase 2.6 — and add separate-file support
later if needed.

This decision drives the parser refactor in 2.1 and 2.3, so it has to
land before those phases.

---

## 3. Proposed phasing

Steps mirror Step 2's structure: small commits per phase, tests stay
green after each. Every U-N from §1 is mapped to a phase.

| Phase | Scope | U-N items folded in |
|---|---|---|
| **3.0 Hardening + framework polish** | Code-quality residue, "Phase 2.7" polish, focus model decision, app-layer tests, pre-release docs, wire-router component avoidance | U-1, U-2, U-3, U-4, U-5, U-6, U-7, U-8, U-9, U-10, U-11, U-12, U-13, U-29, U-31, U-35, U-36, U-37, U-38, U-40 |
| **3.1 Shape DSL + renderer** | ANSI/IEEE shapes for primitives via interpreter; vertical pin orientation falls out | U-17, U-21, U-26 (icons fold in), U-30 |
| **3.2 Toolbar icons** | Folded into 3.1 — listed for traceability | U-26 |
| **3.3 Variable pin counts + IO cap bump** | Dynamic `in_wires` per component; bump `DOMAIN_MAX_IO` from 16 → 256 (or dynamic); pin stubs land alongside | U-15, U-32, U-39 |
| **3.4 CLK primitive + stateful pattern** | First stateful component; establishes `step()` method or per-instance state; unblocks D-FF | U-16, U-19, U-24 |
| **3.5 Custom input sequences** | Bit-string stimulus per input; depends on `input_box` widget from 3.0 | U-27 |
| **3.6 Chipsets** | Largest single block: chipset type + recursive eval + registry + file format + physical-package view; uses 3.1 shapes + 3.3 variable pins | U-18, U-23 |
| **3.7 File-type plan** | Decide and apply `.dcsc` / `.dcss` / `.dcsw` (or stay on `.dcs` with annotation blocks); formalises decisions used by 3.6 | U-28 |
| **3.8 Real circuit elements** | Design-only: document `signal_t` as "digital"; ensure interfaces don't preclude analog | U-25 |
| **3.9 External-workflow integration** | HTTP endpoint, richer undo descriptions, clipboard image | U-20, U-33, U-34 |
| **R-1 part 2 follow-up** | In-GUI display-name editor; lands once U-10 + U-13 are resolved | U-14 |

**Roughly:** 3.0 is the largest bucket — it sweeps up all the
code-quality + framework-polish work that didn't fit Step 2's
feature-driven phases. Better to land 3.0 in small commits across a
few sessions than batch it with feature work. 3.1–3.5 are
independent-ish and can interleave. 3.6 is the largest single block.
3.7 falls out of 3.6's needs. 3.8 is just docs. 3.9 is optional
external-integration polish.

---

## 4. Open questions for the user

These need answers before `step3-design.md` can be written. Suggested
defaults are noted but the user should confirm.

1. **Chipset nesting depth.** Can a chipset contain other chipsets,
   recursively? Suggested default: **yes, unbounded** (with cycle
   detection on load). Implementation cost of "1 level only" vs
   unbounded is similar.

2. **Where shapes live.** Inline annotation block in `.dcs`/`.dcsc`
   (like the Phase 2.6 layout block), or sidecar shape file? Suggested
   default: **inline** (same pattern as `# @layout`). Multi-shape per
   component handled by named alternates: `# @shape:default ...`,
   `# @shape:compact ...`.

3. **CLK frequency configurability.** Just toggle-per-step, or
   configurable period (toggle every N steps)? Suggested default:
   **configurable period via a chipset-style component parameter**,
   since flip-flops and counters will need similar per-instance config
   soon.

4. **Variable pin counts: dynamic alloc vs static bump.** Make
   `in_wires` a heap-allocated `char (*)[NAME_LEN]` with
   `pin_count_in` slots, or just bump `DOMAIN_MAX_PINS_IN` to e.g. 16
   and stay static? Suggested default: **dynamic**, because chipsets
   may have wildly varying pin counts and a static cap of 16 is
   arbitrary. Note: U-39 (`DOMAIN_MAX_IO`) faces the same choice;
   answer it the same way.

5. **Real-circuit signal type.** When analog support arrives, are
   signals plain `float`/`double`, or does each pin carry its own
   physical type (voltage vs current vs binary)? Suggested default
   for **Step 3 design only**: signals are pin-typed, with a small
   enum of physical kinds. Implementation deferred to Step 4.

6. **Custom-input UI choice.** Inline editable bit-string per input
   row (needs `input_box` widget — U-10), or a "Stimulus…" button per
   input opening a popup? Suggested default: **inline editable**,
   because it surfaces stimulus in the main view where users want it.
   Justifies finally building `input_box`.

7. **Toolbar icons + label or just icons?** Suggested default:
   **both**, with the label below or beside the icon — keeps
   discoverability for users not already fluent in ANSI/IEEE shapes.

8. **(NEW) Focus model decision (U-13).** Retire focus (keep all keys
   global, delete `focus_manager`), or wire focus up properly (click
   sets focus, Tab traverses, text-input widget claims focus on open)?
   Suggested default: **wire focus up**, because U-10 (`input_box`)
   becomes a clean implementation once focus exists, and the other
   text-input candidates (custom stimulus, chipset parameter entry,
   display-name editor) all benefit.

9. **(NEW) Wire-router component avoidance (U-40).** Add component-
   avoidance to `auto_route_wire` only, or also overhaul `auto_layout`
   to minimise wire crossings? Suggested default: **start with router
   avoidance only** — covers the visible XOR glitch, lightweight to
   implement. Layout-side optimisation can come later if specific
   topologies still misbehave.

10. **(NEW) `DOMAIN_MAX_IO` target (U-39).** Bump to 256, or make
    dynamic? Suggested default: **dynamic**, same answer as #4 (both
    are the same allocation-pattern decision applied to different
    arrays). If "dynamic" feels premature, **256 as a hardcoded bump**
    is a safe v1 stopgap.

---

## 5. Candidate ideas — discussion only

Recorded so they aren't lost. Not committed for implementation;
evaluate during formal Step 3 planning.

### 5.1 Ctrl+C — copy selection as image / embedded object (recorded 2026-05-21)

**Idea.** When the user selects components and presses `Ctrl+C`, copy
to clipboard as either:

- An **embedded image** (bitmap or vector — PNG / SVG / EMF) that can
  be pasted into Word, PowerPoint, etc. as a picture, OR
- An **embedded object** (OLE / drag-source) that pastes as a richer
  linked artefact.

**Use case.** Letting the user illustrate documents, slides, or
coursework with their DCS schematics without leaving the application
— tighter integration with the "show this to my digital-circuits
teacher" workflow.

**Feasibility sketch.**

| Path | Effort | Notes |
|---|---|---|
| Bitmap → Windows `CF_DIB` | **Low** | Render the selection's bounding box into an off-screen raylib `RenderTexture2D`, export to a CPU-side image, push via `SetClipboardData(CF_DIB, …)`. `iplatform` needs a new `set_clipboard_image(self, w, h, rgba)` method; Linux can stub or use GTK. |
| Vector (SVG / EMF) | Medium | Higher quality; requires a serialise-from-domain path through a custom vector backend. Probably overkill for v1, follow-up if needed. |
| OLE / embedded object | High | DCS would register a clipboard format + handler and run an OLE server. Only justified if users need round-trip "double-click in Word to re-open in DCS". |

**Fallback if not implemented.** Tell users to use **PrintScreen** (or
Windows + Shift + S for the snipping tool) — gives a bitmap that pastes
anywhere.

**When to formally evaluate.** Near U-20 (HTTP endpoint) in the
priority list — both are "tighter external-workflow integration" and
neither is critical for v1. Tracked as U-34 above.

---

Once the open questions are settled, `step3-design.md` will fill in
the concrete types, file formats, and per-phase code locations.
