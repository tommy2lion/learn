# Step 2 — Supplementary Requirements

A set of basic, user-visible requirements layered on top of the current Step 2 codebase, to be implemented *before* Step 3 begins.

Visual style reference: [`step2-supplement-demo1.jpg`](step2-supplement-demo1.jpg) and [`step2-supplement-demo2.jpg`](step2-supplement-demo2.jpg).

---

## Context

The current Step 2 GUI renders wires as direct point-to-point segments (diagonal when the two pins are not co-linear) and treats each wire as an independent edge from one pin to one pin. The two demo images show the visual style we are targeting: rectangular IEC-style gates, **orthogonal** wire routing (horizontal + vertical only), and **black junction dots** wherever two wire segments are electrically joined. This document collects the requirements that must be met before moving on to Step 3.

These requirements layer on top of Step 2 — they do not alter the framework / domain / app split, the layered architecture, or the `.dcs` text-based file format. They do extend the data model (nets, wire geometry, view mode) and the renderer.

### Why these requirements are raised now, at the end of Step 2

These look like incremental UX touch-ups but they are not — they are **architecture-level concerns**, or at minimum two-tier (domain + app) concerns. Specifically:

- Promoting wires from "an implicit derived-from-component-input-names edge" to a renderable, persistable, hit-testable object touches the domain ↔ app boundary, the file format, the renderer, and the editor's interaction model all at once.
- The "two display modes" requirement introduces the *concept* of a circuit having both an externalised (black-box) form and an internalised (schematic) form — that is the same architectural seam Step 3 will widen to make true hierarchical chipsets work.

Addressing them now — while the Step-2 codebase is still freshly understood, well-tested, and not yet padded with new chipset / shape-DSL machinery — saves a lot of rework. Doing them after Step 3 would force a second invasive pass through every wire-related code path. Doing them inside Step 3 would conflate two large refactors (hierarchy + geometry) into one. So they ship as a supplement to Step 2.

---

## R1 — Orthogonal-only wire routing

Wires between component pins **must** be rendered using only horizontal and vertical line segments. A connection whose two endpoints are not co-linear shall be realized as a combination of horizontal and vertical segments (e.g. an L-shape, a Z-shape, or any longer poly-line whose every segment is purely H or purely V).

- No diagonal segments are permitted anywhere in the schematic.
- The path may have any number of bends; the minimum that visually clears other components is preferred.
- The user-experience target is the routing shown in [`step2-supplement-demo2.jpg`](step2-supplement-demo2.jpg).
- The route is **computed once at wire-creation time and persisted**. The user can **drag any bend point** to reshape the wire; the new shape is then saved with the file. The application does not re-route on every redraw.

---

## R2 — Junction dots at electrical joins

Where a vertical wire segment and a horizontal wire segment share the same point **and the two segments are electrically connected** (i.e. they belong to the same net), the renderer shall draw a small filled black dot at that point.

- A wire crossing another wire without electrical connection (two unrelated nets that happen to cross visually) **must not** be marked with a dot — the segments simply pass over each other.
- Junction dots are computed from the wire-network topology, not from pixel-level intersection.
- This is the convention shown throughout [`step2-supplement-demo2.jpg`](step2-supplement-demo2.jpg).
- **Net membership is computed on demand** from the existing wire-name pointers in `component.in_wires[p]` and the producer's output-wire name. No new `net_t` first-class domain object is introduced — keeping the simulation domain pure.

---

## R3 — Click-to-highlight a net

When the user left-clicks anywhere on a wire segment, the application shall:

1. Identify the **net** that segment belongs to — i.e. the maximal connected set of wires/pins that share an electrical signal.
2. Highlight every segment, junction dot, and connected pin of that net with **both a thickened stroke and a distinct bright color** (e.g. orange or yellow — clearly different from the in-progress wiring color). Pin terminals on the highlighted net shall also be emphasized (enlarged and recoloured).
3. Clear the highlight on the next click outside the net (or on a different net, in which case that net is highlighted instead).

The net concept itself is **not** promoted to a first-class domain object (see R2 — net membership is computed on demand). The renderer simply groups segments by producer wire-name when drawing highlights.

---

## R4 — Persist component and wire geometry in `.dcs` files

When a circuit is saved, the `.dcs` file shall record:

1. **Component positions** — already partially supported today via the `# @layout` annotation block; this remains the mechanism.
2. **Wire connection geometry** — i.e. the actual routed poly-line path of each wire, not only the `(source-pin, destination-pin)` endpoints.

On load, the saved geometry shall be restored verbatim, so a user-tweaked layout survives a save/reload round-trip without re-running auto-layout or re-routing the wires.

The textual `.dcs` format remains human-readable and diff-friendly. Wire geometry is stored in a new dedicated **`# @wires`** annotation block (separate from `# @layout`, which keeps component positions). Both blocks are optional — a file without them remains a valid `.dcs` file.

