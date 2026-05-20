# Step 2 Supplement — Refinement notes

Observations from manual testing of the Step 2 supplement implementation
(Phases 1–13) that should be addressed in a future iteration. None of
these block the supplement from being considered "done"; they're polish /
architectural improvements deferred to Step 3 or a Step-2.5 follow-up.

**Status legend:** ✅ implemented · 🟡 partially implemented · ⏸ deferred

---

## R-1 — Decouple the external-view display name from the file basename ⏸

**Observation (manual testing of Phase 8 / 10).** When entering BLACKBOX
view, the rectangle is currently labeled with the file basename (e.g.
`half_adder`, `dff_stub`). This is fine as a default, but it conflates
two distinct concepts:

- **File identity** — where the bytes live on disk (`half_adder.dcs`).
- **Component identity / display name** — what this circuit *is* when
  used as a sub-component or shown in symbol form. Two files with
  different filenames could implement the same functional component
  (e.g. `dff_v1.dcs` and `dff_jk.dcs` are both "D flip-flops") and
  should be able to share the same display label / custom shape.

**Current state (Phase 10).** The `# @display_name = ...` annotation
*does* let the file carry an explicit display name that overrides the
basename. The basename is the fallback when the annotation is absent.
So the structural support is already in place — what's missing is
making this a more visible, first-class concept rather than a quiet
override.

**Refinement proposed.**

1. Make `display_name` a primary user-editable property of the circuit
   (e.g. an editable field in the View menu or a sidebar entry), not a
   write-only field that's only visible by editing the `.dcs` text.
2. Conceptually: the external view eventually represents *the component
   form* of this circuit — name + shape + per-pin styles. The internal
   view is the underlying schematic. These two are different
   *presentations* of the same circuit; the user picks which one to
   show via display_mode (today) but the **content** of the external
   view should be authored independently of the file path.
3. Status bar / window title could distinguish: "Editing
   `half_adder.dcs` — display name **HalfAdder**".

**Forward-looking observation from the user.** This may eventually
become a single unified display model: a circuit always has a
component name + symbol; the "internal vs external" toggle becomes
"schematic vs symbol". The file basename is just a default for the
component name when no `# @display_name` is present.

**Scope estimate.** Small UI work (text-input field in the View menu
or a dedicated dialog) + status-bar polish. The serialization /
parsing piece is already done in Phase 10.

**Connected to.** Step 3 Phase 3.6 (chipsets) needs this same
distinction: a chipset reference inside one circuit references another
circuit by its component name, not its file path. The data model
clarification in R-1 prepares the ground for that.

---

## R-2 — Sidebar `[ ] BLACK-BOX` button is small / discoverable ⏸

**Observation (Phase 8).** The sidebar toggle button works but is
visually subtle — easy to miss against the dark toolbar background.

**Refinement proposed.** Consider a stronger contrast for the active
state (already blue) and a small icon (eye / box outline) next to the
text. Low priority — the menu item and `Ctrl+B` are equally functional.

---

## R-3 — Pin stubs (deferred from Phase 11 plan) ⏸

**Observation (Phase 11 plan).** The implementation plan suggested that
pin stubs poke *outside* the gate body so wires terminate ~6 px before
the outline, with a short stub bridging the gap. Phase 11 skipped this
because moving pin positions outward would ripple through saved wire
geometry — a real risk for limited visual payoff.

**Refinement proposed.** Either (a) accept the current "wire terminates
at outline" look as final, or (b) move pin positions outward and add
geometry-migration logic (re-route any wire whose endpoint matches the
old pin position). (b) is moderate effort and worth bundling with the
Step-3 Phase-3.3 variable-pin-count refactor, which already needs to
touch pin-position math.

---

## R-4 — `circuits/dff_stub.dcs` is not a real D flip-flop ⏸

**Observation.** `dff_stub.dcs` (added in `14e3023`) is a placeholder
showing pin names + external-view styling, but its internal logic
(`Q = D AND CLK; Qbar = NOT Q`) is a level-sensitive AND latch, not
an edge-triggered D-FF. The file header documents this explicitly.

**Refinement proposed.** Replace with a real D-FF circuit once Step 3
Phase 3.4 lands the stateful primitive (`clk_edge_t` or similar). No
work needed before then — the file's stub status is clearly flagged.

