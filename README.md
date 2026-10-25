# Spatial filesystem navigator

Milestone 4 refines deterministic salience, classification, grouping, density
handling, and progressive disclosure on the accepted real-filesystem integration.
The nested dioramas, camera/orbit/zoom behavior, semantic geometry/colors, loading
policy, and controls remain intact. No Milestone 5 functionality is implemented.

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

## Build, test, and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/spatial-navigator                 # current working directory
./build/spatial-navigator ~/projects      # optional starting path
```

The selected path is the root of this spatial world. Parent returns through
entered directories/groups within that tree; navigating above the starting path
is not implemented. Start with a higher directory if a larger enclosing context
is needed. A nonexistent, unreadable, or non-directory start displays an error
status rather than terminating the application. Shell expansion handles `~`.
An explicitly supplied starting directory may itself be a symlink.

An optional GUI smoke test waits for initial filesystem loading, then exercises
camera motion, directory entry/return, resize, rendering, and normal SDL shutdown:

```sh
./build/spatial-navigator ~/projects --smoke-test
./build/spatial-navigator ~/projects --smoke-test /tmp/spatial-milestone3.ppm
```

The optional capture is explicit screenshot output, not filesystem input. This
execution environment cannot connect to X11; GUI behavior requires manual
verification separately from build/test success.

## Filesystem loading and metadata

`filesystem/model` is a main-thread metadata cache with stable parent/child IDs.
`filesystem/reader` performs one-directory enumeration using non-throwing
`std::filesystem` operations. One `std::jthread` handles reads; immutable results
are merged on the main thread. No filesystem traversal runs in layout, rendering,
or the per-frame camera code. The directory reader is independent of the GUI and
can be exercised directly in tests.

The policy is deliberately bounded and explicit:

- Fully enumerate the starting/current directory on the worker.
- Preview at most eight directory objects on the current page, reading at most
  64 entries from each. The preview is an iterator-order sample, sorted before
  analysis; it is not a full recursive scan or a globally ranked sample.
- Focusing another directory can request its preview. Entering one prioritizes a
  full read and then requests previews of its own immediate children.
- Grandchildren are not read recursively at startup. They load progressively
  when their parent becomes active. Visited metadata and pages are cached.
- `LoadPolicy` isolates the preview limits; `HierarchyConfig` retains the existing
  three-level visual preview and six-level total geometry nesting cap, counting
  groups/summaries. Deeper visual rebasing and navigation beyond that cap remain
  future work. The visual preview depth never causes filesystem traversal.

Directory floors and world transforms exist before their contents finish
loading. Results populate those same dioramas. Existing objects keep their
anchors; newly disclosed objects use free layout slots. On partial-to-full
promotion, known ordering is kept before newly discovered objects to avoid
shifting the scene; this can differ from a fresh global salience sort. Series
and temporal grouping still run on the available metadata. A newly completed
sequence can become a compound structure as more members are discovered.

If a complete update would regroup an enclosing directory while the user is
inside one of its groups or descendants, the result waits until return. The
status explicitly says to return to the parent to apply ready updates. This
preserves the active nested context. Ordinary parent-child navigation changes
camera focus and distance rather than replacing the spatial world.

Regular files expose byte size, modification time, permission bits, executable
permission status, hidden status, and an absolute path. Directories expose their
own metadata, not recursively calculated subtree size. Modification times use
system-clock seconds for the existing continuous salience function. Hidden files
are included by default and retain the existing salience reduction. Permission
bits describe metadata; executable status does not launch or probe the file.

Symlinks have their own kind, cyan color, linked geometry, and target text. Broken,
cyclic, or inaccessible targets retain a selectable link object with an error.
Discovered links are not traversed, including directory links, so they cannot
create traversal cycles. Link size/mtime are left unavailable rather than
presenting target metadata as link metadata. Special files are represented with
a distinct semantic category and never opened for content.

Directory and per-entry errors appear in the sidebar, and unavailable entries
are marked in its list. Missing entries during a read are retained as error
records or marked as disappeared when a preview is completed. Permission errors
are not silently presented as empty successful directories. There is no live
watcher or automatic refresh; restart to re-read an already completed snapshot.
Re-entering an errored child retries its enumeration.

Shutdown cancels queued reads, cooperatively stops enumeration between entries,
and joins the worker before the cache is destroyed. A filesystem syscall already
blocked in the kernel (for example on an unresponsive network mount) cannot be
interrupted by `std::stop_token`; it can delay worker shutdown until the OS returns.
The render loop does not wait for that syscall during normal browsing.

## Milestone 4 analysis rules

All scoring/classification/grouping lives in `src/analysis/analysis.*` and uses
cached metadata only. `AnalysisConfig` isolates the main weights and thresholds;
`salience_breakdown` exposes the numeric components for tests. No ML, content
inspection, recursive statistics, or persistent interaction tracking is used.

**Salience:** a semantic baseline (source .60, text .56, image .52, media .48,
directory .62, archive .38, executable/link .42, binary .32, generated .25,
special .20) plus at most .24 modification-recency bonus. Recency decays
continuously with a seven-day half-life; unknown timestamps get no bonus and
future timestamps are clamped. The result is multiplied by explicit artifact,
executable/binary, and hidden modifiers, then bounded to [.02, 1]. This keeps an
old source file above freshly generated output rather than letting mtime decide
everything. Hidden files normally use .65; useful dot configurations such as
`.gitignore` and `.editorconfig` retain .90. No entry is excluded by scoring.

Directory counts contribute at most .06, using only already-cached children.
A complete cached directory with at least four entries and at least 70% likely
artifacts gets a .70 multiplier. Partial previews do not justify that inference.
Recognized cache/dependency directories use .55 but retain their directory shape
and navigability. These characteristics update as previews finish; transforms
remain pinned. Group scores blend .65 of the mean and .35 of the peak member
score, plus a membership bonus capped at .04. One recent member or a huge member
count cannot turn a low-interest group into the most important structure.

**Artifacts:** object files, normal/versioned shared libraries, static libraries,
Python bytecode, compiler dependency sidecars, Ninja/CMake cache outputs, explicit
minified/generated sources, source maps, swap files, and temporary/tilde-backup names
have specific recognizable rules. Standard cache path components (`CMakeFiles`,
`__pycache__`, `.cache`, `.pytest_cache`, `.git`, `node_modules`) provide additional
evidence. Ordinary `build`, `out`, or `dist` directory names do not automatically
mark all their contents generated. Ambiguous `.obj`, `.lib`, `.pdb`, and plain
`.d` require build context; meshes, D source, ordinary logs/maps/data, and Makefiles
are not blanket artifacts. Generated files use a .22 multiplier, binaries .55,
and executables .85. Eight or more leftover artifact files become one expandable
Build artifacts group; smaller sets remain individual, and detected numbered
artifact sequences retain their compound series representation.

**Series:** maximal numeric tokens anywhere in the filename stem are compared
using exact surrounding text, extension, semantic category, and parent directory.
Consecutive runs need at least three members. Unpadded numbers can cross digit
widths; padded runs keep a common width, including `099` to `100`. Padding changes
or extreme known-size mismatches split runs. Duplicate ordinals are ambiguous;
overflowing tokens, explicit `v` versions, dotted version components, and numbers
embedded inside identifiers are conservatively rejected. Competing patterns
prefer the longest run, then pattern name. Numeric order and exact member IDs
are retained. Some ambiguous valid sequences may therefore remain separate;
all files remain reachable.

**Related files:** remaining non-numeric names need a shared name family before
the final `_`, `-`, or space, the same parent, extension, and semantic category,
known timestamps spanning at most ten minutes, and compatible known sizes
(default ratio at most 8). This is a total-span check, not timestamp chaining.
Timestamp coincidence alone, unknown timestamps, mixed types, or a size outlier
are insufficient. Rejected numeric patterns cannot bypass sequence safeguards
by becoming a temporal group. No structural dependency inference is performed.
Groups expose a brief explanation in the existing metadata area.

## Density, stability, and progressive disclosure

Default pages still hold 20 objects plus a remainder summary. Artifact cohorts
and filename sequences each consume one cohesive region. Every member is
reachable by expansion/paging, including low-interest output. Remainder summaries
inherit prominence from their most salient omitted item, identify that item in
the sidebar, and open omitted entries in score order. Remainders inside a series
preserve numeric order instead.

Disclosure priority is computed in the layout layer separately from object
positions. Distant nodes show three important objects/structures, medium nodes
six, and nearby nodes their bounded page. Selection and the active ancestor path
remain visible even outside those limits. This also lets a valuable newly loaded
file be discoverable through a prioritized summary without moving known objects.

LOD uses the existing apparent world scale with separate enter/leave thresholds
(.0022/.0018 for medium, .020/.016 for full detail) to avoid flickering near a
boundary. Labels retain projected-size and overlap checks. Camera changes do not
rerun analysis or layout; salience does not cause continual rearrangement.
Metadata updates retain existing anchors and pages, and navigation continues
through the same nested world. No visual redesign or camera control changes were
needed.

## Shapes and controls

The existing semantic palette and shape vocabulary are retained:

| Category | Geometry | Color |
| --- | --- | --- |
| Directory | Floor/backdrop with nested contents | Blue |
| Source / text | Upright thin slab | Warm / pale |
| Image | Wide panel with a foot | Magenta |
| Media | Offset panels | Violet |
| Executable / binary | Compact prism | Green / subdued green |
| Archive | Stacked bands | Red |
| Generated | Low tile | Subdued gray |
| Symlink | Linked offset forms | Cyan |
| Special / unknown | Compact marker | Muted brown |

Sequences retain a shared floor, roof, and supports containing actual member
modules. Expand to select individual filenames and metadata. Default pages still
hold at most 20 objects plus a remainder summary. Far nested content uses reduced
geometry and labels, with more detail revealed at closer camera scales. Loaded
counts for partial previews refer only to the known subset.

- Click a scene object/sidebar row: select and animate focus
- Double-click or Enter: enter a directory or expand a group; regular files only focus
- Tab: focus next visible object; F: focus selection
- Backspace or Parent: zoom out and restore enclosing page/selection
- Page Up / Page Down or arrows: browse dense contexts
- R or Reset: current-context overview
- Right-drag in the scene: orbit; wheel: zoom
- Animate navigation: enable/disable subsequent transitions
- Window-manager close request: quit; Esc remains unassigned

Orbit cannot interrupt active transitions; zoom can. No free flight, uncontrolled
roll, custom quit key, or destructive file operation was added.

## Automated validation

All four suites run without a window or OpenGL context:

- Analysis/disclosure: continuous recency, balanced ranking, generated classification
  and negative cases, numeric/padding/version/size false positives, conservative
  related grouping, artifact coverage, ranked summaries, stable positions and LOD hysteresis.
- Camera: original orbit/zoom bounds, transition priority/interpolation, nested scale and clipping.
- Mock regression: salience, semantic shapes, numeric sequences, nested transforms,
  containment, continuous navigation, member selection, picking, paging, and cached hierarchy.
- Real filesystem: temporary directory enumeration, parent IDs, sizes/timestamps,
  permissions/executability, hidden files, regular/directory/broken/cyclic symlinks,
  missing/disappearing files, non-directory starts, permission denied where the
  process is not privileged, bounded previews and progressive loading, stable
  partial-to-full placement, compound sequences, update deferral, and worker cancellation.

The real suite includes a 600-file miscellaneous directory plus a 621-entry mix
of 400 recent artifacts, 200 older notes, 20 frames, and important old source.
It checks coverage, discoverability, bounded pages, and stable positions. Test fixtures are temporary and
removed by RAII, including restoration of permissions on the unreadable fixture.
Debug and Release builds and all suites are checked for Milestone 4. The existing
container lacks the ASan runtime, so sanitizer testing is unavailable.

## Manual X11 verification for Milestone 4

Milestones 1–3 were manually accepted by the user. Milestone 4 GUI behavior has
not been verified in this execution environment; automated passes do not replace
these checks. Choose real directories by category rather than assuming any path:

| Directory category | What to inspect |
| --- | --- |
| Source-code repository | Useful older source/config files remain prominent beside recent binaries/output; dot configs are not excessively dimmed. Check nested previews, focus, parent zoom and return stability. |
| Build/output directory | Objects, versioned libraries, dependency/cache/temp files stay low priority. Large leftover cohorts form one Build artifacts complex. Expand it and reach individual members; output is deweighted, never lost. |
| Photo/image directory | Consistently numbered images form compound structures. Wide panels and colors remain familiar. Check that size outliers or inconsistent names do not pull unrelated images into a group. |
| Numbered sequence directory | Try padded frames, `099`→`100`, unpadded digit-width changes, and middle-token notes. Expand and page: names/order must remain numeric. Version-like names and malformed padding should remain separate or split into coherent runs. |
| Many miscellaneous files | Check bounded pages, the Other entries summary's highest-omitted explanation, and ranked remainder expansion. Important content must remain discoverable when a partial preview completes. |
| Session/export outputs | A shared non-numeric family, extension, time window and similar sizes should group; matching time alone, old outliers, unknown times or mixed extensions should not. |

For each category, approach and retreat from nested directories/groups: important
structures should appear first, then more files and readable labels, with no
coordinate jumps or scene replacement. Make small orbit/zoom movements near LOD
boundaries to check flicker/clutter. Revisit and reset without data changes to
check stable positions. Also verify existing symlink/error status, resizing,
picking, Esc remaining unassigned, and normal window-manager closing.

## Layer boundaries

- `filesystem/`: cached metadata, isolated real reader/worker, deterministic test fixtures
- `analysis/`: categories/shapes, explainable salience, artifact evidence, conservative series/related groups
- `layout/`: parent-relative transforms, persistent hierarchy, bounded/ranked summaries, stable slots and disclosure priorities
- `navigation/`: directory/group contexts, selection, pages, enclosing-context stack
- `graphics/`: cached geometry buffers, shaders, camera, projection/picking
- `ui/`: semantic presentation, labels, metadata/status controls
- `app/`: source orchestration, loading policy, snapshot application, SDL lifecycle

The runtime always constructs the real source from the command-line path. Mock
construction remains available only for deterministic regression tests. No new
third-party dependency was introduced; the worker uses the C++20 standard library.
