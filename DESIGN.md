# Spatial Filesystem Navigator

## 1. Project Summary

This project is a lightweight native Linux filesystem navigator inspired by the visual style and spatial interaction of the filesystem navigator shown in *Jurassic Park*.

The goal is not to recreate the original program exactly. Instead, the application should reinterpret the concept as a modern, useful filesystem browser with:

- a retro-futuristic spatial aesthetic
- a primarily 2.5D visual layout
- lightweight native rendering
- strong keyboard and mouse interaction
- intelligent visual prioritization of files and directories
- animated spatial transitions
- optional perspective changes through camera orbit
- conventional file-management functionality where appropriate

The program should remain fast, responsive, and relatively small in scope and dependencies.

The initial target platform is Linux running X11.

---

# 2. Development Stack

Initial implementation should use:

- C++20
- CMake
- SDL2 for window creation, input, and event handling
- OpenGL 3.3 Core or newer conservative desktop OpenGL
- Dear ImGui for conventional UI elements
- standard C++ filesystem facilities where practical
- Linux-specific APIs where they offer clear benefit

The program should not use:

- Electron
- embedded browser engines
- web technologies as the primary UI
- Qt unless a later design decision strongly justifies it
- a full game engine
- unnecessarily large runtime frameworks

Dependencies should be introduced conservatively.

The application should explicitly support native X11 operation.

---

# 3. Design Philosophy

The navigator is not intended to be a conventional file manager with a decorative 3D skin.

The filesystem should be presented as a spatial information environment in which visual prominence represents likely user relevance.

The central concept is:

> The spatial layout should help direct the user's attention toward files and directories they are most likely to want.

The visualization should communicate:

- relevance
- recency
- relationship
- density
- file type
- hierarchy

The filesystem remains the underlying data model, but directory order alone should not determine visual prominence.

---

# 4. Visual Style

The overall visual language should evoke a restrained retro-futuristic technical interface.

Desired characteristics include:

- dark background
- thin wireframe or line-based geometry
- simple geometric forms
- restrained use of glow or brightness
- terminal-inspired colors
- perspective depth without excessive visual complexity
- clear and readable text
- relatively low polygon counts
- subtle animation rather than elaborate visual effects

The visual treatment should feel more sophisticated than the original FSN while preserving its conceptual character.

The scene should remain legible and functional even when displaying real directories with many files.

---

# 5. 2.5D Spatial Model

The normal interface should be fundamentally 2.5D rather than a freely navigable three-dimensional world.

Most filesystem objects should occupy positions in a predominantly planar or layered spatial arrangement.

Depth should be used to:

- distinguish hierarchy
- visually separate clusters
- establish foreground and background
- show relationships
- improve legibility
- support perspective effects

The user should not need to navigate through a fully three-dimensional environment in order to use the application.

Filesystem navigation must remain simple and predictable.

---

# 6. Camera and Perspective

Although the interface is primarily 2.5D, the scene should still be rendered through a perspective-capable camera.

The user should be able to orbit around the current focal location.

Orbit behavior may change:

- yaw
- pitch
- limited viewing distance
- perspective angle

The normal working view should remain relatively shallow and easy to understand.

Orbiting should allow the user to inspect the depth and structure of the current filesystem scene without transforming the program into a conventional 3D game or free-flight environment.

The camera should provide an easy way to return to the standard useful viewing orientation.

Possible controls may include:

- mouse drag
- modifier + mouse drag
- keyboard shortcuts
- reset-view command

Roll should normally remain fixed.

The camera should avoid disorienting rotations.

---

# 7. Motion and Animated Navigation

Movement is an important part of the interface.

Changes in navigation context should normally be animated rather than appearing instantaneously.

Examples include:

- entering a directory
- moving to a parent directory
- focusing on a particular file
- selecting a visually distant directory
- expanding a grouped cluster
- collapsing a grouped cluster
- following search results
- switching between important locations

The animation should make the spatial relationship between the old and new context understandable.

For example, entering a directory may cause:

1. the selected directory object to become the focal point
2. the camera to move toward it
3. surrounding items to shift or recede
4. the directory's contents to expand into the new scene
5. the camera to settle into the normal browsing orientation

Similarly, focusing on a particular file may cause a shorter camera translation and scale adjustment that visually centers the selected object while preserving surrounding context.

Animations should be:

- smooth
- relatively short
- interruptible when practical
- deterministic
- not unnecessarily cinematic

Responsiveness should take priority over visual spectacle.

The user should be able to disable or reduce animations if desired.

---

# 8. Filesystem Data Model

The filesystem layer should represent items independently from the renderer.

