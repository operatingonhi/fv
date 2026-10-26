# Spatial Filesystem Navigator

## 1. Project Summary

This project is a lightweight native Linux filesystem navigator inspired by the visual style and spatial interaction of the filesystem navigator shown in *Jurassic Park*.

The goal is not to recreate the original program exactly. Instead, the application should reinterpret the concept as a modern, useful filesystem browser with:

- a retro-futuristic spatial aesthetic
- a primarily 2.5D visual layout
- lightweight native rendering
- strong keyboard and mouse interaction
- intelligent visual prioritization of files and directories
- nested spatial representation of filesystem hierarchy
- animated spatial navigation without abrupt scene replacement
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

The filesystem should be presented as a spatial information environment in which visual prominence represents likely user relevance and filesystem hierarchy is expressed through nested spatial structure.

The central concepts are:

> The spatial layout should help direct the user's attention toward files and directories they are most likely to want.

and:

> Directories are nested places within one continuous spatial world, not separate unrelated scenes.

The visualization should communicate:

- hierarchy
- relevance
- recency
- relationship
- density
- file type
- grouping
- containment

The filesystem remains the underlying data model, but directory order alone should not determine visual prominence.

---

# 4. Spatial Metaphor

A useful conceptual analogy is:

- directories are cities or districts
- ordinary individual files are houses or buildings
- sequential or strongly related file groups are apartment complexes or other compound buildings
- nested subdirectories are smaller cities or districts contained within larger ones
- the camera moves through this hierarchy rather than loading unrelated replacement scenes

This analogy is conceptual rather than literal.

The renderer does not need to depict realistic buildings, roads, or cities.

The important idea is that hierarchy and grouping should be visible spatially.

A subdirectory should look like a contained place whose contents already exist within it.

A file series should look like one cohesive compound structure containing multiple members rather than a row of unrelated identical objects.

---

# 5. Visual Style

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

# 6. 2.5D Spatial Model

The normal interface should be fundamentally 2.5D rather than a freely navigable three-dimensional world.

Most filesystem objects should occupy positions in a predominantly planar or layered spatial arrangement.

Depth should be used to:

- distinguish hierarchy
- visually separate clusters
- establish foreground and background
- show relationships
- improve legibility
- support perspective effects
- show containment

The user should not need to navigate through a fully three-dimensional environment in order to use the application.

Filesystem navigation must remain simple and predictable.

---

# 7. Nested Directory Dioramas

Directories must not be represented only as opaque nodes that are replaced by another scene when entered.

A directory should contain a visible spatial representation of at least some of its contents.

If the current directory is represented by a floor plane and backdrop plane, a subdirectory may use a smaller version of that same spatial form.

Its child files and child directories should be positioned within that subdirectory's visible region.

The result should resemble a nested diorama.

For example:

```text
current directory
|
+-- file
|
+-- file
|
+-- subdirectory
    |
    +-- file
    |
    +-- file series
    |
    +-- nested subdirectory
        |
        +-- file
```

The graphical scene should similarly contain:

```text
large directory environment
    ordinary file object
    ordinary file object

    smaller subdirectory environment
        smaller file object
        compound file-series structure

        even smaller nested directory environment
            smaller file object
```

The nested content does not need to remain readable at every scale.

From an upstream directory:

- child files may be tiny
- labels may disappear
- detailed geometry may simplify
- low-salience objects may become abstract
- nested contents may be represented at reduced detail

However, the contents should still visibly exist.

Entering the subdirectory should reveal and enlarge the content that was already visible rather than replacing the whole scene with newly appearing objects.

This continuity is a core requirement.

---

# 8. Hierarchical Scene Continuity

Filesystem hierarchy should be represented as a continuous nested spatial structure.

The application should avoid the conceptual model:

```text
directory A scene
    -> destroy/hide scene
directory B scene
```

when B is a child of A.

The preferred conceptual model is:

```text
directory A
    contains directory B
        contains directory C
```

and navigation changes what portion of this existing hierarchy occupies the viewport.

Scene construction, layout, and camera systems should therefore support nested coordinate systems or equivalent hierarchical transforms.

A child's layout should be spatially relative to its parent.

This requirement should influence the architecture even if early implementations render only a limited hierarchy depth for performance.

