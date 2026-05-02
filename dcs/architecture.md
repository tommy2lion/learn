# DCS Architecture

## Project Goal

DCS (Digital Circuit Simulation) is a tool for building and simulating digital logic circuits. It supports:

- A visual GUI editor for drawing circuits and observing timing diagrams
- A text-based circuit file format that fully describes any circuit
- A CLI/headless mode for scripted simulation without opening the GUI
- A hierarchical component model that scales from primitive gates up to a CPU

---

## Core Concept: Everything Is a Component

The central abstraction is the **component**. A component has:

- Named **input pins**
- Named **output pins**
- An **implementation** — either a built-in primitive or a circuit made of sub-components

This applies at every level of abstraction:

| Level | Examples |
|-------|---------|
| Primitive | AND, OR, NOT |
| Basic composite | NAND, NOR, XOR, XNOR, half-adder |
| Medium composite | D flip-flop, 4-bit adder, counter, register |
| Complex | ALU, control unit, memory |
| System | CPU |

Building a CPU is the same operation as building a NAND gate — you compose smaller components. There is no special case.

---

## File Format: `.dcs`

### Key Decision: The Circuit File Is the Script

Rather than keeping two separate files (a circuit state file + a command script), DCS uses a **single `.dcs` text file** that is both the saved circuit and the recipe to recreate it.

Rationale:
- No synchronization problem between "state" and "commands"
- The GUI acts as a visual editor that reads and writes `.dcs` files
- The CLI executes the same `.dcs` files headlessly
- A circuit opened in the GUI can always be reconstructed from its `.dcs` file alone
- Follows the model of infrastructure-as-code tools (Makefile, Dockerfile, Terraform HCL)

### Syntax (Draft)

A `.dcs` file defines a named component and its body. The body lists sub-component instantiations and wire connections.

```
# nand.dcs — NAND gate built from AND + NOT
component nand(a, b) -> y:
    g1 = and(a, b)
    y  = not(g1)
```

```
# half_adder.dcs
component half_adder(a, b) -> sum, carry:
    sum   = xor(a, b)
    carry = and(a, b)
```

```
# top-level test circuit (no component keyword = runnable entry point)
inputs:  a, b, cin
outputs: sum, cout

fa = full_adder(a, b, cin)
sum  = fa.sum
cout = fa.carry
```

Key syntax rules:
- `component name(inputs) -> outputs:` defines a reusable component
- A file without a `component` header is a **top-level circuit** — it can be run directly
- References to other components are resolved by searching the **component library path**
- Comments start with `#`

### Component Library

Each `.dcs` file containing a `component` definition is a library file. A search path (similar to `$PATH`) tells the interpreter where to look for component definitions. This allows:

- A standard library of common components (NAND, XOR, flip-flops, adders)
- Per-project component directories
- Future package-style distribution of component collections

---

## Architecture Layers

The software is divided into four layers. Layers 1 and 2 have no dependency on the UI.

### Layer 1 — Simulation Engine

Pure logic, no UI, no file I/O.

- **Component registry**: maps component names to their definitions
- **Circuit graph**: a directed graph of component instances and wires (net list)
- **Signal propagation**:
  - Combinational circuits: topological sort + single-pass evaluation
  - Sequential circuits (Phase 2+): clock-driven step simulation
- **Timing recorder**: collects input/output signal values at each time step, produces a waveform data structure

### Layer 2 — DSL Parser and Interpreter

Sits between the file system and the simulation engine.

- Parses `.dcs` syntax into an AST
- Resolves component references (lookup in library path, recursive loading)
- Instantiates the circuit graph for the simulation engine
- Serializes an in-memory circuit graph back to `.dcs` text (used by the GUI to save)

### Layer 3 — CLI Mode

Runs without opening a window.

```sh
dcs run circuit.dcs --input "a=1,b=0,cin=1" --steps 8
dcs run circuit.dcs --stimulus stimulus.txt --output timing.csv
```

- Loads and interprets a `.dcs` file
- Accepts input values from flags or a stimulus file
- Outputs timing data to stdout or a file

### Layer 4 — GUI Mode

Visual editor and simulation viewer (tech stack TBD; C + raylib is the established pattern in this repo).

- **Canvas**: displays components as labeled boxes, wires as lines; drag to place, click to connect
- **Properties panel**: set input signal values before running
- **Run/Step controls**: run full simulation or advance one clock step at a time
- **Timing diagram**: waveform viewer showing all signal values over time
- **Sidebar**: component browser (primitives + loaded library components)

Every edit in the GUI updates the underlying `.dcs` representation. Saving writes the `.dcs` file. Opening a `.dcs` file parses it and rebuilds the visual layout.

> **Layout note**: The `.dcs` format describes connectivity, not visual positions. Visual layout (x/y coordinates of component instances on the canvas) can be stored in an optional `# @layout` annotation block at the end of the file, so the GUI can restore positions without polluting the logical description.

---

## Phased Development

### Phase 1 — MVP

**Goal**: end-to-end working prototype with the simplest circuits.

- Primitives: AND, OR, NOT (built-in)
- `.dcs` file format: component definitions, instantiation, wiring
- CLI: load circuit, set inputs, print output table
- GUI: place gates, draw wires, set inputs, click RUN, see timing diagram

### Phase 2 — Component System

- User-defined composite components in `.dcs` files
- Component library with configurable search path
- Standard library: NAND, NOR, XOR, XNOR, half-adder, full-adder, D flip-flop, counter, register

### Phase 3 — Sequential Logic

- Clock signal abstraction
- Flip-flop and latch primitives
- Multi-bit bus support
- Memory block component
- Step-by-step simulation with clock visualization

### Phase 4 — CPU

- Multi-bit ALU
- Register file
- Instruction decoder / control unit
- Program memory + simple assembler to load programs
- End-to-end: write assembly → simulate execution → observe register states

---

## Summary of Key Decisions

| Question | Decision |
|----------|----------|
| Circuit file suffix | `.dcs` |
| Command script vs. circuit file | **Unified** — the `.dcs` file IS the script |
| Component model | Everything is a component; primitives are just the base case |
| Visual layout storage | Optional annotation block inside the `.dcs` file, does not affect simulation |
| Simulation model (Phase 1) | Combinational only (no clock); sequential added in Phase 3 |
