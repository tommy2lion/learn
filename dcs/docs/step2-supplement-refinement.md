# Step 2 Supplement — Refinement notes

Observations from manual testing of the Step 2 supplement implementation
(Phases 1–12) that should be addressed in a future iteration. None of
these block the supplement from being considered "done"; they're polish /
architectural improvements deferred to Step 3 or a Step-2.5 follow-up.

---

## R-1 — Decouple the external-view display name from the file basename

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

## R-2 — Sidebar `[ ] BLACK-BOX` button is small / discoverable

**Observation (Phase 8).** The sidebar toggle button works but is
visually subtle — easy to miss against the dark toolbar background.

**Refinement proposed.** Consider a stronger contrast for the active
state (already blue) and a small icon (eye / box outline) next to the
text. Low priority — the menu item and `Ctrl+B` are equally functional.

---

## R-3 — Pin stubs (deferred from Phase 11 plan)

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

## R-4 — `circuits/dff_stub.dcs` is not a real D flip-flop

**Observation.** `dff_stub.dcs` (added in `14e3023`) is a placeholder
showing pin names + external-view styling, but its internal logic
(`Q = D AND CLK; Qbar = NOT Q`) is a level-sensitive AND latch, not
an edge-triggered D-FF. The file header documents this explicitly.

**Refinement proposed.** Replace with a real D-FF circuit once Step 3
Phase 3.4 lands the stateful primitive (`clk_edge_t` or similar). No
work needed before then — the file's stub status is clearly flagged.

---

## R-5 — Editing undo / redo (`Ctrl+Z` / `Ctrl+Y`)

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

## R-6 — Vertical input-pin approach (edge case + pin orientation)

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

## R-7 — Network type classification + per-pin orientation case enumeration

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

---

## R-8 — Shared vertical bus per net

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

---

## R-9 — Feedback circuits (output → earlier-stage input)

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

## Out of scope / explicitly deferred

The remaining stretch goal from the implementation plan:

- **Phase 13** — Steiner-trunk routing (fan-outs draw one shared
  horizontal trunk + per-consumer vertical drops, like
  `step2-supplement-demo2.jpg`). Would also unlock dragging the
  shared trunk as a unit, which v1 conservatively refuses (see
  Phase-12 commit message for why).

(Phase 12 — manual bend-point editing — landed in `fcd0ed6`.)

Phase 13 can slide into Step 3 without compromising the supplement's
numbered requirements (R1–R6 in
[`step2-supplement-req.md`](step2-supplement-req.md)).