---

## R-5 — Editing undo / redo (`Ctrl+Z` / `Ctrl+Y`) ⏸

**Observation (clarified during Phase-12 review).** The codebase has no
user-facing undo. The only way to revert an edit is to reload the last
saved file, which discards everything since the save. A misclick that
deletes a gate, a wire dragged the wrong way, or an accidental
disconnect — all permanent until the next save.

(There IS a "rollback" in `wire_geometry_shift_segment` and other
domain functions, but that's *single-call atomicity* — the function
either succeeds or makes no changes. It's a safety net for one
operation, not an editing history. Worth noting because the naming
can suggest user-level undo.)

**Refinement proposed — Command pattern.**

1. New `domain/command.h` interface:
   ```c
   interface tagt_command_vt {
       void (*do_)  (command_t *self);
       void (*undo) (command_t *self);
       void (*destroy)(command_t *self);
   };
   ```
2. New `app/command_stack.{h,c}` holding a bounded stack (e.g. 100
   entries). `command_stack_push(stack, cmd)` records on do;
   `command_stack_undo(stack)` pops + applies the undo;
   `command_stack_redo(stack)` re-applies forward.
3. Every mutation site in `circuit_canvas_widget.c` is wrapped in a
   concrete command:
   - `place_component_cmd_t` (Phase 4 connect_wire path becomes a command)
   - `connect_wire_cmd_t` / `disconnect_input_cmd_t`
   - `delete_node_cmd_t` (records the deleted component for restore)
   - `drag_node_cmd_t` (records the original position)
   - `wire_shift_segment_cmd_t` (records `delta` so undo applies `-delta`)
4. Global shortcuts: `Ctrl+Z` → `command_stack_undo(app->cmd_stack)`,
   `Ctrl+Y` (or `Ctrl+Shift+Z`) → `command_stack_redo(...)`.

**Scope estimate.** Medium. The infrastructure (~200 LOC: interface,
stack, command helpers) is small. The invasive piece is **routing
every existing mutation through a command** instead of directly
mutating circuit/geometry — that's every connect / disconnect / delete /
drag-end / wire-shift site in `circuit_canvas_widget.c`. Each becomes
a couple of lines of "build a command, push it, execute it" instead
of the direct mutation.

**Connected to.**

- Already listed in `docs/step3-plan.md` under the Step-2.7 polish
  carry-over ("undo/redo (Command pattern)"). This refinement note
  makes it visible here too.
- The Phase-12 wire-edit drag would gain proper rollback semantics:
  drag too far, ESC to cancel → undo restores the original geometry
  (currently ESC just leaves the wire wherever the last successful
  shift put it).

**Naming clarity.** When this lands, use "undo / redo" for the
user-facing operation and reserve "atomic" or "all-or-nothing" for
the in-call safety pattern. Calling the latter "rollback" in code
comments is misleading.

---

## R-6 — Vertical input-pin approach (edge case + pin orientation) ⏸

**Observation (Phase-13 manual testing, `b2832c1`).** The Steiner router
now arrives at each off-trunk consumer pin via a per-consumer Z (V drop
+ H stub), so the wire enters the pin **horizontally from the side** —
matching the natural orientation of left-edge input pins on AND/OR/NOT
gates. One residual case still arrives vertically:

- When a consumer sits directly **below or above the producer**
  (`consumer.x == producer.x`), `auto_route_net` emits a single V drop
  from `(px, py)` to `(px, cy)` with **no H stub**. The wire reaches
  the pin from above (or below) — visually it's a vertical line
  terminating at the pin, with the same ambiguity the rest of Phase 13
  used to have before the `b2832c1` fix.

This is rare in practice (most consumers aren't directly below their
producer in a left-to-right schematic flow), so it wasn't worth a
sub-grid nudge in the Phase-13 fix. Recording it here so the case is
visible.

**Refinement option A (small fix).** Treat `cx == px` like a normal
off-trunk consumer: nudge the anchor by ±1 grid step, emit V drop
at the nudged x, then a 1-grid-step H stub into the pin. The wire
acquires a tiny "kink" approach instead of a straight drop. Trivial
change in `auto_route_net` — one branch becomes one extra append.