**CLI-generated files (special case).** The headless CLI builds `.dcs` files from circuit definitions without knowing visual layout. Such files will have neither `# @layout` nor `# @wires`. When the GUI opens such a file it shall:

1. Run the existing auto-layout pass to assign component positions.
2. Run the new auto-router to compute initial wire geometry for every connection.
3. On first save, the file is updated in place with both blocks.

This means the CLI does **not** need to learn about geometry — it continues to emit only the structural lines (`inputs:`, `outputs:`, `<name> = <gate>(...)`) it produces today. The GUI is the source of truth for visual information.

---

## R5 — Two display modes per DCS

A DCS circuit shall support two display modes, switchable by the user:

1. **External (black-box) view** — the circuit is rendered as a single component shape exposing only its input and output pins, with the circuit's name (and optionally a small symbol) inside the shape. This is the form used when the DCS is referenced as a sub-component of another DCS.
2. **Internal (schematic) view** — the full internal schematic: every sub-component, every wire, every junction dot. This is the form used while editing or while inspecting the circuit standalone.

Requirements:

- The user shall be able to toggle between the two modes for the currently-open circuit (menu item, keyboard shortcut, or sidebar button — see the design doc for the chosen UX).
- When a DCS is instantiated as a sub-component inside another DCS, it shall appear in **External** mode by default in the outer schematic. The user can choose to view the inner one separately.
- The external shape must clearly show all top-level input and output pins so other circuits can wire to them.
- The **default** external shape is a plain rectangle with the circuit's name centered and labelled I/O pins on the edges. The renderer must dispatch through an **architectural hook** (a function pointer / vtable entry on the circuit's external-view metadata) so that future components can supply their own shape — e.g. a D flip-flop drawing a triangular clock-edge mark inside the rectangle at its clock input pin. The hook ships in this supplement; the full shape DSL and concrete custom shapes are deferred to Step 3 (Phase 3.1).
- This is **structural support only** — full hierarchical / chipset semantics (recursive evaluation, package-pin diagrams, etc.) remain a Step 3 concern. The supplement only requires that the data model and renderer accommodate the two visualizations of a single DCS, and that the rendering pipeline is extensible.

---

## R6 — Visual style

The overall schematic style shall follow the two demo images placed under `docs/`:

- **`step2-supplement-demo1.jpg`** — three rectangular IEC-style gates (`&` for AND, output bubble for negation) connected with strictly orthogonal wires.
- **`step2-supplement-demo2.jpg`** — a 3-to-8 decoder showing dense orthogonal routing with junction dots, IEC AND-gate shapes with output bubbles, triangle inverters with output bubbles, and a bus-style fanout where one source drives many destinations through a single net.

The supplement does not yet introduce a general shape DSL (that is Step-3 Phase 3.1); we may continue using the existing hardcoded per-component render code, refreshed to match the demo style.

---

## Resolved decisions

These items were open in an earlier draft and have now been settled by the user:

1. **Routing strategy.** *Decision:* Route once at wire-creation time, persist the result, and let the user **drag bend points** to customise.
2. **Net data model.** *Decision:* **Compute net membership on demand** from the existing wire-name pointers — no new domain object.
3. **Wire geometry storage syntax.** *Decision:* Add a dedicated **`# @wires`** annotation block (separate from `# @layout`). Named nets appear in the file via their producer wire-name. A side concern — `.dcs` files generated by the headless CLI have no layout/wires — is handled GUI-side: the GUI auto-lays-out and auto-routes on first open, and saves the result back. The CLI stays minimal.
4. **External-mode shape.** *Decision:* Default to a plain rectangle (component name + labelled I/O pins). Retain a render-time **architectural hook** (function pointer / vtable) so per-component custom shapes can plug in later — e.g. a D flip-flop drawing a clock-edge triangle inside its box. The full shape DSL ships in Step 3 Phase 3.1; the hook is in this supplement.
5. **Mode switch UX.** *Decision:* Make the choice in the design doc. The chosen path must be both easy to operate *and* flexibly extensible (so future view modes can plug in without re-designing the menu).
6. **Net highlight visuals.** *Decision:* Use **both** a thickened stroke **and** a distinct bright colour (e.g. orange or yellow), and emphasize the connected pin terminals (enlarged + recoloured).

---

## Out of scope (will be addressed in Step 3)

- ANSI shape DSL and toolbar icons (Step 3 Phase 3.1, 3.2).
- Variable pin counts, CLK component, real circuit elements (Phases 3.3 – 3.5, 3.8).
- Recursive evaluation of nested DCS sub-components and package-pin diagrams (Phase 3.6).
- Full chipset / sub-circuit file format (Phase 3.6, 3.7).