A newly opened directory should also have its parent and sibling context, even
when that parent has not previously been visited. Load one parent listing and
bounded, unnamed sibling-content previews asynchronously. Align this parent
backdrop around the current directory's existing coordinates; its arrival must
not reposition current objects or move the camera. Dense parent listings retain
groups and remainder summaries, with the current directory represented
individually on the chosen parent page. The filesystem root has no such backdrop;
an unreadable parent must leave the current directory usable.

Only the current directory, its descendants at the configured detail level, and
its immediate parent/siblings need to be drawn. Do not draw grandparent objects.
Crossing a cached scene boundary during ordinary directory navigation must retain
a continuous zoom. Rebase cached metadata/layouts into a local browsing frame
before small scales lose precision; map the displayed camera pose into that
frame without a screen-space jump. An unseen parent may load asynchronously,
keeping the current view until its spatial context can be used for the ascent.



---

# 9. Level of Detail for Nested Content

Nested visualization must not require full-detail rendering of the entire filesystem.

Use progressive levels of detail.

Possible behavior:

### Far away

A subdirectory may show:

- its containing geometry
- a few major internal shapes
- strong file groups
- highly salient items
- no labels

### Medium distance

Show:

- more internal files
- recognizable file-type geometry
- major groups
- selected labels

### Focused or entered

Show:

- individual important files
- readable labels
- detailed groups
- subdirectory contents
- relevant metadata

Nested content should therefore become progressively clearer as the camera approaches it.

The immediate subdirectories shown on the current page must have their contents
represented, including their files and child directories. Their displayed pages
remain visible even at distant LOD. Large contents may use compact compound groups
and navigable remainder summaries to preserve the bounded footprint. Enumerate
these immediate directories asynchronously without recursively scanning deeper
levels. Distinguish pending/unavailable contents from confirmed empty directories.
Nested filenames need not be displayed.

This should feel like zooming into an existing world.

It should not feel like waiting for a separate scene to load.

---

# 10. Camera and Perspective

Although the interface is primarily 2.5D, the scene should still be rendered through a perspective-capable camera.

The user should be able to orbit around the current focal location.

Orbit behavior may change:

- yaw
- pitch
- limited viewing distance
- perspective angle

The normal working view should remain relatively shallow and easy to understand.

Orbiting should allow the user to inspect the depth and nested structure of the current filesystem scene without transforming the program into a conventional 3D game or free-flight environment.

The camera should provide an easy way to return to the standard useful viewing orientation.

Possible controls may include:

- mouse drag
- modifier + mouse drag
- keyboard shortcuts
- reset-view command

Roll should normally remain fixed.

The camera should avoid disorienting rotations.

---

# 11. Continuous Camera Navigation

Navigation through filesystem hierarchy should primarily be expressed as camera motion through the nested scene.

Entering a child directory should normally cause the camera to move toward and into the spatial region representing that directory.

It should not normally cause an abrupt cut.

The desired sequence is approximately:

1. the selected subdirectory becomes the camera target
2. the camera moves toward it
3. the subdirectory grows naturally in apparent size because the camera is approaching it
4. its previously visible internal objects become clearer
5. the camera enters or settles within that directory's browsing scale
6. labels and higher-detail geometry progressively become visible

Likewise, navigating to the parent directory should feel like moving outward from the current diorama and revealing its surrounding parent environment.

This creates spatial continuity.

The user should gain an intuitive sense that:

> "I was inside this part of that larger directory."

rather than:

> "The application replaced one screen with another."

---

# 12. Camera Cuts

Abrupt camera cuts should be avoided for ordinary hierarchical navigation.

Do not use cuts for:

- entering a child directory
- leaving for the parent directory
- focusing a nearby file
- opening a grouped series
- focusing a nearby subdirectory

A cut may be appropriate for exceptional cases such as:

- jumping to an unrelated absolute path
- following a search result in a distant filesystem location
- restoring a saved location when continuity cannot reasonably be shown

Even in these cases, a short transition may be preferable.

Ordinary parent-child navigation must use continuous motion.

---

# 13. Motion and Animated Navigation

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

Animations should make the spatial relationship between old and new context understandable.

For entering a subdirectory, the principal animation should be a spatial zoom or camera travel into that directory's already-existing graphical representation.

For leaving it, the animation should reverse that concept and reveal the parent context.

Focusing a particular file should generally use a shorter camera translation and scale adjustment that centers or emphasizes the selected object while preserving nearby context.