**Refinement option B (broader; ties into Step 3).** Promote pin
**orientation** to a first-class concept. Today every input pin on a
rectangular gate body is implicitly left-facing (wires expected to
approach horizontally from the left), and every output is right-facing.
A real shape DSL (Step 3 Phase 3.1) would let a component declare
per-pin orientation:

```c
typedef enum {
    PIN_ORIENT_LEFT,    /* pin extends from the left edge; H approach */
    PIN_ORIENT_RIGHT,
    PIN_ORIENT_TOP,     /* pin extends from the top edge; V approach */
    PIN_ORIENT_BOTTOM,
} pin_orient_t;
```

Then `auto_route_net` (or its Step-3 successor) consults pin orientation
when choosing the final approach segment:

- `LEFT` / `RIGHT` orientation: final segment is H (current default).
- `TOP` / `BOTTOM` orientation: final segment is V (and the anchor /
  drop / stub topology mirrors accordingly — H trunk perpendicular to
  the approach axis isn't always the right call).

This unlocks several cases the current router can't handle gracefully:

- D flip-flops with a **clock pin on the bottom** (oriented `BOTTOM`,
  expecting a V approach from below).
- Power / ground pins on the top of an IC.
- Gates rotated 90° in the schematic — every pin's orientation rotates
  with the shape.

**Connected to.** Step 3 Phase 3.1 (shape DSL) is the natural home for
per-pin orientation — it's a property of the shape, not the wire. Phase
3.4 (CLK / stateful primitives) also benefits since clock-input
convention typically has the CLK pin at the bottom or with the clock
mark on a specific edge.

**Scope estimate.** Option A: 1-line change in `auto_route_net`.
Option B: medium — touches `component_t` (per-pin metadata), the
shape DSL, the router's approach-segment logic, the canvas widget's
hit-tests (input/output pin lookup needs to consider orientation).
Bundle with Step 3 Phase 3.1 / 3.3.

---

## R-7 — Network type classification + per-pin orientation case enumeration 🟡

**Observation (user feedback, Phase 13 review).** A more complete way
to think about wire routing is to classify each net by its **producer
pin orientation** and then enumerate the cases for each combination
of (network type) × (consumer pin orientation + relative position).

R-6 sketched the pin-orientation idea but didn't enumerate the cases.
This is the comprehensive model:

### Network types

- **Horizontal network.** Producer pin emerges horizontally (LEFT or
  RIGHT facing on the gate body). The natural trunk is a horizontal
  bus at the producer's y.
- **Vertical network.** Producer pin emerges vertically (TOP or BOTTOM
  facing). The natural trunk transitions from a vertical segment off
  the producer pin to a horizontal bus, then the rest behaves as a
  horizontal network.

### Cases for a horizontal network reaching consumers

**(1-1) Consumer pin is horizontal** (the common case today).
Pull a tap from the horizontal trunk: V drop at an anchor column, H
stub into the pin. **Implemented in `b2832c1`.**

**(1-2) Consumer pin is vertical** — six sub-cases depending on whether
the pin is on the top or bottom of its component AND where the trunk
sits vertically relative to the pin:

| Sub-case | Pin location | Trunk vs pin Y | Topology |
|---|---|---|---|
| 1-2-1 | Top of component | Trunk **above** pin | V drop directly down into the pin |
| 1-2-2 | Top of component | Trunk **below** pin | V up from trunk, H across, V down into pin |
| 1-2-3 | Top of component | Trunk **level** with pin | V up, H across, V down (a small loop) |
| 1-2-4 | Bottom of component | Trunk **above** pin | V down from trunk, H across, V up into pin (inverse of 1-2-2) |
| 1-2-5 | Bottom of component | Trunk **below** pin | V up from trunk into the pin |
| 1-2-6 | Bottom of component | Trunk **level** with pin | V down, H across, V up (inverse of 1-2-3) |

Sub-cases 1-2-3 and 1-2-6 are the trickiest (need a three-bend
detour because the trunk is exactly at the pin's y but on the wrong
side of the component body).

### Vertical network: transition then reuse

For (2) **vertical networks**, the producer's V segment terminates at
a chosen y, then a horizontal trunk extends from that point — and the
rest of the routing reuses the horizontal-network logic above.

### Scope estimate

Once pin orientation is first-class (R-6 Option B), the router needs
a single dispatch on (network type, consumer pin orientation, relative
position) — about a dozen branches, each emitting 1–3 segments.
Implementation roughly the size of the current `auto_route_net` plus
shape-DSL hooks (Step 3 Phase 3.1). Major refactor; bundle with
Step 3.

**Connected to.** R-6 (pin orientation is the prerequisite), R-8
(shared vertical bus changes how multi-consumer cases assemble).

**Status (post-Phase-13).** Partial. The auto-align pass
(`circuit_canvas_widget_auto_align`) lands in `ec00095` /
`7ffdf09` and handles the simplest case — when a consumer's pin
sits within 8 px of its producer's y, snap the component so the
pin aligns exactly (avoiding a tiny dog-leg). The full case
enumeration above (six sub-cases for vertical pins) is still
deferred, gated on R-6 pin-orientation metadata. See
**P13-E** below for what landed.

---

## R-8 — Shared vertical bus per net ✅

**Observation
([`issues/202605201546-issue-there-should-be-a-vertical-bus.png`](../issues/202605201546-issue-there-should-be-a-vertical-bus.png)).**
The current Steiner-Z router gives each off-trunk consumer its own
**anchor column** (V drop at `snap-midpoint(producer.x, consumer.x)`).
For a fan-out to three consumers at distinct y values, you get three
separate V drops at three different x columns — visually busy.

A cleaner topology: **one shared vertical bus** per net.

```
producer ───┬─────────────  (horizontal trunk)
            │
            ┝──── consumer A (above-trunk)
            │
            ┝──── consumer B (level, via short stub)
            │
            ┕──── consumer C (below-trunk)

            ↑
            shared V bus at one carefully-chosen x
```

The V bus extends up from the trunk to cover the highest off-trunk
consumer, and down to cover the lowest. Each off-trunk consumer's
final H stub branches off the V bus at the consumer's own y.

### Where to place the V bus

The user's two constraints:

1. **Must not overlap with other V lines** (from this net OR neighbouring
   nets). Two V buses at the same x would render on top of each other
   and look like one wire.
2. **At least to the LEFT of every consumer's x** (so every H stub
   goes rightward from bus → consumer, consistent direction).

