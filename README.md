# Spatial filesystem navigator

Milestone 5 adds read-only browser actions to the accepted spatial filesystem
visualization: keyboard selection, desktop file opening, search/filtering, path
entry, history, metadata inspection, and terminal launching. There is no
alternative explorer view or destructive file operation.

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

The selected path starts a cached spatial world. Ordinary parent/child navigation
zooms through its existing nested geometry. Backspace at the initial root, or
entry beyond the six-level geometry cap, starts another bounded world; unrelated
path jumps also use a controlled scene change. Invalid/inaccessible paths report
status without terminating. Path entry supports absolute paths, paths relative
to the current directory, and `~`/`~/`; no shell expansion is performed.

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
  groups/summaries. Navigation beyond that cap starts another bounded world; seamless
  visual rebasing remains future refinement. The visual preview depth never causes filesystem traversal.

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
Preview loading never traverses discovered links, so it cannot create traversal
cycles. Explicit activation resolves and validates a link target, then enters
its directory or opens its regular file; broken/cyclic targets report errors. Link size/mtime are left unavailable rather than
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

- Click a scene object/sidebar row: shared selection and animated focus
- Arrows: select a drawn eligible object in the requested screen direction; prefer
  nearby aligned targets, without wrapping or moving the camera
- Tab / Shift+Tab: focus next / previous eligible object on the page
- Home / End: select first / last eligible page object; F: focus selection
- Enter / double-click: enter directory, expand group, or open a regular file
- Backspace / Parent: enclosing context; Page Up / Down: dense-context pages
- Alt+Left / Right: history back / forward
- Ctrl+L: path entry; Ctrl+F: search; F3 / Shift+F3: next / previous match
- Ctrl+Shift+F: filters; Ctrl+H: toggle hidden entries; Ctrl+I: metadata inspector
- Ctrl+Shift+T: open terminal here
- Esc: cancel editors/popups/pending navigation and clear search; never quit
- R / Reset: overview; right-drag: orbit; wheel: zoom
- Window-manager close / normal SDL quit: clean shutdown; no custom quit key

Scene bindings yield to active ImGui editors/popups. Search-field Enter applies
the query and activates the selected match. Orbit cannot interrupt transitions;
zoom can. The animation checkbox controls subsequent transitions.

## Read-only browser behavior

Selection belongs to navigation, identified by entry/group identity. Scene,
sidebar, keyboard, search, and inspector use that same selection. The sidebar is
the existing spatial object selector, not an alternative file-explorer view.

Search is a case-insensitive literal filename substring over cached entries in
the current directory/group (all pages), plus up to two cached descendant levels.
It does not initiate recursive searches or build an index. Incomplete previews
are indicated. Matching groups/directories retain their spatial context;
nonmatches dim and become unselectable. F3 cycles matching objects across pages.
Matched nested contents gain detail within the existing bounded disclosure.
Filters select files/directories or semantic category and toggle hidden/artifact
entries. Ancestor directory context remains available for matching descendants.
Query/filter changes preserve layout coordinates. Esc clears search, while
filters persist until changed/reset in the Filters popup.

History holds up to 64 directory/group visits, including page, selection, query,
and useful camera state. Going somewhere new truncates forward history. Cached
worlds are shared among visits; same-world restoration animates through existing
geometry. Other worlds restore cached snapshots with a controlled change.
Targets are validated on the filesystem worker; missing locations report an
error and are skipped on subsequent history attempts. Snapshots are not live
refreshes. Path validation shares the existing single worker, and generation
checks discard stale results after world switches.

The sidebar selection summary shows name, semantic type, size and modification
time (or member count for groups). Inspect / Ctrl+I toggles an integrated,
scrollable metadata region with no window title bar or close button; Esc dismisses
it without quitting. The detailed region uses cached filename/path, kind, size, timestamp,
permissions, executable state, symlink target/errors, semantic classification,
salience and group details. It does not collect expensive metadata per frame.

