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
  form a 26-member sequence with a numeric token in the middle of each name
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

Series detection examines each maximal numeric token anywhere in the filename
stem. All text before and after the token, including the extension, must match.
Consecutive runs of at least three become one group with numerically ordered
members. Thus both `frame_001.png` and `older-note-0-draft.txt` patterns work.
Gaps split runs; duplicate ordinals and overflowing numbers are handled
conservatively. When several numeric tokens suggest overlapping sequences, the
longest run wins, then pattern name breaks ties. No entry belongs to two groups.
Directory names are never series members. Remaining related files must share an
underscore-delimited prefix, a semantic category, and a total modification-time
span of at most ten minutes. Time proximity alone does not imply a group.

## One persistent nested world

Each directory has a floor and backdrop. A child directory owns a smaller copy
of that environment containing its actual analyzed files, groups, and child
directories. `LocalTransform` composes uniform scale and translation through the
hierarchy. Layout coordinates are local to their containing directory; world
coordinates and dimensions inherit the parent transform. Entering a directory
never relocates, enlarges, or substitutes its layout.

The canonical floor is 16 by 16 local units. A child's local floor is fitted
inside its parent's directory footprint with a margin. Salience still controls
placement order, footprint, and brightness. Default pages contain at most 20
objects plus one summary of entries outside that page. Paging and summaries keep
low-interest entries accessible. Directory/group pages and child nodes are cached;
returning to a directory preserves the page already visible in its miniature.
Paging explicitly changes contents within that diorama, not its world transform.

`HierarchyConfig` builds three levels of preview below the current context and
caps nesting at six levels, including group/summary views. This covers the mock
directory tree and prevents recursive summary expansion without bound. At the
cap, use pages rather than expanding another summary. Deeper previews are added
when approached; already-existing layouts keep their transforms. This prototype
analyzes the small, entirely in-memory fixture once; it is not a real filesystem
scan or a plan to recursively scan real data at startup.

Geometry LOD uses world scale relative to camera distance. Each node shows at
most three, six, or its full bounded page of actual member objects. The same
objects retain their positions as more detail is disclosed. Selection and the
active ancestor path remain represented. Labels additionally suppress tiny
projected objects and collisions. The sidebar always identifies the selection.
Buffers update only on selection/context changes or LOD threshold crossings;
camera animation itself does not recompute layout or allocate a new scene every
frame. Parent and sibling context stay in the hierarchy, subject to ordinary
view-frustum clipping and LOD.

Camera entry targets the existing child transform and interpolates both focal
position and browsing distance. Returning to the parent reverses the scale
change. Near/far clipping and zoom limits use the current browsing scale so that
the same orbit/zoom controls work inside small dioramas. Yaw/pitch limits, fixed
roll, smoothstep interpolation, and transition priority over orbit are unchanged.

## Semantic shapes and compound sequences

The analysis layer maps semantic categories to a small `FileShape` vocabulary.
Presentation builds boxes/plates from that shape; the OpenGL renderer receives
geometry and colors and has no filename-extension rules.

| Semantic category | Geometry | Color |
| --- | --- | --- |
| Directory | Floor and backdrop containing smaller objects | Blue |
| Source and text/documents | Upright thin slab with a header stripe | Warm source / pale text |
| Image | Wide thin panel with a small foot | Magenta |
| Media | Two offset panels | Violet |
| Executable and binary | Compact heavier prism | Green / subdued green |
| Archive | Three stacked bands | Red |
| Generated output | Low-profile tile | Subdued gray |

Sequences act as apartment complexes: the actual members occupy a shared floor
under one roof frame with supporting posts. This gives them one compound
silhouette instead of loose nearby cubes. The visible modules use their semantic
file shapes. Coarse LOD shows fewer actual units and the full count; approaching
reveals more. Enter/expand zooms into that same structure and makes its members
selectable in numeric order, with filenames and metadata. Groups retain their
member IDs, transforms, and page ordering across expansion/return. Related and
summary groups use the same inexpensive shared enclosure. Selected objects have
a gold outline; terminal-inspired colors and basic salience remain in place.

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
transitions; zoom can interrupt them. Ordinary entry/parent navigation changes
the camera target within the persistent world, not the scene's origin. The
animation checkbox deliberately disables motion when requested. Roll is fixed;
there is no free flight.

## Manual visual verification for revised Milestone 2

Builds and logic tests cannot establish the appearance or feel of the revised
spatial model. Run `./build/spatial-navigator` under interactive X11 and inspect:

1. At `/mock`, inspect `projects`: it should visibly contain files and miniature
   directory environments. Enter `projects`, `engine`, then `src` and return one
   level at a time. Each step should enlarge existing objects or zoom outward,
   without the previous scene disappearing or child objects changing position.
2. Repeat through `media/renders/take01`. Inspect a nested directory before
   activating it and watch the same contents become clearer on approach. Check
   that coarse details and labels appear without layout movement, and that
   orbit/reset still feel useful at every scale.
3. At the root, inspect `older-note-*-draft.txt (26)` as one shared complex, not
   unrelated files. Expand it; select individual notes and use the next page to
   reach note 20 onward. In `take01`, do the same with the 20-frame image complex.
   Individual member names, ordering, metadata, and parent return should work.
4. Compare document slabs, image panels, paired media panels, compact binary
   prisms, banded archives, and low generated tiles. Check that shape distinctions
   work from oblique angles and that semantic colors remain legible.
5. Inspect the recent session group in `documents`; the old session note remains
   separate. In `dense-240`, inspect bounded pages, summaries, and the return to
   the previous page. Low-salience generated files must remain accessible.
6. Test scene/sidebar selection and double-clicking during focus animation,
   orbit priority, zoom limits at deep levels, empty-directory parent return,
   animation disable, and resize down to 720x480. Check label clutter, picking
   alignment, clipping, and camera motion in particular.

The execution environment cannot currently connect to X11. These visual checks
are pending user verification; the revision is not considered visually validated
merely because automated builds/tests pass.

## Structure

- `filesystem/`: plain entry metadata and fixed in-memory tree construction
- `analysis/`: semantic classification, configurable basic salience, series and related groups
- `layout/`: parent-relative transforms, persistent nested hierarchy, page bounds, and summaries
- `navigation/`: directory/group context, selection, paging, and parent context stack
- `graphics/`: geometry buffers, shaders, projection/picking math, camera transitions
- `ui/`: semantic palette, geometry presentation, spatial labels, conventional controls
- `app/`: persistent-world orchestration, semantic camera requests, SDL/OpenGL lifetime, event/render loop

No new dependencies were introduced. Real filesystem integration and all other
Milestone 3 work remain deferred. See `DESIGN.md` for product scope.