Animations should be:

- smooth
- relatively short
- interruptible when practical
- deterministic
- not unnecessarily cinematic

Responsiveness should take priority over spectacle.

The user should eventually be able to disable or reduce animations if desired.

---

# 14. Filesystem Data Model

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

The model should naturally support parent-child hierarchy.

---

# 15. Salience

Each file and directory should eventually receive a calculated salience score.

Salience represents the estimated probability that an item is relevant to the user's current attention or likely next action.

Salience may influence:

- position relative to the visual focal region
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
- frequency of user interaction

The architecture must allow the scoring model to evolve without changing rendering code.

---

# 16. Recency

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

# 17. File Type Geometry

Files must not all use the same basic cube geometry.

Color is one visual channel for file classification, but geometry should provide a second independent visual channel.

Different semantic file categories should use distinct but visually compatible shapes.

Possible categories include:

- source code
- plain text / notes
- images
- executable binaries
- libraries
- archives
- configuration files
- documents
- media
- symlinks
- generated files
- miscellaneous files

The exact mapping should be refined through visual testing.

Example conceptual mappings might include:

- source/text: thin upright slab or document-like plate
- image: wider rectangular panel
- executable: compact heavier prism
- archive: stacked or banded block
- configuration: narrow technical marker
- symlink: linked or offset shape
- generated/build artifact: low-profile subdued object

These are suggestions, not mandatory final shapes.

The essential requirement is:

> Different kinds of files should be distinguishable by shape as well as color.

Shapes should remain simple and cheap to render.

Do not create realistic icons or detailed models.

---

# 18. Shape Semantics

Geometry should communicate purpose without overwhelming the scene.

Shape categories should:

- remain recognizable from oblique viewing angles
- remain distinguishable when rendered small
- work with wireframe or minimal solid rendering
- not depend solely on labels
- complement terminal-inspired color

The semantic classification system should determine the shape category.

The renderer should not contain file-extension heuristics.

---

# 19. File Series Detection

The navigator should detect apparent file series.

Examples include:

```text
img1.png
img2.png
img3.png
```

```text
frame_001.png
frame_002.png
frame_003.png
```

```text
older-note-0-draft.txt
older-note-1-draft.txt
older-note-2-draft.txt
```

Series detection must be based on the structure of the filename, not only a numeric suffix at the end of the full filename.

For example, these should be recognized as a series:

```text
older-note-0-draft.txt
older-note-1-draft.txt
older-note-2-draft.txt
```

because the differing numeric token occurs within an otherwise common filename pattern.

Possible indicators include:

- common filename prefix
- common filename suffix
- common extension
- one varying numeric or alphabetic token
- sequential values
- close timestamps
- similar sizes

The detection algorithm should initially favor deterministic transparent rules.

---

# 20. File Series Must Form Compound Structures

A detected file series should not merely be rendered as multiple independent file objects placed near one another.

It should form a single visually cohesive compound structure.

Use the analogy:

> An individual file is a house.  
> A file series is an apartment complex.

The group should visibly read as one structure containing multiple members.

Possible compound forms include:

- repeated connected modules
- segmented towers
- joined plates
- stepped terraces
- rows sharing a common base
- arcs sharing structural supports
- ribbon-like complexes

The exact design may evolve through experimentation.

The key requirements are:

- the group has an overall silhouette
- the individual members remain visually represented
- the members visibly belong to the same structure
- the group consumes less visual clutter than unrelated individual objects
- the group can expand or focus to expose individual members

Simply assigning all members the same color or placing them close together is not sufficient.

---

# 21. Sequential Group Member Representation

Individual members of a series should still be addressable.

A compound series structure should preserve:

- member count
- ordering
- individual selection
- individual filename
- individual metadata

At normal viewing distance, the structure may act as one object.

When focused or expanded, individual members should become selectable.

This should resemble approaching an apartment building and then distinguishing individual units.

---

# 22. Temporal Grouping

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

Temporal groups should eventually be visually cohesive, though not necessarily as strongly structured as explicit sequential file series.

---

# 23. Structural and Functional Grouping

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

# 24. Group Visualization

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
- repeated modules within a shared superstructure

Grouping should reduce clutter without making individual files inaccessible.

A group should support expansion and collapse.

For explicit filename sequences, a compound structural representation is preferred over loose clustering.

