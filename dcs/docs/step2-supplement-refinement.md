# Step 2 Supplement — Refinement notes

Observations from manual testing of the Step 2 supplement implementation
(Phases 1–11) that should be addressed in a future iteration. None of
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

## Out of scope / explicitly deferred

These were called out by the implementation plan as stretch goals
(Phases 12 and 13). Listed here so they're visible alongside the
refinements above:

- **Phase 12** — manual bend-point editing (let the user drag a wire
  segment perpendicular to its axis to reshape the route).
- **Phase 13** — Steiner-trunk routing (fan-outs draw one shared
  horizontal trunk + per-consumer vertical drops, like
  `step2-supplement-demo2.jpg`).

Both can slide into Step 3 without compromising the supplement's
numbered requirements (R1–R6 in
[`step2-supplement-req.md`](step2-supplement-req.md)).
