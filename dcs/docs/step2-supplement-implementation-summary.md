# Step 2 Supplement — Implementation Summary

A retrospective on the Step 2 supplement implementation
([`step2-supplement-implementation-plan.md`](step2-supplement-implementation-plan.md))
covering all 13 phases — what was actually built, where the plan held
up exactly, where it bent, and the design decisions that emerged from
running it.

---

## 0. Final outcomes

| Metric | Before supplement | After supplement |
|---|---|---|
| Test count | 213 | **518** (+305) |
| Test suites | 6 | 8 (+ `test_wire_geometry`, `test_circuit_canvas_supplement`) |
| Source files (`src/`) | 27 | 29 (+ `wire_geometry.{h,c}`, `external_view.{h,c}`) |
| Layers touched | framework, domain, app | same — no new layer |
| Plan phases completed | — | **13 / 13** (incl. 2 stretch) |

The supplement's six numbered requirements (R1–R6 in
[`step2-supplement-req.md`](step2-supplement-req.md)) are all met:

- R1 Orthogonal wire routing ✓
- R2 Junction dots at electrical joins ✓
- R3 Click-to-highlight a net ✓
- R4 Persist component + wire geometry in `.dcs` ✓
- R5 Two display modes (internal / external) ✓
- R6 Visual style toward IEC convention ✓

---

## 1. Phase-by-phase summary

### Phase 1 — `wire_geometry` skeleton

**Built.** `src/app/wire_geometry.{h,c}` with the data model
(`wire_segment_t`, `wire_net_geom_t`, `wire_geometry_t`) and lifecycle
+ accessor API (`init`, `release`, `find`, `get_or_create`,
`set_segments`, `net`). 39 unit tests covering the H/V invariant,
atomicity on rejection, capacity growth, double-release safety.

**Difficulties.** None. Pure data structure; smooth.

**Design changes from plan.** None — followed design §1.2/§1.3 verbatim.

---

### Phase 2 — Z-router

**Built.** `auto_route_wire(producer_pin, consumer_pin)` emitting
one H, one V, or three-segment Z with the midpoint x snapped to the
8 px routing grid. 27 new tests.

**Difficulties.** Snap-collapse edge case: when the snapped midpoint
lands exactly on `producer.x` or `consumer.x`, the first or third
segment becomes zero-length. Added a one-grid-step "nudge" to keep
all three segments non-degenerate.

**Design changes.** None significant.

---

### Phase 3 — Renderer uses geometry

**Built.** Canvas widget gained a `wire_geometry_t wires` field;
`seed_geometry_from_circuit` runs at create / set_circuit time; the
`draw_world` wire pass switched from `draw_line(a, b)` per consumer
to iterating segments per net.

**Difficulties.** Two known limitations called out and accepted:
- New wires added in the GUI before Phase 4's mutation hooks landed
  rendered as diagonal direct lines (the defensive fallback).
- Dragged components left stale segments at the old position until
  Phase 4's drag-end reseat.

**Design changes.** Added a **two-pass renderer** instead of the
plan's literal "per-consumer net lookup":
- Pass 1: iterate `cw->wires.nets[]` and draw all routed segments.
- Pass 2: direct-line fallback for any consumer whose net has no
  geometry yet.

This avoids the redraw-the-whole-net-per-consumer waste the plan's
literal text would cause for fan-outs, while preserving the defensive
fallback semantics.

Also added a **stale-producer filter** in Pass 1 — skip nets whose
producer was deleted from the circuit. This handles deletion before
Phase 4 hooked the cleanup.

---

### Phase 4 — Mutation hooks

**Built.** Targeted `reseat_wire_geometry(wire_name)` (used by
`connect_wire` and `disconnect_input`); full reseed via
`seed_geometry_from_circuit` for component / input / output deletion
and drag-end (multi-select can touch many wires; tracking the set was
fiddlier than a full reseed at this scale). `assert_geometry_consistent`
under `#ifndef NDEBUG`. 18 new integration tests in a brand-new
`test_circuit_canvas_supplement.c`.

**Difficulties.** Choosing between targeted-per-net reseat and full
reseed for each mutation site — settled on: targeted for connect /
disconnect (one wire affected), full for delete / drag-end (many wires
possible).