---

# 25. Large Directory Handling

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
- several detected series as compound structures
- one cluster of older files
- one cluster of build artifacts
- one cluster of miscellaneous low-salience files

The layout engine should have explicit density limits.

Nested directory visualization must respect these limits so that upstream views do not become excessively dense.

---

# 26. Progressive Disclosure

The amount of detail shown should depend on user focus and camera scale.

An unfocused directory may be represented compactly while still showing a reduced representation of its contents.

A focused directory may reveal:

- additional child files
- labels
- metadata
- detected groups
- internal structure

Entering the directory should reveal greater detail through spatial zoom and level-of-detail changes.

This should reduce visual overload while maintaining continuity.

---

# 27. Low-Interest and Generated Files

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

# 28. Terminal-Inspired File Colors

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

# 29. Geometry and Shapes

Different semantic categories should use different simple geometry.

Examples:

Directory:
- floor plane plus backdrop
- platform
- frame
- container
- gateway-like region

Individual file:
- slab
- prism
- plate
- tile
- marker

Sequential series:
- compound structure
- segmented tower
- joined stack
- shared-base complex
- repeated connected modules

Other cluster:
- grouped low-detail geometry

The program should use simple geometry rather than detailed 3D models.

Shape should complement color rather than rely on color alone.

---

# 30. Nested Layout

Layout must support parent-relative coordinate systems or equivalent hierarchical transforms.

A subdirectory's position and scale should be defined within the coordinate space of its parent.

Its contents should then be positioned within the subdirectory's own local space.

For example:

```text
root local coordinates
    projects directory transform
        project-A directory transform
            source.cpp transform
            notes.txt transform
```

This hierarchical representation is preferred because it naturally supports:

- nested dioramas
- camera zoom into children
- zoom back to parents
- scalable depth
- consistent animation
- level-of-detail control

The implementation need not literally use a traditional scene graph if another architecture is cleaner, but the behavior should be equivalent.

---

# 31. Labels

Text must remain highly legible.

The current directory's name and every object name on its current visible page
must remain displayed, independently of zoom, salience, selection, search/filter
dimming, and nested detail settings. Move colliding current-page labels
instead of silently suppressing them. Full long names remain available in the
sidebar/inspector when a scene label must be abbreviated.

Scene names are restricted to the current directory and its immediate visible
entries. Files, directories, groups, and summaries inside child or sibling
directories keep their geometry without names, even at full detail or during
search highlighting. Entering a directory reveals its current-page entry names.
The current-directory heading is independent of spatial projection and must
remain visible at startup, when ascending, and at `/`.

Label placement and emphasis may depend on:

- salience
- selection
- hover state
- focus
- distance
- zoom level
- nesting depth

Important or selected files should receive clear labels.

Dense low-salience regions may use abbreviated labels or none until focused.

Nested files remain visible as geometry without names until their directory is entered.

That is expected behavior.

---

# 32. Interaction

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

Keyboard-first use should be practical.

Mouse interaction should remain intuitive.

Double-clicking or activating a subdirectory should initiate the continuous zoom/navigation transition into the nested directory representation.

---

# 33. Conventional UI

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

# 34. Search

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

# 35. Navigation History

The navigator should maintain:

- previous locations
- forward locations
- recent locations
- optional bookmarks

History should integrate with animated spatial navigation.

Returning to a previous location should ideally restore a sensible previous camera and layout state.

---

# 36. Responsiveness

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

# 37. Performance Principles

Important rules:

1. Do not recursively scan the entire filesystem at startup.
2. Do not create unnecessary rendering objects every frame.
3. Cache directory metadata.
4. Cache layout calculations where practical.
5. Rebuild layout only when needed.
6. Avoid expensive allocations inside the render loop.
7. Render only relevant detail.
8. Use simple geometry.
9. Treat large directories explicitly.
10. Use level-of-detail simplification for nested scenes.
11. Do not assume every visible nested file requires full geometry or a label.
12. Profile before introducing complicated optimizations.

---

# 38. Filesystem Safety

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

# 39. Architecture

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

Turns analyzed filesystem data into nested spatial arrangements.

Produces information such as:

- parent-relative position
- scale
- depth
- group bounds
- orientation
- nested transforms

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

# 40. Camera Architecture

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

or:

> enter this child directory

The camera system should determine how to animate toward the corresponding target.