A filesystem entry may contain information such as:

- name
- full path
- parent path
- file type
- extension
- MIME-like classification if available
- size
- modification time
- access time when reliable
- permissions
- ownership
- symlink information
- executable status
- hidden status
- generated/build-artifact status if inferred
- directory child count
- optional usage history

Filesystem state must not be encoded directly in rendering objects.

---

# 9. Salience

Each file and directory should eventually receive a calculated salience score.

Salience represents the estimated probability that an item is relevant to the user's current attention or likely next action.

Salience may influence:

- distance from the visual focal region
- rendered size
- label visibility
- brightness
- geometry complexity
- priority for individual rendering
- likelihood of being summarized into a group
- degree of visual emphasis

The initial salience model should use simple heuristics.

Possible inputs include:

- modification recency
- access recency where useful
- recent navigation history
- explicit user interaction
- file type
- apparent direct usefulness
- relationship to other recently modified files
- membership in a detected series
- directory size
- whether the item appears generated
- whether the item is hidden
- whether the item appears to be a build artifact
- whether the file is executable
- frequency of user interaction

The architecture must allow the scoring model to evolve without changing rendering code.

---

# 10. Recency

Recently modified files should generally have higher visual prominence than very old files.

Recency should be treated continuously rather than using only fixed categories such as "new" and "old."

A recency weighting function may eventually use:

- exponential decay
- logarithmic decay
- piecewise weighting

The exact mathematical model should remain configurable.

Recency should influence salience but should not completely dominate it.

An old but frequently used file may remain more important than a newly generated temporary file.

---

# 11. File Series Detection

The navigator should detect apparent file series.

Examples include:

- img1.png
- img2.png
- img3.png

or:

- frame_001.png
- frame_002.png
- frame_003.png

or similar sequential naming schemes.

Possible indicators include:

- common filename stem
- numeric suffix
- alphabetic suffix
- common extension
- close timestamps
- similar sizes

Detected series should be represented as a cohesive visual object rather than consuming one unrelated object per file.

Possible visual forms include:

- segmented bars
- stacked plates
- ribbons
- arcs
- grouped towers
- repeated geometric modules

The group should be expandable so individual files remain accessible.

---

# 12. Temporal Grouping

Files that appear to belong to the same work session or output batch may be visually grouped based on timestamps.

Examples include:

- several simulation results generated within minutes
- multiple screenshots created during the same session
- documents edited during the same work period
- a group of plots exported together

Temporal grouping should be conservative.

Timestamp proximity alone should not always imply strong relationship.

Other evidence may include:

- filename similarity
- common extension
- common size range
- common parent directory

---

# 13. Structural and Functional Grouping

Later versions may infer relationships that are not apparent from naming or timestamps alone.

Examples include:

- `.cpp` and corresponding `.hpp`
- source files associated with a `CMakeLists.txt`
- data file and derived plot
- image and sidecar metadata
- log and corresponding output file

This should be considered an advanced feature.

Early versions should favor transparent deterministic heuristics over complex classification systems.

---

# 14. Group Visualization

Related files should appear as cohesive shapes or visually connected structures.

The visualization should communicate that the items belong together.

Methods may include:

- shared base geometry
- common enclosing frame
- common outline
- common platform
- subtle connector lines
- compact arrangement
- shared highlight behavior

Grouping should reduce clutter without making individual files inaccessible.

A group should support expansion and collapse.

---

# 15. Large Directory Handling

Large directories must not consume a disproportionate amount of the visible scene.

The program should avoid rendering hundreds or thousands of equal-priority objects simultaneously.

Possible mitigation strategies include:

- salience-based filtering
- grouped summaries
- type-based clustering
- age-based clustering
- generated-file clustering
- progressive detail
- rendering only representative items
- expanding additional items on demand

Example:

A directory containing 1,000 files might display:

- the 10 most salient files individually
- several detected series
- one cluster of older files
- one cluster of build artifacts
- one cluster of miscellaneous low-salience files

The layout engine should have explicit density limits.

---

# 16. Progressive Disclosure

The amount of detail shown should depend on user focus.

An unfocused directory may be represented compactly.

A focused directory may reveal:

- additional child files
- labels
- metadata
- detected groups
- internal structure

Entering the directory may reveal its complete local context.

This should reduce visual overload while maintaining discoverability.

---

# 17. Low-Interest and Generated Files

Files that users are unlikely to select directly should generally receive reduced visual prominence.

Examples may include:

- object files
- static libraries
- shared libraries
- caches
- temporary files
- generated dependency files
- intermediate build artifacts

These files must not automatically become invisible.

