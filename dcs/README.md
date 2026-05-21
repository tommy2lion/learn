# DCS — Document trail

The development of DCS (Digital Circuit Simulation) — design, review, code review, and forward plan — laid out in reading order.

---

**[prototype_version/architecture.md](prototype_version/architecture.md)** — The architecture document formed through initial simple discussions (using natural language)

↓

**[prototype_version/step1-design.md](prototype_version/step1-design.md)** — Step 1 implementation plan (the prototype).

↓

**[docs/step2-refactor-plan.md](docs/step2-refactor-plan.md)** — Step 2 refactoring requirements (14 points).

↓

**[docs/step2-refactor-design.md](docs/step2-refactor-design.md)** — Step 2 design proposal: framework + domain + app, phased migration.

↓

**[docs/step2-review-of-refactor-design.md](docs/step2-review-of-refactor-design.md)** — Review of the design proposal (11 points; integrated back into `step2-refactor-design.md`).

↓

**[docs/step2-code-review-req.md](docs/step2-code-review-req.md)** — Code-review request after Step 2 implementation: seven aspects to audit, eight forward-looking requirements to assess.

↓

**[docs/step2-code-review.md](docs/step2-code-review.md)** — Code-review report: convention, safety, layering, SoC, testability, extensibility verdicts; per-item assessment of the eight forward requirements.

↓

**[docs/step2-supplement-req.md](docs/step2-supplement-req.md)** — Step 2 supplement requirements: orthogonal wire routing, junction dots, click-to-highlight nets, wire-geometry persistence, internal/external display modes.

↓

**[docs/step2-supplement-design.md](docs/step2-supplement-design.md)** — Step 2 supplement design: app-layer `wire_geometry` sidecar, Z-router, `# @wires` file-format extension, external-view render hook with per-pin style enum.

↓

**[docs/step2-supplement-implementation-plan.md](docs/step2-supplement-implementation-plan.md)** — Step 2 supplement implementation plan: 13 phased commits (1–11 required, 12–13 stretch) with goal / touches / steps / tests / done-when per phase.

↓

**[docs/step3-plan.md](docs/step3-plan.md)** — Step 3 plan: nine phases (3.0 hardening through 3.8 design-only analog accommodation), seven open questions awaiting decisions.

↓

**[docs/step2-supplement-refinement.md](docs/step2-supplement-refinement.md)** — Refinement entries captured during and after Phase 13: 19 R-* items spanning UX polish, deferred cosmetic work, framework dead-code findings, and Step-3 prerequisites.

↓

**[docs/step2-supplement-implementation-summary.md](docs/step2-supplement-implementation-summary.md)** — Retrospective on the 13-phase supplement implementation: what shipped, where the plan held up exactly, where it bent, and the design decisions that emerged from running it.

↓

**[docs/step2-supplement-refinement-plan.md](docs/step2-supplement-refinement-plan.md)** — Refinement implementation plan: orders the in-scope R-* items into 9 stages (≈12–13 commits with sub-staging), each bounded enough to fit one AI session without context drift.

↓

**[docs/step2-supplement-refinement-summary.md](docs/step2-supplement-refinement-summary.md)** — Retrospective on the refinement work: 11 R-* items resolved, 3 partial, 6 deferred to Step 3; 561 → 683 tests across 22 commits; side-bugs and design themes that surfaced along the way.

↓

**[docs/step2-end-unfinished-req.md](docs/step2-end-unfinished-req.md)** — Step 2 carry-over audit: 38 unfinished requirements compiled from every Step 2 doc, cross-checked against current source. Maps each item to its proposed Step 3 phase; informs the Step 3 plan.
