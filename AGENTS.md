# AGENTS.md

## Purpose

This repository contains a lightweight native Linux spatial filesystem navigator.

The application uses a 2.5D retro-futuristic filesystem visualization inspired by the general concept of early spatial filesystem navigators.

Read `DESIGN.md` before making significant architectural or user-interface changes.

`DESIGN.md` defines product goals and intended behavior.

This file defines implementation rules for automated coding agents working in the repository.

---

# Development Environment

Primary target:

- Linux
- X11
- Void Linux development environment

Primary language:

- C++20

Initial libraries:

- SDL2
- OpenGL
- Dear ImGui
- CMake

Do not replace the core stack without a specific technical reason.

Do not silently change the X11 requirement to Wayland.

Wayland support may be considered later, but native X11 behavior is currently a requirement.

---

# General Working Rules

Before making substantial changes:

1. inspect the existing repository
2. read `DESIGN.md`
3. understand the relevant subsystem
4. propose or internally determine the smallest appropriate change
5. avoid unrelated refactoring
6. build after meaningful changes
7. run applicable tests

Prefer incremental implementation over large rewrites.

Do not create elaborate abstractions without a demonstrated need.

Do not introduce new dependencies casually.

---

# Architecture

Maintain strong separation between:

```text
filesystem
analysis
layout
graphics
navigation
ui
```

These layers should not be collapsed into one another.

---

# Filesystem Layer

The filesystem layer is responsible for retrieving filesystem information.

It may know about:

- paths
- metadata
- file types
- timestamps
- permissions
- symlinks
- directories

It must not know about:

- OpenGL
- shaders
- screen coordinates
- camera matrices
- rendered geometry

Do not embed rendering behavior in filesystem classes.

---

# Analysis Layer

The analysis layer is responsible for semantic interpretation.

Examples:

- salience computation
- series detection
- temporal grouping
- generated-file classification
- clustering
- semantic file classification

Analysis output should be representable as ordinary data.

Do not render directly from analysis code.

Do not place salience heuristics inside OpenGL code.

---

# Layout Layer

The layout system converts analyzed filesystem information into spatial placement.

Layout may determine:

- position
- depth
- scale
- cluster bounds
- orientation
- group arrangement

Layout must remain independent from low-level rendering.

Do not perform filesystem scans inside layout code.

---

# Graphics Layer

The graphics subsystem should draw scene data provided by other layers.

It may know about:

- geometry
- shaders
- buffers
- camera matrices
- visual styling
- line rendering
- text rendering where appropriate

It should not decide:

- whether binaries are important
- whether files belong to a series
- whether timestamps indicate related files
- which files are likely important

Those decisions belong elsewhere.

---

# Navigation Layer

Navigation should manage:

- current directory
- selection
- focus
- navigation history
- requested transitions
- camera target requests

Navigation should request semantic actions such as:

> focus object X

rather than directly constructing camera matrices.

---

# Camera Rules

The interface is primarily 2.5D.

Do not turn it into a first-person 3D environment.

Do not introduce WASD free-flight controls unless explicitly requested.

The camera should support:

- orbit around the current focal location
- limited yaw
- limited pitch
- zoom
- reset-view
- animated movement to focused objects
- animated movement when entering directories

The default view should remain easy to understand and relatively shallow.

Orbit is a secondary inspection mechanism, not the primary method of filesystem navigation.

Avoid uncontrolled roll.

Avoid camera states that leave the scene disorienting or unusable.

---

# Motion and Animation

Navigation transitions should visibly animate when appropriate.

Important cases include:

- entering a directory
- leaving a directory
- focusing a file
- focusing a directory
- expanding a group
- collapsing a group
- moving to a search result

Avoid instantaneous hard cuts unless required for responsiveness.

Animations should remain:

- short
- responsive
- interruptible when practical
- deterministic

Do not implement elaborate cinematic camera systems.

Use a small explicit interpolation or transition system.

Avoid introducing a large animation dependency.

---

# Animation Separation

Do not bury animation state inside unrelated rendering code.

Prefer explicit state such as:

- starting camera state
- target camera state
- elapsed time
- duration
- easing function

The same principle applies to animated layout or group expansion.

---

# Salience

Salience is a core architectural concept.