**Design changes.** Added two helpers not in the original plan:
- `wire_geometry_remove_net(name)` — for the disconnect path.
- `circuit_canvas_widget_reseat_wires()` — exposed as a public API
  so tests can drive the reseat without synthesizing every mouse
  event. The plan implicitly assumed integration tests would all
  use synthetic events; the helper made the suite cleaner.

---

### Phase 5 — Junction dots

**Built.** `wire_geometry_junctions(net, out, max)` deriving join
points from segment endpoints. Renderer pass 3 draws black dots at
each junction. `wire_segments_collinear` helper. 22 new tests.

**Difficulties.** **Design contradiction discovered.** Design §3.1's
literal rule said *"junction iff count ≥ 3, OR count == 2 with
non-collinear segments"* — i.e., every polyline corner becomes a
junction. But:
- The demo image (`step2-supplement-demo2.jpg`) clearly shows **no**
  dots at simple bends; dots appear only at T/+ junctions.
- The implementation plan's own fan-out test asserts "*exactly one
  junction at the branch point*" — which only passes under the
  count ≥ 3 rule (corners would add two more).

**Design change.** Adopted the **count ≥ 3 rule** (matches both the
demo and the plan's tests). Documented the divergence in the commit
message.

---

### Phase 6 — Click-to-highlight net

**Built.** `wire_geometry_pick(world, tol, out)` returning the
closest net within tolerance. Canvas widget gained
`highlighted_net[DOMAIN_NAME_LEN]` field and
`set_highlight(wire_name)` setter. Renderer branches in all three
passes (wire pass 1, fallback pass 2, junction pass 3) plus a new
pin-emphasis pass after `draw_node`. 28 new tests (18 pick + 10
highlight state-machine).

**Difficulties.** Synthetic-event integration tests required cam
overrides (target/offset/zoom = 0/0/1) so screen coords map directly
to world coords.

**Design changes.**
- **Color**: design §4.2 said `COLOR_BLUE`, but Q6's user answer
  asked for "bright color". Picked `COLOR_ORANGE` since `COLOR_BLUE`
  was already used for the in-progress wiring preview — using it for
  highlight would conflict.
- Added pin emphasis as a **separate post-pass** rather than weaving
  the orange-larger logic into `draw_node` itself. Cleaner separation
  of concerns: `draw_node` knows nothing about highlights; the
  emphasis pass knows nothing about node-kind details.

---

### Phase 7 — `.dcs` persistence (`# @wires` block)

**Built.** `circuit_io_parse_ex` / `circuit_io_serialize_ex` with an
optional `wire_geometry_t *geom_out` parameter (legacy entry points
became thin wrappers passing NULL). Round-trip + backward-compat +
forward-compat tests. 25 new test_circuit_io cases.

**Difficulties — biggest of the implementation.**
**Layering violation.** `wire_geometry` was placed in `src/app/` per
Phase 1's plan. But `circuit_io.{h,c}` lives in `src/domain/`. The
`_ex` signature takes `wire_geometry_t *`. Having `domain/` depend on
`app/` is a layering violation per the project's architecture
invariants (`docs/step2-refactor-design.md` §A.7, recorded in CLAUDE.md
§3.1).

**Design change — file move.** Asked the user; moved
`wire_geometry.{h,c}` from `src/app/` to `src/domain/`. Justification:
wire_geometry is pure geometric data — no UI, no raylib, no Win32
deps — so it fits the domain layer's "no UI concerns" rule. The
move preserved git history via `git mv`.

**Additional API.** Added two helpers not in the original plan:
- `wire_geometry_append_segments` (exposed the previously-private
  helper as public, needed by the parser).
- `wire_geometry_move(dst, src)` — transfer ownership so the load
  path can install parsed geometry without copying. Cleaner than
  doing a deep copy.

**Compiler warning fix.** GCC complained about possible truncation in
the parser's `snprintf(name, 64, "%s", buf + 4)` (source up to 507
bytes into 64-byte buffer). Fixed with `%.*s` precision to make the
bound explicit; snprintf was already safe.

---

### Phase 8 — Display-mode toggle (internal / external)

**Built.** `display_mode_t { DISPLAY_INTERNAL, DISPLAY_EXTERNAL }` in
editor_state. Canvas widget gained `display_mode` and `display_name`
fields + helpers. New `src/app/external_view.{h,c}` with the default
rectangle renderer. Three UX entry points (View menu, `Ctrl+B`,
sidebar button) all funnelling through one helper. Event short-circuit
when in external mode. 14 new tests including mock-igraph dispatch
verification.