Regular-file activation resolves/validates the target asynchronously, then uses
`xdg-open` with an absolute path as a separate argument. On Void Linux the runtime
helper is provided by `xdg-utils`. No shell is involved, and the browser does not
wait for the application to exit. Missing helpers and failed launcher exits
appear in status. Opening a file delegates to its desktop association.

Terminal command precedence is `FSN_TERMINAL`, then `TERMINAL`, then
`xdg-terminal-exec` with `xterm` fallback if the default launcher cannot start.
For example, launch with `FSN_TERMINAL='xterm -fa Monospace'`. Quoted arguments and
backslash escapes are supported, but shell expansion, pipelines and substitutions
are not. The child starts in the selected actual directory, otherwise the current
directory; groups use their containing directory. Process launching is isolated
in `app/desktop.*`, closes inherited descriptors, and polls exits without blocking.
No runtime helper is installed automatically.

## Automated validation

All five suites run without a window or OpenGL context:

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
The practical-browser suite additionally tests directional/shared selection,
history and stale/deleted targets, path parsing and symlink normalization,
search/filtering across pages and nested caches, command construction and missing
launchers. Debug and Release builds and all suites are checked for Milestone 5. The existing
container lacks the ASan runtime, so sanitizer testing is unavailable.

## Manual X11 verification for Milestone 5

Milestones 1–4 were manually accepted by the user. Milestone 5 requires manual
GUI validation: this execution environment cannot connect to X11 and has no
`xdg-open`, `xdg-terminal-exec`, or `xterm` available for successful launch checks.

- Use a source repository for keyboard-only arrows/Tab/Enter/Backspace navigation;
  compare mouse, sidebar and keyboard selection with the inspector. There is no
  separate list view to synchronize.
- Check parent/child zoom continuity, history restoration, orbit, zoom, reset,
  resizing and nested disclosure after each new navigation method.
- Open a harmless regular file and a file symlink with a configured desktop
  association. Check missing helpers, broken links and disappeared files report
  status while the browser remains responsive.
- Search a photo/sequence directory: match a member inside a compound structure,
  move across pages with F3/Shift+F3, try no matches, and cancel with Esc.
- Filter a build/output directory by category, hidden status and artifacts;
  inspect dimming, selection eligibility and unchanged layout, then reset filters.
- Enter absolute, relative and `~/` paths; try nonexistent/inaccessible paths.
  Test Back/Forward, restored selection/camera/query, a new visit truncating
  forward history, and a deleted history destination.
- Inspect files, directories, groups and symlinks: metadata should be legible
  and the optional inspector should not obscure normal browsing.
- Open a terminal in a selected directory and in the current directory with a
  file selected; check its working directory. Try valid and invalid configured
  terminal commands.
- Use a large miscellaneous directory to check summaries, paging, search and
  filtering remain bounded and useful without disrupting established grouping.
- Check editing fields/popups consume their keys, Esc cancels without quitting,
  and the window-manager close button/i3 close command shuts down cleanly.

Seamless world-root/depth rebasing, cache eviction/refresh/watchers, Unicode-aware
search, indexing, profiling, and further visual/performance refinements are
intentionally deferred. Milestone 6 has not begun.

## Layer boundaries

- `filesystem/`: cached metadata, isolated real reader/worker, deterministic test fixtures
- `analysis/`: categories/shapes, salience, artifact evidence, series/related groups and cached search/filter matching
- `layout/`: parent-relative transforms, persistent hierarchy, bounded/ranked summaries, stable slots and disclosure priorities
- `navigation/`: shared selection, directional neighbors, paths, history and directory/group contexts
- `graphics/`: cached geometry buffers, shaders, camera, projection/picking
- `ui/`: semantic presentation, labels, metadata/status controls
- `app/`: source orchestration, view/history snapshots, isolated desktop launching and SDL lifecycle

The runtime always constructs the real source from the command-line path. Mock
construction remains available only for deterministic regression tests. No new
third-party dependency was introduced; the worker uses the C++20 standard library.