Do not treat all filesystem entries as equally important.

The salience system should remain separate from rendering.

Initial heuristics should be deterministic and understandable.

Potential factors include:

- modification recency
- access recency where useful
- user interaction history
- file type
- generated-file likelihood
- series membership
- temporal grouping
- directory density
- hidden-file status

Do not prematurely introduce machine learning.

Do not design the architecture around a single fixed salience equation.

The scoring method should remain replaceable.

---

# Recency

Recently modified files should normally receive increased salience.

Do not implement recency as a single binary new/old threshold unless required temporarily for a prototype.

Prefer a continuous weighting model.

The exact weighting function should remain configurable or isolated.

---

# Generated and Low-Interest Files

Likely-generated files may receive lower salience.

Examples include:

- `.o`
- `.a`
- `.so`
- generated dependencies
- cache files
- temporary files
- intermediate build output

Do not hide these files permanently.

Deweighting is different from exclusion.

User behavior should eventually be able to override default assumptions.

---

# Series Detection

Support future detection of sequences such as:

```text
img1.png
img2.png
img3.png
```

or:

```text
frame_001.png
frame_002.png
frame_003.png
```

Series detection belongs in the analysis layer.

Do not implement series recognition in rendering code.

Detected series should be passed to layout and rendering as a group.

---

# Related Files

The architecture should support groups inferred from:

- filename similarity
- timestamp proximity
- file type
- shared structure
- later functional relationships

Keep early heuristics conservative.

Do not infer complex relationships using opaque logic if a simpler deterministic method is sufficient.

---

# Large Directories

Large directories must not overwhelm the scene.

Do not simply render thousands of equally prominent objects.

Support strategies such as:

- salience-based selection
- summarization
- clustering
- progressive expansion
- group objects

The exact thresholds should not be deeply hard-coded.

Large-directory behavior should eventually be configurable.

---

# Progressive Disclosure

More detail should appear when an area becomes important.

A distant or unfocused directory may remain compact.

A focused or entered directory may expand.

Do not assume every item requires a visible label at all times.

---

# File Colors

Use semantic file categories.

The renderer should receive semantic style information rather than infer file types itself.

Colors should be inspired by common terminal conventions.

Possible future support for `LS_COLORS` should not require major architectural changes.

Do not hard-code classification logic in shaders.

---

# Geometry

Use lightweight simple geometry.

Avoid detailed models.

Prefer:

- lines
- boxes
- plates
- frames
- simple extrusions
- stacks
- ribbons
- compact group geometry

Do not add heavyweight model-loading infrastructure unless explicitly needed.

---

# Rendering Performance

Do not perform unnecessary work every frame.

Avoid:

- filesystem access in the render loop
- repeated heap allocation each frame
- rebuilding static geometry unnecessarily
- recomputing layout when inputs have not changed

Cache where reasonable.

Use profiling before introducing complex optimizations.

---

# Filesystem Performance

Do not recursively scan the whole filesystem at startup.

Do not compute expensive directory statistics synchronously when avoidable.

Large directory scans should eventually support asynchronous execution.

The render loop must remain responsive.

---

# Error Handling

Filesystem errors are expected runtime conditions.

Handle cases such as:

- permission denied
- missing file
- disappearing file
- broken symlink
- unreadable directory
- unavailable mount

Do not allow these conditions to terminate the application unnecessarily.

---

# File Operations

Do not add destructive file operations during early milestones unless requested.

When later implemented:

- deletes must be explicit
- overwrite behavior must be clear
- rename/move errors must be handled
- symlink semantics must remain explicit

Do not silently delete or overwrite user data.

---

# Dependency Environment

The project is developed inside a Void Linux Podman container.

System packages are installed when the container image is built.
Do not attempt to install system packages during development.

If a required system dependency is missing:

1. stop before introducing a workaround
2. identify the exact missing dependency
3. identify the expected Void Linux package if known
4. explain why it is required
5. allow the user to update and rebuild the container

Small project-specific dependencies may be vendored when this is an
intentional project decision.

Before adding a new dependency, determine whether existing components or the standard library can perform the task adequately.

Prefer small focused dependencies.

Avoid:

- unnecessary frameworks
- embedded browsers
- game engines
- large GUI frameworks
- complex dependency trees

