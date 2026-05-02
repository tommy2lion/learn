# DCS Phase 1 Design

## Scope

Build a working end-to-end prototype with AND, OR, NOT gates:
- A simulation engine that evaluates circuits
- A `.dcs` text format to save and load circuits
- A CLI that runs a `.dcs` file and prints output
- A GUI to draw circuits interactively and display timing diagrams

**Tech stack:** C99 + raylib, Windows + MSYS2 MinGW64 (consistent with sibling projects).

---

## Directory Structure

```
dcs/
├── src/
│   ├── sim/
│   │   ├── sim.h          # types (Signal, Wire, Instance, Circuit) + API
│   │   └── sim.c          # gate eval + propagation (no raylib)
│   ├── parser/
│   │   ├── parser.h       # parse_circuit(), serialize_circuit()
│   │   └── parser.c       # .dcs lexer + parser + serializer (no raylib)
│   └── gui/
│       ├── main.c         # raylib init + event loop
│       ├── canvas.h/c     # render component boxes and wires
│       ├── editor.h/c     # interactive editing state
│       ├── sidebar.h/c    # component palette (AND/OR/NOT)
│       └── waveform.h/c   # timing diagram widget
├── cli/
│   └── main.c             # CLI entry point (no raylib)
├── test/
│   ├── test_sim.c         # unit tests for sim module
│   └── test_parser.c      # unit tests for parser module
├── circuits/              # fixture .dcs files used by tests and demos
│   ├── and_gate.dcs
│   ├── not_gate.dcs
│   └── half_adder.dcs
└── Makefile
```

---

## `.dcs` Syntax for Phase 1

A circuit file defines named wires. Each wire is the output of a gate applied to previous wires or external inputs.

```
# and_gate.dcs
inputs: a, b
outputs: y

y = and(a, b)
```

```
# half_adder.dcs
inputs: a, b
outputs: sum, carry

carry = and(a, b)
a_or_b = or(a, b)
n_carry = not(carry)
sum = and(a_or_b, n_carry)
```

Rules:
- `inputs:` and `outputs:` declare the external interface (comma-separated names)
- Each body line has the form `<wire_name> = <gate>(<input>, ...)` where inputs are wire names or circuit inputs
- Supported gates: `and(a, b)`, `or(a, b)`, `not(a)`
- Wire names must appear in `inputs:` or be defined by a previous line before use
- Output wire names must all be assigned by the end of the file
- Comments start with `#`

**Timing simulation:** Inputs can be given multiple values (one per time step). Running 4 steps with `a = 0,1,0,1` and `b = 0,0,1,1` produces 4 rows in the output table. Phase 1 is purely combinational — output at step N depends only on inputs at step N.

---

## Step 1.1 — Simulation Engine

**Goal:** Evaluate any combinational circuit built from AND, OR, NOT.

### Key Types (`src/sim/sim.h`)

```c
typedef uint8_t Signal; // 0 = low, 1 = high, 2 = undefined

typedef struct {
    char name[64];
    Signal value;
} Wire;

typedef enum { GATE_AND, GATE_OR, GATE_NOT } GateType;

typedef struct {
    GateType type;
    char out_wire[64];      // output wire name
    char in_wires[2][64];   // input wire names (in_wires[1] unused for NOT)
    int  in_count;
} GateInst;

typedef struct {
    Wire     *wires;        // all wires (inputs + internal + outputs)
    int       wire_count;
    GateInst *gates;        // ordered list of gate instances
    int       gate_count;
    char      input_names[16][64];
    int       input_count;
    char      output_names[16][64];
    int       output_count;
} Circuit;
```

### API

```c
Circuit *circuit_new(void);
void     circuit_free(Circuit *c);

// Returns 0 on success, -1 if wire already exists
int circuit_add_input(Circuit *c, const char *name);
int circuit_add_output(Circuit *c, const char *name);
int circuit_add_gate(Circuit *c, GateType type, const char *out_wire,
                     const char *in1, const char *in2); // in2 NULL for NOT

// Set input signal values, then call circuit_run to evaluate
void circuit_set_input(Circuit *c, const char *name, Signal value);
Signal circuit_get_output(Circuit *c, const char *name);

// Evaluates all gates in order; returns 0 if all outputs resolved, -1 if cycle detected
int circuit_run(Circuit *c);
```

Gates are stored in the order they are added, which must be topological. The parser is responsible for ordering. For Phase 1, the `.dcs` format already enforces this (a wire must be defined before use).