**Difficulties.** Tests needed a way to verify which render path was
taken without opening a real window. Solved by writing a counting
mock `igraph_t` (`mock_igraph_init` in
`test_circuit_canvas_supplement.c`); subsequent phases reuse it.

**Design changes.** Added `IK_B` to the igraph key enum (was missing)
+ mapped in `graph_raylib.c`. Minor framework addition.

---

### Phase 9 — External-view metadata + render hook

**Built.** `external_view_metadata_t` (display_name, per-pin styles,
optional render fn pointer). `external_view_draw` as dispatcher;
`external_view_draw_default` as fallback. `PIN_STYLE_CLOCK`
implemented as proof-of-concept (small triangular wedge inside the
box at clock pins). Canvas widget's `display_name` field migrated
into `external_meta`. 16 new tests.

**Difficulties.** The earlier Phase 8 tests that read `cw->display_name`
broke when the field migrated into the metadata struct. Fixed by
exposing `circuit_canvas_widget_external_meta()` as a public accessor
so tests (and future UI surfaces) can read the metadata.

**Design changes.** None — followed design §6.2 closely. The hook
+ proof-of-concept structure matches the plan's intent ("ship the
seam now, exercise it with PIN_STYLE_CLOCK").

---

### Phase 10 — Metadata persistence

**Built.** `# @display_mode`, `# @display_name`, `# @pin_style`
annotations round-tripping through `_ex` entry points. dcs_app's
load/save paths marshal between app-layer `external_view_metadata_t`
and a new domain-layer plain-data subset. 29 new test_circuit_io
cases.

**Difficulties — layering issue, again.** `external_view_metadata_t`
contains a function pointer referencing `igraph_t` (framework). So
the domain-layer `circuit_io_parse_ex` / `serialize_ex` can't take it
directly without violating the framework → app → domain layering.

**Design change — domain-layer plain-data struct.** Introduced
`circuit_meta_t` in `src/domain/circuit_io.h` as a pure-data
persistable subset (display_mode + display_name + pin styles as
`int`s). dcs_app marshals between this and the app-layer metadata.
The render-fn pointer is **not** persisted (per design §6.4 — the
full shape DSL is Step 3 work).

**Basename quirk** addressed: if the file's display_name happened to
equal the file basename, the serializer used to stamp it into the
file as a `# @display_name = <basename>` line. Then renaming the file
would still show the OLD name. Fixed by comparing in
`pack_meta_for_save` and emitting `# @display_name` only when it
differs from the basename.

---

### Phase 11 — Visual style refresh (IEC glyphs)

**Built.** AND → rectangle + `&`. OR → rectangle + `OR` (initially
`≥1`, see below). NOT → right-pointing triangle + small unfilled
output bubble (replaced the rectangle-with-NOT-text). Output pin
circle skipped for NOT (the bubble IS the visible pin marker).

**Difficulties — font support.** Initially used the IEC `≥1` symbol
for OR. Manual GUI testing revealed raylib's default font renders
`≥` (U+2265) as a tofu `?`, so OR appeared as "?1". Code change
swapped to ASCII "OR" with a code comment documenting how to restore
the IEC glyph when a Unicode-capable font is in use.

**Design changes.** Skipped the pin-stub aesthetic (short stubs poking
outside the gate body) — moving pin positions outward would ripple
through saved wire geometry. Documented in
[`step2-supplement-refinement.md`](step2-supplement-refinement.md) R-3.

---

### Phase 12 — Manual bend-point editing (stretch)

**Built.** `wire_geometry_pick_segment` + `wire_geometry_shift_segment`
in the domain. New `CMODE_WIRE_EDIT` canvas mode with hover cursor
(N-S / E-W), press/drag/release flow, ESC + right-click cancel.
`point_matches_pin` + `seg_is_draggable` helpers (pin-terminal +
degree-2 endpoint check). 27 unit + 18 integration tests.

**Difficulties.**