A simple heuristic: `V_bus_x = snap-to-grid(min(consumer.x) -
margin)`, plus a per-net offset to avoid collision with other nets'
V buses.

### Effect on junction-dot derivation

The shared V bus changes the junction topology:

- The point where the H trunk meets the V bus is a degree-3 T-join →
  one junction dot.
- Each H stub attaches to the V bus mid-segment, but in the
  current `wire_geometry_junctions` rule, that only counts as a
  junction if the V bus is **split** at each consumer's y (so the
  H stub's a-endpoint sits on a V-segment endpoint, not mid-segment).
- So the V bus needs to be emitted as **multiple V segments**, broken
  at each off-trunk consumer's y — same trick as the H trunk's
  breaks-at-anchor-x today.

### Implementation impact

`auto_route_net` would gain a per-net V-bus-x decision step, then
emit:

- One or more H trunk segments at producer.y (broken at the V bus's x).
- Multiple V bus segments at V_bus_x (broken at each off-trunk
  consumer's y).
- One H stub per off-trunk consumer from `(V_bus_x, cy)` to
  `(consumer.x, cy)`.

### Cons / open questions

- **Net-collision detection.** "Don't overlap with other nets' V lines"
  requires the router to know about other nets — currently each net
  routes independently. A per-canvas allocator of V-bus columns would
  be needed.
- **Phase-12 drag.** Moving the shared V bus would affect every
  consumer's stub. Currently the bend-drag refuses fan-out shifts;
  with a shared bus the drag could move all stubs together (probably
  desirable).

**Connected to.** R-7 (case enumeration assumes a clean trunk + bus
structure). Step 3 Phase 3.1 (shape DSL) would also need to know
about the bus to do collision-aware placement.

**Scope estimate.** Medium. Touches `auto_route_net` significantly,
adds a per-canvas V-bus-x registry, updates the junction renderer's
expectations. Maybe 200–300 LOC + tests.

**Status (post-Phase-13).** Implemented in `a50861a` + collision
avoidance in `1725001` + draggable as a unit in `71ed6c3`. The
per-canvas V-bus-x registry is NOT yet built — collision avoidance
is per-route (later-routed nets shift around earlier-routed ones),
not globally optimal. Workable in v1; see **P13-D** below.

---

## R-9 — Feedback circuits (output → earlier-stage input) ⏸

**Observation (user, Phase 13 review).** Real digital circuits often
include **feedback** — the output of a later stage drives the input
of an earlier stage. Examples:

- Latches / flip-flops feedback their Q output to gate inputs internally.
- Oscillators / ring oscillators.
- State machines whose next-state logic depends on current state.

The current routing assumes left-to-right signal flow: producer
typically at a smaller x than its consumers, trunk extends to the
right. Feedback wires reverse this — a producer at large x feeds a
consumer at smaller x.

### What works today

`auto_route_net` (after R-6's Steiner-Z fix) already handles
producers with consumers to the **left** — the trunk lo / hi span
both directions (`producer_interior` test case). So a feedback wire
between two components horizontally renders correctly.

### What probably needs revision

- **Routing aesthetics.** A feedback wire that goes from right to
  left would benefit from a more deliberate path — typically routed
  along the top or bottom of the schematic to avoid crossing the
  forward signal flow.
- **Layout.** `auto_layout` currently uses topological depth ordering,
  which doesn't terminate cleanly when feedback creates cycles. Today
  the parser explicitly rejects feedback (`circuit_add_component`
  requires the input wires to already exist). A real DCS supporting
  feedback needs:
  - Cycle-aware layout (Sugiyama-like layered drawing with feedback
    arrows).
  - Simulation handling — `circuit_evaluate` today walks components
    in topological order, which is undefined for cycles.

### What to do

This is **Step 3+ work** — feedback needs a stateful primitive
(D-FF, register) to break the combinational loop, plus a sequential
simulator that handles state evolution step by step. The router
refinement is the smaller piece; the simulator + parser + layout
changes are the bulk.

For Step 2 supplement: feedback is **explicitly out of scope** and
acknowledged here so future work has a placeholder.

**Connected to.** Step 3 Phase 3.4 (stateful primitives like CLK and
flip-flops introduce the need for proper feedback handling).

---

## Post-Phase-13 implementation work

After the implementation plan's final stretch phase landed
(`22ea9c8`), additional refinement work followed in response to
manual GUI testing. Cataloguing it here so the deferred-vs-done
picture is complete and future readers can find the relevant
commits without scanning the whole log.

### P13-A — `circuits/demo1-and3.dcs` test fixture (`6e4a979`)

The implementation plan referenced this file as a manual-visual
gate target, but no commit ever actually created it. Built it now
with a three-AND-gate fan-out topology specifically designed to
exercise Phase 5 junction dots, Phase 8/9 external view, Phase 11
glyphs, and Phase 13 fan-out — all in one circuit.

Verified parses + evaluates as `Y = A AND B AND C` via the CLI.

### P13-B — Steiner Z arrival per consumer (`b2832c1`)

**Issue:** Phase 13 originally landed the V drops *straight above*
each consumer pin, so the wire reached the pin via a vertical line
from above — ambiguous (couldn't tell whether the V actually
connected to the pin or just visually passed through). See
`issues/202605201506-issue-unable-to-see-how-the-input-enters-the-pin-clearly.png`.

**Fix:** `auto_route_net` now emits per-consumer Z arrivals:

- `cy == py`: trunk extends to `cx`; no V, no stub.
- `cx == px`: single V drop straight down; no stub.
- otherwise: V drop at `snap_midpoint(px, cx)` + H stub from
  `(anchor_x, cy)` to `(cx, cy)`.

This is what the single-consumer Z-router already did; multi-
consumer mode now matches.

### P13-C — Refinement entries R-6 / R-7 / R-8 / R-9 recorded (`d1458e8`, `aeb2a2e`)

Documented the four follow-up refinement ideas that came out of
post-Phase-13 review without immediately implementing them: vertical
input-pin approach (R-6), comprehensive routing case enumeration
(R-7), shared V bus per net (R-8), feedback circuits (R-9).

R-8 was then implemented (see below). R-6, R-7 (partial), and R-9
remain deferred.

### P13-D — R-8 shared V bus per net (`a50861a`, `1725001`, `71ed6c3`)

Replaces per-consumer anchor columns from P13-B with **one shared
V bus per net** — matches the classic schematic bus convention.
Landed in three steps:

- **`a50861a`** — algorithm: H trunk to `snap_midpoint(px, min_cx)`,
  then one V bus broken at every unique y (producer.y + each
  consumer.y), then one H stub per consumer. All-collinear-consumers
  case shortcut to H-trunk-only. n=1 falls back to the Z-router.
- **`1725001`** — collision avoidance: shift V_bus_x leftward by
  2*GRID (16 px) when another net's V segment sits within that
  range in an overlapping y interval. Cap at 16 attempts (256 px
  of clearance); routing order is deterministic (inputs first,
  components in storage = topological order).
- **`71ed6c3`** — `wire_geometry_shift_v_bus` operation so the
  whole V bus can be dragged as a unit (Phase 12 interaction).
  Canvas widget's `seg_is_draggable` recognises V-bus segments
  (≥ 2 V segs at the same column) and dispatches to the new shift.

**Constraint still NOT enforced:** the collision check is pairwise
against other nets routed BEFORE this one (route order dependent);
no global registry of "used V-bus-x columns" exists. Two nets
routed simultaneously couldn't collide-avoid each other. Workable
in v1; per-canvas registry can come later if needed.

### P13-E — R-7 partial: auto-align components to trunks (`ec00095`, `7ffdf09`)

The full R-7 (pin-orientation case enumeration) remains deferred,
but one part of its spirit shipped: when a consumer's pin sits
within **8 px (= 1 grid step)** of its producer's y, the component
is snapped vertically so the pin aligns exactly. Result: small
dog-leg V drops + H stubs collapse to single straight H wires on
file open.

- **`ec00095`** — initial implementation. `auto_align_components_to_trunks`
  (now `circuit_canvas_widget_auto_align`) shifts components in
  topological order; multi-input gates align to the pin with
  highest producer fan-out (tie-break: smaller |Δ|); external
  outputs also align to their producers. Called by `_create` and
  `_set_circuit` only — NOT by drag-end (so user drag intent isn't
  fought).
- **`7ffdf09`** — fix for "auto-align ran but had no visible effect on
  file open". Root cause: `load_circuit_from_text` ran auto-align +
  re-routed, then loaded the file's `# @wires` block which
  overwrote the freshly-aligned wires with the file's *stale*
  routing. Fix: `circuit_canvas_widget_auto_align` now returns the
  shift count; the load path skips `load_geometry` when any shift
  happened (preserving fresh routes), and keeps file's wires only
  when no shift occurred (preserving Phase-12 manual bend-drags).

The user-supplied fixture `circuits/testx.dcs` establishes the
8 px threshold: Δ=7 snaps, Δ=17 doesn't. Three of testx.dcs's four
wires straighten on open; the in2 wire (Δ=17) keeps its Z as
presumed deliberate.

### Tally

| Refinement | Pre-Phase-13 | Post-Phase-13 status |
|---|---|---|
| R-6 (pin orientation) | proposed | ⏸ deferred |
| R-7 (case enumeration + auto-align) | proposed | 🟡 auto-align done; case dispatch deferred |
| R-8 (shared V bus) | proposed | ✅ V bus + collision avoidance + draggable |
| R-9 (feedback circuits) | proposed | ⏸ deferred (Step-3+) |

Test count grew from **518** (Phase 13's commit) to **561** (after
the auto-align fix) — +43 tests across the post-Phase-13 work.

---

## Out of scope / explicitly deferred

All 13 phases of the implementation plan landed:

- Phases 1–11: numbered requirements R1–R6 (in
  [`step2-supplement-req.md`](step2-supplement-req.md)).
- Phase 12 — manual bend-point editing (`fcd0ed6`).
- Phase 13 — Steiner-trunk routing (`22ea9c8`), refined into shared
  V bus (`a50861a` and follow-ups).

Refinements R-1 through R-9 above capture everything observed
during testing that should still be addressed but doesn't block
"done". Cross-references to Step 3 phases noted per-entry where
relevant.