Filesystem logic should not directly manipulate camera matrices.

---

# 41. Animation Architecture

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
- level-of-detail transition

Prefer simple easing functions.

Avoid introducing a large animation framework.

Parent-child navigation must use this transition system rather than abruptly replacing the visible scene.

---

# 42. Configuration

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
- nested rendering depth
- level-of-detail thresholds

Defaults should work well without configuration.

---

# 43. Adaptation and User Behavior

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

# 44. Non-Goals

Initial versions are not intended to:

- replace every feature of mature desktop file managers
- replicate a full desktop shell
- render the entire filesystem at full detail simultaneously
- become a general-purpose 3D engine
- use machine learning unnecessarily
- depend on network services
- require cloud connectivity
- require a database server
- imitate the original FSN pixel-for-pixel
- use abrupt scene replacement as the normal model for directory navigation

---

# 45. Milestone 1 — Rendering Foundation

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
15. Implement one simple animated camera transition.
16. Handle resizing correctly.
17. Handle clean shutdown.
18. Document required Void Linux packages.
19. Build and run successfully.

Do not access the real filesystem yet.

---

# 46. Milestone 2 — Mock Spatial Filesystem

Use an in-memory fake filesystem.

The purpose of Milestone 2 is to validate the spatial metaphor before adding real filesystem complexity.

The mock filesystem should include:

- multiple nested directory levels
- ordinary standalone files
- several semantic file types
- recent and old files
- generated or low-salience files
- filename sequences
- temporal groups
- at least one dense directory

Milestone 2 should demonstrate:

## Nested directory dioramas

Subdirectories should visibly contain representations of their children.

At least one subdirectory should contain another visible subdirectory.

The user should be able to see reduced representations of nested contents from the parent view.

## Continuous directory navigation

Entering a child directory must zoom or move into the already-visible nested representation.

Do not replace the parent scene with an abrupt cut.

Navigating back to the parent should zoom outward and restore the larger context.

## File-type geometry

Files of different semantic categories should use visibly different simple geometric forms in addition to their different colors.

Do not render every standalone file as the same cube.

## Sequential file compound structures

Filename sequences must be detected and rendered as cohesive compound structures.

For example:

```text
older-note-0-draft.txt
older-note-1-draft.txt
older-note-2-draft.txt
```

should appear as members of one compound structure rather than unrelated cubes.

## Salience

Implement basic deterministic salience.

## Terminal-inspired colors

Retain semantic terminal-inspired coloring.

## Grouping

Implement initial grouping without introducing complex machine learning or opaque heuristics.

Milestone 2 is not complete if the mock filesystem exists but these visual and navigation behaviors are absent.

---

# 47. Milestone 3 — Real Filesystem Integration

Introduce the real filesystem backend.

Implement:

- directory reading
- file metadata
- file types
- permissions
- symlinks
- hidden files
- error handling

Populate the existing nested spatial model from real directory contents.

Do not yet implement destructive file operations.

The real filesystem implementation should preserve:

- nested directory dioramas
- continuous parent-child camera navigation
- series grouping
- semantic shapes
- salience-based layout

---

# 48. Milestone 4 — Salience and Grouping

Refine heuristics for:

- modification recency
- generated-file deweighting
- series detection
- embedded numeric sequence detection
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

# 49. Milestone 5 — Practical File Navigation

Add:

- opening files
- search
- filtering
- path entry
- navigation history
- metadata inspector
- open terminal here
- keyboard navigation

At this point the program should begin functioning as a practical file browser.

---

# 50. Milestone 6 — Refinement

Improve:

- nested layout stability
- hierarchical level-of-detail
- performance
- animation
- camera feel
- semantic geometry
- compound sequence structures
- grouping
- theme system
- large-directory behavior
- user configuration

Add profiling and stress testing.

---

# 51. Guiding Principles

The visual layout should express inferred importance and relationship rather than merely mapping directory contents one-to-one onto screen objects.

Hierarchy should be spatially continuous.

A directory should feel like a place containing other places and objects.

Entering a child directory should feel like moving into an existing part of the scene.

Different kinds of files should be recognizable by geometry as well as color.

Sequential files should behave visually like units within one compound structure rather than unrelated standalone objects.

The scene should feel spatial, animated, nested, and explorable while remaining useful as a real filesystem navigator.