1. **Fan-out shift semantics.** Initial naïve test expected
   `shift_segment` to succeed on a fan-out V. Algorithm correctly
   refused (shifting the second route's V would corrupt the first
   route's V into a diagonal). Test rewritten to verify the
   conservative refusal + atomic rollback.

2. **"Drag past limit" test math wrong.** First version dragged 50
   px past the producer pin's x and expected rejection. But shifting
   the V *past* the producer pin's x doesn't collapse the H — it just
   reverses the H's direction (a still at producer, b past producer).
   The actual collapse threshold is V.x **equal** to producer.x.
   Fixed.

3. **Cursor + event-handler separation.** `ccw_handle_event` doesn't
   have an `igraph_t *` to call `set_cursor` on. Solved by stashing
   the desired cursor in `cw->we_hover_cursor` during the event
   handler; `ccw_draw` (which has `g`) consumes it.

4. **Early MOUSE_MOVE return.** The early `EV_MOUSE_MOVE` block in
   `ccw_handle_event` returns 1 unconditionally, so the later
   `CMODE_WIRE_EDIT` mode-specific MOVE branch was unreachable.
   Solved by handling the wire-edit drag *inside* the early MOVE
   block (same pattern as `CMODE_DRAGGING`'s node-drag logic).

**Design changes.** The plan didn't anticipate the fan-out
conservative-refusal interaction or the cursor stash. Both were
discovered during implementation; documented in commit and refinement
doc.

---

### Phase 13 — Steiner-trunk router (stretch)

**Built.** `auto_route_net(g, name, producer, consumers[], n)` — one
trunk H segment per unique x interval, one V drop per consumer whose
y differs from producer.y. `auto_route_wire` kept for the
single-consumer fallback. Canvas widget's `seed_geometry_from_circuit`
and `reseat_wire_geometry` rewritten to use the new whole-net API.
19 new unit tests.

**Difficulties.** Anticipated test churn but it didn't materialise —
the existing fan-out tests in `test_circuit_canvas_supplement.c`
(from Phase 4) only assert "seg_count > 0" and "fewer or equal after
disconnect"; both hold under both topologies.

**Design changes.** None of substance. Key insight: **break the trunk
at each consumer.x so each drop's top sits on a trunk-segment
endpoint** (instead of in the middle of a long trunk). This makes
the junction-dot derivation pick up T-joins automatically — no extra
code needed in the junction detector.

**Trade-off (documented in commit).** Phase-12 manual bend-drag
becomes mostly unusable for multi-consumer Steiner routes. Trunk
segments have degree-3 endpoints at every T-join, so the
draggability check rejects them. Single-consumer Z-routes keep their
draggable middle V intact. Documented for future work.

---

## 2. Recurring patterns and lessons

### 2.1 Layering issues surfaced at persistence boundaries

The supplement crossed the domain ↔ app layering boundary twice:

- **Phase 7** — wire_geometry needed by circuit_io serializer.
  Resolution: move wire_geometry into domain (it was pure data).
- **Phase 10** — external_view_metadata_t needed by circuit_io
  serializer. Resolution: introduce a domain-layer plain-data
  *subset* (`circuit_meta_t`), marshal in dcs_app.

Both resolutions kept the layering invariants intact. The repeated
pattern: **the persistence layer is in domain; anything it needs to
serialize must be expressible as domain-friendly plain data**.

### 2.2 Plan vs reality: count rules and tests

Two phases had tension between the design text and the implementation
plan's tests:

- **Phase 5 junction rule** — design said count ≥ 2 with non-collinear;
  plan's test demanded count ≥ 3 only. Demo image confirmed count ≥ 3
  was right. Adopted plan's test, rewrote design's wording in the
  commit.
- **Phase 13 fan-out test ambiguity** — old per-consumer test assumed
  6 segments for a 2-consumer fan-out (= 3+3 Z-routes); under Steiner
  the count drops to 4. Since the test was on `auto_route_wire`
  directly (not the canvas), and `auto_route_wire` is unchanged,
  the old test still passes.

Takeaway: when plan and design disagree, **trust the demo image and
the test assertions** — those are the concrete contract; the design
prose is just a sketch.

### 2.3 The atomic-rollback pattern (single-call all-or-nothing)

Used in `wire_geometry_set_segments`, `wire_geometry_append_segments`,
`wire_geometry_shift_segment`. Shape:

1. **Snapshot** — `memcpy` the affected buffer into a `malloc`'d backup.
2. **Mutate freely** — apply the proposed change in place.
3. **Validate** — walk every potentially-affected slot; check invariants.
4. **Commit or restore** — on validation failure, `memcpy` the backup
   back over the live buffer.

Works because the segment data is flat POD with no nested allocations.

**Naming clarity** (added to refinement doc R-5 after a user question):
this is *single-call* atomicity, NOT user-level editing undo. Calling
it "rollback" in comments can mislead readers expecting Ctrl+Z. Use
"atomic" or "all-or-nothing"; reserve "undo / redo" for the
editing-history Command pattern.

### 2.4 Mock-igraph for integration testing

`test_circuit_canvas_supplement.c` has a fully-stubbed `igraph_t`
implementation (counts each primitive call, records `draw_text`
strings). Introduced in Phase 8 for render-dispatch testing;
reused in Phase 9 (clock-pin extra-line counts) and beyond.

Pattern is reusable: any future test that needs to verify *which*
render path was taken can drop this mock in and inspect the counters.

### 2.5 Manual visual gates

Most user-visible phases have a manual visual gate at the end. Claude
can't see the screen, so phases were committed as "tentative" until
the user ran `dcs_gui.exe` and reported back. Multiple bugs found
this way:

- Phase 11: OR gate showed `?1` (font issue) → fixed to ASCII "OR".
- Phase 6: highlight color choice confirmed (orange not blue).
- Phase 8: three UX entry points all visible + working.

Important: **never claim a phase complete on test-suite green alone**
if it has a visual gate.

### 2.6 The user owns the pace

The supplement landed across ~25 commits over multiple sessions. The
user paced phase-by-phase ("Please follow ... and start with phase N")
and ran manual tests in between. Several bonus commits emerged from
this rhythm — manual-test artifacts (saved `.dcs` files with new
annotations), the `dff_stub.dcs` sample, README reorganization, the
zoom shortcut, the comprehensive CLAUDE.md.

---

## 3. Cross-cutting design decisions

| Decision | Phase | Rationale |
|---|---|---|
| `wire_geometry.{h,c}` in `domain/` (not `app/`) | 7 | Pure geometric data; consumed by domain's circuit_io. |
| `circuit_meta_t` as plain-int domain struct | 10 | Avoids domain → app dep for external_view_metadata_t. |
| `_ex` entry points; legacy as thin wrappers | 7, 10 | Lets CLI keep using the small legacy API; domain tests stay green. |
| Both `auto_route_wire` and `auto_route_net` kept | 13 | Single-consumer cases use the simpler Z-router; multi-consumer use Steiner. |
| `assert_geometry_consistent` is debug-only | 4 | Catches drift in dev / test builds; zero release-build overhead. |
| `wire_geometry_pick` + `pick_segment` as separate APIs | 6, 12 | pick returns net name (for highlight); pick_segment returns indices (for shift). Different consumers want different shape; either alone is enough. |
| Three UX entry points → one helper | 8 | Future fourth surface (e.g. status-bar widget) plugs in by calling the helper; no fan-out of behaviour. |
| Forward-compatible parsing (silent ignore unknowns) | 7, 10 | New `.dcs` files round-trip cleanly on older parsers (the older parser just drops unknown annotations). |
| Backward-compatible serialization | 7, 10 | `circuit_io_serialize(c)` byte-identical to `_ex(c, NULL, NULL)` — verified by an explicit test. |

---

## 4. What was deferred

Tracked in [`step2-supplement-refinement.md`](step2-supplement-refinement.md):

- **R-1** — Decouple external-view display_name from file basename
  (make it a first-class editable property; eventually become
  a unified "schematic vs symbol" view model).
- **R-2** — Make the sidebar BLACK-BOX button more visually prominent.
- **R-3** — Pin stubs (deferred from Phase 11; bundle with Step-3
  variable-pin-count refactor).
- **R-4** — Replace `dff_stub.dcs` with a real D-FF once Step 3
  Phase 3.4 lands the stateful primitive.
- **R-5** — Editing undo / redo via the Command pattern
  (`Ctrl+Z` / `Ctrl+Y`). Touches every mutation site in
  `circuit_canvas_widget.c`.
- **Phase-13 trade-off** — Phase-12 bend-drag is mostly disabled for
  multi-consumer Steiner routes. Resolving would mean either
  recognising trunks as groups (move all trunk segments together) or
  allowing partial topology relaxation during drag.

None of these block the supplement from being "done"; all can land
in a Step-2.5 polish pass or Step 3.

---

## 5. What the supplement unlocks for Step 3

- **Shape DSL (Phase 3.1)** — the `external_view_metadata_t` render
  hook is in place; Step 3 fills it in. The dispatcher pattern
  (`external_view_draw` → custom `render` fn else default) means
  Step 3 doesn't need to retrofit anything.
- **Variable pin counts (Phase 3.3)** — wire_geometry is independent
  of `DOMAIN_MAX_PINS_IN`; only the canvas widget's pin-position math
  cares about pin counts. wire_geometry stays untouched when
  pin counts grow.
- **Chipsets (Phase 3.6)** — the external/internal view duality
  introduced in Phase 8 is exactly the structure chipsets need (a
  chipset reference renders in external view; opening it shows the
  internal schematic). The "what's the right display_name for this
  circuit when used as a sub-component" question raised in R-1 is
  the chipset-naming question in disguise.
- **CLK primitive (Phase 3.4)** — the `PIN_STYLE_CLOCK` proof-of-
  concept shows the architectural seam works. A real D-FF declares
  `meta.input_styles[clk_idx] = PIN_STYLE_CLOCK` and gets the clock
  wedge for free.

---

## 6. Commit history (supplement-related)

A condensed audit trail. Phase commits in bold; supporting commits
indented.

- **`dfdeb70`** — Phase 1: wire_geometry skeleton
- **`beea043`** — Phase 2: Z-router
- **`95b7148`** — Phase 3: renderer uses geometry
- **`0aa3df4`** — Phase 4: mutation hooks
- **`a10f388`** — Phase 5: junction dots
- **`9f3d2fb`** — Phase 6: click-to-highlight
  - `1eb80c6` — manual-test artifact (round-tripped circuits)
- **`aee84e4`** — Phase 7: `# @wires` persistence
- **`220d768`** — Phase 8: display-mode toggle
- **`738ca96`** — Phase 9: render hook + clock pin
  - `14e3023` — dff_stub.dcs sample
- **`89cfac0`** — Phase 10: metadata persistence
  - `01048c6` — manual-test artifacts (dff_stub_copy.dcs etc.)
- **`2c3fa1b`** — Phase 11: visual style refresh
  - `7d0ccbf` — OR gate ASCII fallback (raylib font issue)
  - `c9776b0` — refinement doc R-1 through R-4
  - `ce32c45` — or_gate.dcs sample
- `ce568ae` — CLAUDE.md comprehensive coding standards
- **`fcd0ed6`** — Phase 12: manual bend-point editing (stretch)
  - `713a033` — refinement doc R-5 (Ctrl+Z undo)
  - `65cf2ae` — Ctrl+= / Ctrl+- zoom shortcuts
- **`22ea9c8`** — Phase 13: Steiner-trunk fan-out (stretch)

22 commits total: 13 phase commits, 9 supporting commits (manual-test
artifacts, samples, refinements, ergonomic additions).

---

## 7. If I were doing this again

- **Decide the layer for new domain-friendly modules up-front.** The
  Phase-7 move of wire_geometry from app/ → domain/ cost a small
  refactor that could have been avoided by placing it correctly in
  Phase 1. Lesson for future modules: ask "does the persistence
  layer need to know about this?" — if yes, it goes in domain.
- **Resolve plan / design contradictions before implementing.** The
  Phase-5 junction rule confusion ate ~30 min of investigation that
  could have been a one-line clarifying note in the plan ("count >=
  3 only — corners don't get dots").
- **Mock igraph from Phase 8 onward — could it have been earlier?**
  Yes, but only marginally. Phase 4 introduced
  `circuit_canvas_widget_reseat_wires` as a public helper, which let
  most integration tests skip render dispatch entirely. The mock
  igraph only became essential when verifying *which render path was
  taken* (Phase 8+).
- **The atomic-rollback pattern is the safety net I should have
  written sooner.** It's how `set_segments` is correct under invalid
  input; how `shift_segment` is correct under boundary drags; how
  `append_segments` rolls back on H/V validation failure. Reuse it
  in any future domain mutation that can fail mid-operation.