Dear ImGui is maintained as a Git submodule at:

    third_party/imgui/

The project should compile the required Dear ImGui source files directly
as part of the application build.

For the current SDL2 + OpenGL3 implementation, use the standard Dear
ImGui SDL2 platform backend and OpenGL3 renderer backend.

Do not download Dear ImGui during CMake configuration or compilation.
Do not use CMake FetchContent to retrieve it.
Do not replace the submodule with a system-installed ImGui package.

Other third-party source dependencies should not be added as Git
submodules without approval.

# CMake

Keep the build system readable.

Avoid large amounts of abstraction for a small project.

Prefer explicit targets.

Use modern target-based CMake.

Avoid global compiler or linker state where target-level configuration is appropriate.

---

# C++ Style

Prefer modern C++20 where it improves clarity.

Prioritize:

- clear ownership
- RAII
- const correctness
- simple value types
- explicit lifetime
- understandable APIs

Avoid clever template-heavy designs without clear benefit.

Do not build custom abstractions when standard-library types are sufficient.

---

# Threading

Do not introduce extensive multithreading early.

The likely long-term model is:

- main/render/UI thread
- background filesystem or analysis tasks where needed

Use synchronization conservatively.

Do not create thread pools or task frameworks before there is a concrete requirement.

---

# Dear ImGui

Use Dear ImGui for practical conventional interface elements such as:

- status display
- search
- metadata panels
- settings
- menus
- debugging tools

Do not implement the entire spatial scene as ImGui widgets.

The main filesystem visualization should be rendered through the graphics subsystem.

---

# Shaders

Use modern OpenGL shader-based rendering.

Do not use deprecated fixed-function OpenGL.

Keep shaders small and readable.

Avoid shader complexity unless required by the visual design.

---

# Visual Effects

The intended aesthetic is restrained.

Avoid:

- excessive bloom
- heavy post-processing
- particle effects
- elaborate reflections
- expensive lighting
- complex materials

Prefer:

- clean wireframes
- simple geometry
- subtle brightness differences
- selective depth cues
- terminal-inspired colors

---

# Testing

Add tests where logic can be tested independently of rendering.

Good test candidates include:

- series detection
- salience functions
- file classification
- grouping
- path handling
- layout constraints

Do not require OpenGL to test pure analysis functions.

Separate logic from rendering sufficiently to make this possible.

---

# Manual Visual Validation

Some behavior cannot be validated solely by automated tests.

Examples include:

- camera feel
- orbit limits
- animation speed
- readability
- clutter
- spatial coherence

Do not claim that visual behavior has been verified unless the application was actually run and inspected.

When visual inspection is required, state that explicitly.

---

# Milestone Discipline

Work milestone by milestone.

Do not implement later-stage functionality early unless it is required to support the current architecture.

For Milestone 1:

- use mock filesystem objects
- do not scan the real filesystem
- implement rendering
- implement camera
- implement orbit
- implement one animated navigation transition

For Milestone 2:

- use an in-memory mock filesystem
- test spatial layout
- test salience
- test grouping
- test file coloring

Real filesystem integration begins later.

---

# Scope Control

When given a task:

1. implement the requested behavior
2. avoid unrelated cleanup
3. avoid architecture rewrites unless necessary
4. mention significant design concerns
5. keep the repository buildable

Small coherent changes are preferable to broad speculative changes.

---

# Agent Behavior

When uncertain about a design choice:

- consult `DESIGN.md`
- preserve existing architecture
- choose the simpler implementation
- avoid introducing new dependencies
- leave clear notes where future refinement is intended

Do not silently reinterpret major product requirements.

---

# Repository Documentation

Keep documentation synchronized with significant architectural changes.

If the implementation intentionally deviates from `DESIGN.md`, either:

- update the design document
- or clearly explain the inconsistency

Do not allow implementation and design documentation to diverge silently.

---

# Initial Goal

The first objective is not to create a complete file manager.

The first objective is to prove that the intended interaction model works:

- lightweight X11 application
- 2.5D spatial filesystem scene
- orbit around the current focal location
- perspective depth
- smooth animated movement
- clear mock directory and file geometry

The filesystem backend and intelligent analysis will be layered onto this foundation later.
