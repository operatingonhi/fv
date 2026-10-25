# Spatial filesystem navigator

Milestone 2: an in-memory mock filesystem visualized through the native X11,
SDL2 / OpenGL 3.3 Core foundation accepted in Milestone 1. Filesystem data,
analysis, layout, presentation, rendering, and navigation remain separate.
There is no real filesystem backend, scanning, file opening, or file operation.

## Dependencies on Void Linux

Provide these packages when building the development container image:

- `base-devel`: C++20 compiler, linker, and build tools
- `cmake`: project configuration and build
- `SDL2-devel`: window creation, X11 input, and OpenGL context creation
- `MesaLib-devel` and `libglvnd-devel`: desktop OpenGL headers and libraries

Running also requires access to an X11 server (`DISPLAY`, its socket and any
required authorization) and a working OpenGL 3.3 driver. The application requires
SDL's native `x11` backend; it does not fall back to Wayland or a headless backend.
No packages are installed by the build.

Dear ImGui is compiled directly from the existing Git submodule, including the
standard SDL2 and OpenGL3 backends. Initialize it after cloning:

```sh
git submodule update --init third_party/imgui
```

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/spatial-navigator
```

The two test executables require no window or OpenGL context. They cover camera
limits and animation, mock hierarchy, salience, semantic classification, numeric
series detection, timestamp grouping, layout bounds and density, entry coverage,
nested navigation, group expansion, file focusing, presentation, and picking.

With an accessible X11 display, the optional finite smoke run enters a mock
directory, animates focus, orbits, zooms, resizes, returns to the parent, resets,
checks scene pixels and OpenGL errors, and quits through SDL's event queue:

```sh
./build/spatial-navigator --smoke-test
# Optional explicit screenshot output; never used as filesystem input:
./build/spatial-navigator --smoke-test /tmp/spatial-milestone2.ppm
```

Milestone 2 Debug and Release builds and both logic suites are checked during
implementation. The execution environment cannot connect to X11, so GUI behavior
requires manual visual validation; this is separate from build/test success.
The existing container lacks the ASan runtime, so sanitizer testing is unavailable.

## Mock data and deterministic rules

The tree uses fixed timestamps relative to `mock_now`, not the host clock:

- `/mock`: several directories, mixed semantic file types, and older notes to
  exercise overflow immediately
- `/mock/projects/engine/src` and `include`: nested source/header files
- `/mock/documents`: recent `session_*` notes, an old same-prefix note, and
  unrelated documents/images
- `/mock/media/renders/take01`: `frame_001.png` through `frame_020.png`
- `/mock/build`: recent objects, libraries, cache, and binary output
- `/mock/dense-240`: 240 files mixing recent generated output with older notes
- `/mock/empty-directory`: empty-context behavior

Basic salience is a positive baseline plus exponentially decaying modification
recency, with a seven-day half-life. Directories retain a navigation baseline.
Generated output, binaries, executables, archives, and hidden names receive
explicit weight reductions. The small `AnalysisConfig` isolates these prototype
choices; this is not the final scoring model.

Series detection uses an exact filename prefix/extension and a trailing integer.
Consecutive runs of at least three become one group with numerically ordered
members. Gaps split runs; duplicate ordinals and overflowing numbers are handled
conservatively. Directory names are never series members. Remaining related files
must share an underscore-delimited prefix, a semantic category, and a total
modification-time span of at most ten minutes. Series detection takes precedence;
time proximity alone does not imply a group.

Analyzed objects are sorted by salience, then name. Layout places prominent
objects at the center/front and increases their size and brightness. Default
pages contain at most 20 objects plus one summary of entries outside that page.
`LayoutConfig` holds the page/spacing limits. Paging or opening the summary keeps
all low-interest files reachable. Expanded groups expose their members without
regrouping; series retain numeric order.

Directory platforms/gateways, individual prisms, series ribbons, and shared
stack platforms use cached wireframe geometry. Series show up to six sample
segments and the full member count. Color semantics are blue directories,
warm source files, pale documents, magenta images, violet media, red archives,
green executables, and subdued binaries/generated output. Selected objects have
a gold outline. Spatial labels prioritize selection, prominent entries, and
groups; overlapping labels are suppressed. The sidebar always identifies the
selected entry. Layout/geometry are rebuilt on context or selection changes,
not on every animation frame.

## Controls

- Click a scene object or sidebar row: select and animate focus
- Double-click: enter a directory or expand a group; files receive focus only
- Tab: focus the next visible object; F: focus the selection
- Enter or Enter / expand: enter the selected directory/group
- Backspace or Parent: return to the enclosing context and restore its page/selection
- Page Up / Page Down or sidebar arrows: browse a dense context
- R or Reset: return to the current context's overview
- Right-drag in the scene: bounded orbit; wheel: bounded zoom
- Animate navigation checkbox: enable/disable subsequent transitions
- Escape or window close: quit

Transitions last 0.4 seconds with smoothstep easing. Orbit input is ignored during
transitions; zoom can interrupt them. Entering a context anchors its new layout
at the previous object's position while the camera moves toward its overview.
Group expansion uses the same camera transition; layout changes themselves are
instantaneous in this milestone. Roll is fixed; there is no free flight.

## Manual visual verification

Run `./build/spatial-navigator` in the interactive X11 environment and check:

1. Recent files appear larger/brighter and nearer the center/front; generated
   files in `build` remain visible with subdued styling.
2. Directory gateways and the different semantic colors remain distinguishable.
3. Enter `media`, `renders`, then `take01`: the 20-frame series is one cohesive
   ribbon with a count. Expand it, focus individual frames, then return.
4. In `documents`, the three recent session notes share one platform; the old
   session note and unrelated files remain separate. Expand and return.
5. Enter `dense-240`: pages remain uncluttered, later pages are reachable, and
   opening Other entries exposes the remainder. Parent restores the prior page.
6. Focus files and directories using both the sidebar and scene. Check double-click
   behavior during focus animation, nested/parent transitions, orbit priority,
   reset, and reduced animation mode.
7. Resize down to the 720x480 minimum and back, checking label readability,
   sidebar access, selection highlighting, and mouse picking alignment. Enter
   the empty directory and confirm Parent still works.

## Structure

- `filesystem/`: plain entry metadata and fixed in-memory tree construction
- `analysis/`: semantic classification, configurable basic salience, series and related groups
- `layout/`: deterministic placement, scale, page bounds, and remainder summaries
- `navigation/`: directory/group context, selection, paging, and parent context stack
- `graphics/`: geometry buffers, shaders, projection/picking math, camera transitions
- `ui/`: semantic palette, geometry presentation, spatial labels, conventional controls
- `app/`: orchestration, context anchors, SDL/OpenGL lifetime, event/render loop

No new dependencies were introduced. Real filesystem integration and all other
Milestone 3 work remain deferred. See `DESIGN.md` for product scope.