### Tests (`test/test_sim.c`)

```
test_and_gate:       and(0,0)=0  and(0,1)=0  and(1,0)=0  and(1,1)=1
test_or_gate:        or(0,0)=0   or(0,1)=1   or(1,0)=1   or(1,1)=1
test_not_gate:       not(0)=1    not(1)=0
test_nand_chain:     not(and(a,b)) truth table
test_half_adder:     all 4 input combinations, verify sum and carry
test_undefined_input: running with Signal=2 propagates undefined correctly
```

---

## Step 1.2 — `.dcs` Parser and Serializer

**Goal:** Read a `.dcs` file into a `Circuit`; write a `Circuit` back to `.dcs` text.

### API (`src/parser/parser.h`)

```c
// Parses .dcs text into a new Circuit. Returns NULL on error (sets err_out).
Circuit *parse_circuit(const char *text, char *err_out, int err_len);

// Serializes a Circuit to .dcs text. Caller frees the returned string.
char *serialize_circuit(const Circuit *c);
```

Implementation stages:
1. **Lexer**: scan text into tokens — `IDENT`, `COLON`, `COMMA`, `EQUALS`, `LPAREN`, `RPAREN`, `NEWLINE`, `COMMENT`, `EOF`
2. **Parser**: consume tokens to build `Circuit` via the `circuit_add_*` API from Step 1.1
   - Parse `inputs:` line → `circuit_add_input`
   - Parse `outputs:` line → `circuit_add_output`
   - Parse body lines → `circuit_add_gate` (gate type inferred from function name)
3. **Serializer**: walk `Circuit` fields and emit valid `.dcs` text

### Tests (`test/test_parser.c`)

```
test_parse_and_gate:      parse and_gate.dcs → circuit with 1 gate, 2 inputs, 1 output
test_parse_half_adder:    parse half_adder.dcs → circuit with 4 gates, 2 inputs, 2 outputs
test_round_trip:          parse → serialize → parse again → results identical
test_parse_error_undef:   wire used before defined → returns NULL with error message
test_parse_error_syntax:  malformed line → returns NULL with error message
test_parse_comments:      comment lines and blank lines are ignored
```

---

## Step 1.3 — CLI Executable

**Goal:** Run a circuit from the command line; print a signal table.

### Usage

```sh
# Print output for a single input assignment
dcs_cli.exe and_gate.dcs --input "a=1,b=0"
# Output:
# a=1 b=0 -> y=0

# Run multiple time steps (comma-separated values per input)
dcs_cli.exe half_adder.dcs --input "a=0,1,0,1" --input "b=0,0,1,1"
# Output (one row per step):
# step  a  b  sum  carry
#    0  0  0    0      0
#    1  1  0    1      0
#    2  0  1    1      0
#    3  1  1    0      1
```

### Implementation (`cli/main.c`)

1. Parse `argv` for filename and `--input` flags
2. Call `parse_circuit(file_text, ...)`
3. For each time step: call `circuit_set_input` for each input, then `circuit_run`, then read outputs
4. Print the table

**Tests:** Shell-level integration tests using the fixture `.dcs` files in `circuits/`. Add a `make test_cli` Makefile target that runs the executable and checks output with simple string comparisons.

---

## Step 1.4 — GUI Foundation (View Only)

**Goal:** Open the GUI, load a `.dcs` file, see the circuit rendered as boxes and wires.

### Layout

```
+----------------------------------+
|  [File: half_adder.dcs]  [Open]  |
+----------------------------------+
|                                  |
|         CIRCUIT CANVAS           |
|   (component boxes + wires)      |
|                                  |
+----------------------------------+
```

### Rendering (`src/gui/canvas.h/c`)

```c
typedef struct {
    Vector2 pos;           // center on canvas
    char inst_name[64];
    char gate_type[16];    // "AND", "OR", "NOT"
    // pin positions computed from pos + gate_type
} CanvasNode;

typedef struct {
    CanvasNode *nodes;
    int node_count;
    // wires: pairs of (node_idx, pin_idx) for src and dst
} CanvasState;

void canvas_init(CanvasState *cs, const Circuit *c);  // auto-layout
void canvas_draw(const CanvasState *cs, Camera2D cam);
```

Auto-layout for Phase 1: arrange gates left-to-right in topological order, inputs on the left edge, outputs on the right edge.