Their default salience should simply be lower.

User interaction should be able to override this assumption.

If a supposedly low-interest file is repeatedly selected or opened, it should become more prominent.

---

# 18. Terminal-Inspired File Colors

File and directory colors should be inspired by conventional terminal file coloring.

The default visual semantics should feel familiar to Linux users.

Likely categories include:

- directories
- executables
- source/text files
- images
- archives
- symlinks
- media
- devices or special files
- hidden files
- generated/build artifacts

The exact palette should remain configurable.

Later versions may optionally interpret `LS_COLORS`.

The renderer should not hard-code file classifications.

Color assignment should occur through a theme or semantic styling system.

---

# 19. Geometry and Shapes

Different semantic categories may use different basic geometry.

Examples:

Directory:
- platform
- frame
- container
- gateway-like structure

Individual file:
- simple plate
- prism
- marker
- tile

Series:
- segmented ribbon
- stack
- arc
- repeated structure

Cluster:
- grouped low-detail geometry

The program should use simple geometry rather than detailed 3D models.

Shape should complement color rather than rely on color alone.

---

# 20. Labels

Text must remain highly legible.

Labels should not attempt to display every filename at all times.

Label visibility may depend on:

- salience
- selection
- hover state
- focus
- distance
- zoom level

Important or selected files should receive clear labels.

Dense low-salience regions may use abbreviated labels or none until focused.

---

# 21. Interaction

The application should support both keyboard and mouse operation.

Important interactions should eventually include:

- navigate into directory
- navigate to parent
- select file
- focus file
- open file
- search
- filter
- orbit camera
- zoom
- reset camera
- expand group
- collapse group
- show file metadata
- open terminal at location
- reveal hidden items
- toggle visual summaries
- toggle conventional list view

Keyboard-first use should be practical.

Mouse interaction should remain intuitive.

---

# 22. Conventional UI

Dear ImGui or equivalent lightweight UI may be used for non-spatial interface elements.

Examples include:

- current path
- search box
- metadata inspector
- settings
- context menus
- status indicators
- navigation history
- shortcut hints
- filter controls

The conventional interface should complement the spatial scene rather than dominate it.

---

# 23. Search

Search should eventually integrate with the spatial visualization.

Possible behavior:

- matching files become brighter
- unrelated files become dimmer
- matching groups expand
- the camera can animate toward selected results
- the path to a result may be visually highlighted

Search should remain fast.

Initial implementation may use direct directory search before introducing broader indexing.

---

# 24. Navigation History

The navigator should maintain:

- previous locations
- forward locations
- recent locations
- optional bookmarks

History should integrate with animated navigation.

Returning to a previous location should ideally restore a sensible previous camera and layout state.

---

# 25. Responsiveness

The rendering loop must not be blocked by expensive filesystem operations.

Longer tasks should eventually run asynchronously.

Examples include:

- scanning very large directories
- classification
- series detection
- metadata gathering
- recursive size calculation
- indexing

The interface should remain interactive during background work.

---

# 26. Performance Principles

Important rules:

1. Do not recursively scan the entire filesystem at startup.
2. Do not create unnecessary rendering objects every frame.
3. Cache directory metadata.
4. Cache layout calculations where practical.
5. Rebuild layout only when needed.
6. Avoid expensive allocations inside the render loop.
7. Render only visible or relevant objects.
8. Use simple geometry.
9. Treat large directories explicitly.
10. Profile before introducing complicated optimizations.

---

# 27. Filesystem Safety

The application will eventually support file operations.

Destructive actions must be handled carefully.

Operations such as:

- delete
- move
- overwrite
- rename

should require clear confirmation when appropriate.

Symlink behavior must be explicit.

Filesystem errors should be treated as normal runtime conditions.

Examples include:

- permission denied
- file disappeared
- broken symlink
- unavailable mount
- unreadable directory

These should not crash the application.

---

# 28. Architecture

The project should maintain clear subsystem boundaries.

A likely structure is:

```text
src/
    app/
    filesystem/
    analysis/
    layout/
    graphics/
    navigation/
    ui/
```

Responsibilities:

## filesystem/

Reads real filesystem information.

Should not know about OpenGL or screen coordinates.

## analysis/

Computes:

- salience
- series detection
- grouping
- clustering
- semantic classification

Should not render anything.

## layout/

Turns analyzed filesystem data into spatial arrangements.

Produces information such as:

- position
- scale
- depth
- group bounds
- orientation

Should not perform filesystem traversal.

## graphics/

Draws the scene.

Should not contain filesystem heuristics.

## navigation/

Tracks:

- current directory
- focus
- selection
- history
- camera target
- navigation state

## ui/

Implements:

- overlays
- inspector
- search
- settings
- menus

---

# 29. Camera Architecture

Camera state should be separated from filesystem state.

Likely camera state includes:

- focal position
- position
- target
- yaw
- pitch
- distance
- field of view
- transition state

The camera should support both:

- direct user control
- programmatic animated transitions

Navigation code may request:

> focus this spatial object

The camera system should determine how to animate toward the corresponding target.

Filesystem logic should not directly manipulate camera matrices.

---

# 30. Animation Architecture

Animations should not be scattered throughout rendering code.

The project should use a small explicit transition system.

A transition may contain:

- start state
- target state
- duration
- easing function
- elapsed time

Possible animated properties include:

- camera position
- camera target
- yaw
- pitch
- zoom
- object position
- object scale
- object opacity
- group expansion

Prefer simple easing functions.

Avoid introducing a large animation framework.

---

# 31. Configuration

Later versions may expose configurable parameters such as:

- recency weighting
- clustering sensitivity
- orbit sensitivity
- animation duration
- field of view
- item size
- label density
- color theme
- maximum individually rendered items
- hidden-file treatment

Defaults should work well without configuration.

---

# 32. Adaptation and User Behavior

Long-term versions may adapt to actual user behavior.

Potential signals include:

- files opened frequently
- directories visited frequently
- recently selected items
- repeated search results
- commonly used projects

This should be treated as an optional future enhancement.

Early versions should use deterministic heuristics.

If user history is recorded, the implementation should be transparent and local.

---

# 33. Non-Goals

Initial versions are not intended to:

- replace every feature of mature desktop file managers
- replicate a full desktop shell
- render the entire filesystem simultaneously
- become a general-purpose 3D engine
- use machine learning unnecessarily
- depend on network services
- require cloud connectivity
- require a database server
- imitate the original FSN pixel-for-pixel

---

# 34. Milestone 1 — Rendering Foundation

Goal:

Produce the smallest runnable version of the visual interface.

Requirements:

1. Create the CMake project.
2. Initialize SDL2.
3. Create an OpenGL 3.3 Core context.
4. Support native X11 operation.
5. Create a resizable application window.
6. Implement a stable render loop.
7. Render a dark background.
8. Render a simple perspective grid.
9. Render several hard-coded mock filesystem objects.
10. Initialize Dear ImGui.
11. Display a minimal path/status overlay.
12. Implement zoom.
13. Implement limited yaw/pitch orbit around the current focal point.
14. Implement reset-view behavior.
15. Implement one simple animated camera transition between mock directory objects.
16. Handle resizing correctly.
17. Handle clean shutdown.
18. Document required Void Linux packages.
19. Build and run successfully.

Do not access the real filesystem yet.

The purpose of Milestone 1 is to validate:

- rendering architecture
- camera architecture
- orbit behavior
- animation behavior
- visual style

---

# 35. Milestone 2 — Mock Spatial Filesystem

Use an in-memory fake filesystem.

Add:

- mock directories
- mock files
- mock timestamps
- mock file types
- mock series
- mock large directory

Implement:

- spatial layout
- basic salience
- terminal-inspired colors
- directory focusing
- animated directory transitions
- file focusing
- group rendering

The purpose is to validate visualization and interaction before introducing real filesystem complexity.

---

# 36. Milestone 3 — Real Filesystem Integration

Introduce the real filesystem backend.

Implement:

- directory reading
- file metadata
- file types
- permissions
- symlinks
- hidden files
- error handling

Populate the existing spatial model from real directory contents.

Do not yet implement destructive file operations.

---

# 37. Milestone 4 — Salience and Grouping

Implement initial heuristics for:

- modification recency
- generated-file deweighting
- series detection
- temporal grouping
- directory density handling
- progressive summarization

Validate behavior using directories containing:

- source code
- images
- build outputs
- many files
- old and recent files

---

# 38. Milestone 5 — Practical File Navigation

Add:

- opening files
- search
- filtering
- path entry
- navigation history
- metadata inspector
- open terminal here
- conventional list view
- keyboard navigation

At this point the program should begin functioning as a practical file browser.

---

# 39. Milestone 6 — Refinement

Improve:

- layout stability
- performance
- animation
- camera feel
- grouping
- theme system
- large-directory behavior
- user configuration

Add profiling and stress testing.

---

# 40. Guiding Principle

The visual layout should express inferred importance and relationship rather than merely mapping directory contents one-to-one onto screen objects.

The scene should feel spatial, animated, and explorable while remaining useful as a real filesystem navigator.
