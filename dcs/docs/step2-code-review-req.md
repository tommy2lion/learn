# Step 2 Code Review Request

Please perform a thorough walkthrough of the current codebase, focusing on the following aspects:

## (1) Compliance with Coding Conventions
Does the code strictly adhere to the coding conventions defined in `step2-review-of-refactor-design.md`?
- Correct use of macros `class` / `interface`
- Naming rules (`tagt_` prefix, `_t` suffix, snake_case)
- Header protection, access control, `self` parameter conventions, etc.

## (2) Code Safety
- Are there any risks of wild pointers?
- Are array subscripts protected against out-of-bounds access?
- Do loops have upper-bound protection?
- Any memory leaks or double-free issues in memory management?

## (3) Framework Independence Verification
Can the framework portion be compiled independently, providing only libraries and headers, completely decoupled from the application?
*(Not asking to change the build process, only to verify architecturally that the framework never depends on application code.)*

## (4) Responsibility Assignment and Separation of Concerns
- Is the assignment of responsibilities reasonable?
- Does the code follow the principle of separation of concerns?
- Does the object design follow the Dependency Inversion Principle (depending on interfaces rather than concrete implementations)?

## (5) Testability and Mock-Friendliness
- Is the object design easy to unit-test?
- Is it easy to design mock classes to replace interface implementations?
- Can core logic be tested without bringing in a graphical environment?

## (6) Extensibility Design
- How extensible is the current object design?
- Does it strictly adhere to “depend on interfaces, not concrete implementations”?
- Would adding a new graphics backend, platform implementation, or component type require minimal changes?

## (7) Extensibility Review Against Future Requirements
To help evaluate the current design's extensibility, the following next-step requirements are proposed. Please analyze whether the current architecture can naturally support these needs, and how much change would be required.

### (7-1) ANSI/IEEE Standard Symbol Support
It is desired to use ANSI/IEEE standard gate shapes for a more professional appearance.
Basic gates (AND, OR, NOT, etc.) can be constructed from combinations of straight lines, arcs (including semicircles and full circles).
A simple description language is envisioned, using the gate's center as the coordinate origin, with concise statements to describe the gate shape.
Example (AND gate):
```
shape: line(-2,1,0,1), line(-2,0,0,-1), arc(...), circle(...)
```

### (7-2) Simulation Package
The shape description method above should be extensible to any custom component, forming a component's “simulation package” (graphical symbol used in logic simulation).

### (7-3) Chipsets and Physical Package
- For example, a NOT gate: its simulation package would be a triangle plus a small circle, one input pin, one output pin.
- A chipset (e.g., 74LS08) internally consists of multiple NAND gates. A chipset may contain multiple components, but also typically needs power and ground pins.
- Corresponding to the simulation package, a chipset also requires a “physical package” pinout diagram (physical pin arrangement).

### (7-4) CLK Clock Generator
The CLK generator should be provided as a special primitive component, given its frequent use.

### (7-5) Forward-Looking Design Considerations for Real Circuit Components
Real circuit simulation involves basic components like resistors, inductors, capacitors, light bulbs, LEDs, crystal oscillators, etc.
Due to the complexity of real circuits, these can be deferred to Step 3, but the current design should accommodate their future extension.

### (7-6) Toolbar Button Icons
Can the icons for the basic primitive buttons on the left toolbar use miniature versions of ANSI/IEEE gate shapes, with labels such as “AND”, “OR”, “NOT”, “CLK” beside them?

### (7-7) Custom Input Sequences
In the left INPUT area, is it possible to directly input a bit sequence like “1010110” to define stimulus?

### (7-8) File Type Planning Discussion
- Currently, component files use the `.dcs` suffix. Is there a need to differentiate file types?
- For the shape drawings of each component: should they be stored in a single separate file, or defined internally by each component (the latter approach also allows a single gate type to have multiple rich representations)?
- If custom input sequences are included, they could form a “simulation file” together with the current simulated components.
- When there are many output pins, is it necessary to save the waveform output as a separate file for independent display and analysis?

---

Based on the above, please first conduct a code walkthrough and produce `step2-code-review.md`; also integrate the unimplemented Step 2 requirements with the above needs to form `step3-plan.md`.
Do not output `step3-design.md` yet; for now, only discuss the Step 3 plan.