Pan and zoom: handled by raylib's `Camera2D` (drag with middle mouse or right mouse; scroll to zoom).

### `gui/main.c` structure

```
init raylib window
load .dcs from argv[1] (or show file path input)
parse → Circuit → CanvasState
loop:
  handle pan/zoom input
  BeginDrawing
    canvas_draw(...)
  EndDrawing
close window
```

---

## Step 1.5 — GUI Editor (Interactive Editing)

**Goal:** Build a circuit by dragging gates onto the canvas and connecting pins.

### Layout

```
+----------+---------------------------+
| Sidebar  |   Canvas                  |
|          |                           |
| [AND]    |   [AND]---[NOT]           |
| [OR]     |                           |
| [NOT]    |                           |
|          |                           |
| [INPUT+] |                           |
| [OUTPT+] |                           |
+----------+---------------------------+
```

### Interaction model (`src/gui/editor.h/c`)

State machine with three modes:

| Mode | Trigger | Action |
|------|---------|--------|
| `IDLE` | default | hover highlight |
| `DRAGGING` | click sidebar item | places new gate at drop position |
| `WIRING` | click an output pin | drag to input pin, creates wire |

Additional interactions:
- Right-click a gate or wire → delete it
- Drag an existing gate to reposition
- `Ctrl+S` → serialize `Circuit` → write `.dcs` file

The editor maintains the `Circuit` struct as the authoritative state. Every edit calls `circuit_add_gate`, `circuit_add_wire`, or the removal equivalents. The canvas node positions are tracked separately in `CanvasState` (visual only, not part of the logical circuit).

---

## Step 1.6 — GUI Simulation and Timing Diagram

**Goal:** Set input values, click RUN, see a timing diagram.

### Layout

```
+----------+---------------------------+
| Sidebar  |   Canvas                  |
+----------+---------------------------+
|  Inputs  |   Timing Diagram          |
|  a: [0]  |   a    __    __           |
|  b: [1]  |   b  ____              |
|  [RUN]   |   y    __                 |
+----------+---------------------------+
```

### Inputs panel (`src/gui/editor.h/c`)

- One toggle button per circuit input (cycles 0 → 1 → 0)
- "Steps" counter (default 8): how many time steps to simulate

### RUN action

1. For each time step 0..N-1: apply input values → `circuit_run` → record outputs
2. For Phase 1 (combinational): inputs are static across all steps unless the user toggles between runs. Future: stimulus pattern per step.

### Timing diagram widget (`src/gui/waveform.h/c`)

```c
typedef struct {
    char  signal_name[64];
    uint8_t *values;   // one value per step
    int step_count;
} WaveformTrack;

void waveform_draw(const WaveformTrack *tracks, int track_count,
                   Rectangle bounds, int steps);
```

Each track renders as a step-function line: high = upper half, low = lower half, transitions are vertical edges.

---

## Makefile Targets

```makefile
# Executables
dcs_gui.exe   # src/sim + src/parser + src/gui + raylib
dcs_cli.exe   # src/sim + src/parser + cli/main.c  (no raylib)

# Tests (compile + run, exit 0 = all pass)
test          # runs test_sim.exe and test_parser.exe
test_sim      # unit tests for sim module
test_parser   # unit tests for parser module
test_cli      # integration tests for CLI (shell-level)

all: dcs_gui.exe dcs_cli.exe
clean: remove .exe and .o files
```

---

## Implementation Order

| Step | Deliverable | Tests |
|------|------------|-------|
| 1.1 | `sim.c/h` — gate types, circuit struct, `circuit_run` | `test_sim.c` all passing |
| 1.2 | `parser.c/h` — parse + serialize `.dcs` | `test_parser.c` all passing |
| 1.3 | `cli/main.c` — `dcs_cli.exe` runs circuits | shell integration tests with `circuits/*.dcs` |
| 1.4 | `gui/canvas.c/h` + `gui/main.c` — view a loaded circuit | manual: open `half_adder.dcs`, see boxes + wires |
| 1.5 | `gui/editor.c/h` + `gui/sidebar.c/h` — build a circuit interactively | manual: build half-adder, save to `.dcs`, re-open |
| 1.6 | `gui/waveform.c/h` — timing diagram after RUN | manual: run half-adder, verify waveform matches truth table |

Each step compiles and passes its tests before moving to the next. Steps 1.1–1.3 can be developed and validated entirely without a display.
