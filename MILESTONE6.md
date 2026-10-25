# Milestone 6 audit and validation

The accepted Milestones 1–5 implementation is the baseline. The checkpoint's
pending Milestone 5 acceptance is superseded by the user's acceptance.

## Audit before implementation

Satisfactory systems to preserve:

- Parent-relative cached transforms, pinned preview anchors, deferred ancestor
  updates, and cached page children preserve spatial continuity.
- Explicit camera interpolation, bounded orbit/zoom, shared selection, cached
  search/filter dimming, history restoration, and integrated inspector work
  without rebuilding layout for ordinary interactions.
- One cancellable filesystem worker, bounded child previews, explicit symlink
  activation, argv-based launchers, and error-valued metadata handle normal I/O
  failures. Rendering performs no filesystem queries.
- Continuous recency, artifact cohorts, conservative temporal families, numeric
  sequences, hysteretic LOD, and revision-based GPU uploads are already present.

Deficiencies and targeted work:

- Twenty objects plus a summary creates a fifth row. Reserve the last slot in
  the existing footprint for the summary; use one shared page-capacity rule for
  layout, search, and history. Tab should wrap pages, with arrows selecting.
- Eager recursion through remainder summaries materializes repeated omitted
  member lists. Bound summary previews and expand them on entry.
- LOD reduces object counts but still emits full semantic geometry and labels
  at every depth. Simplify distant files, limit nested traversal and labels,
  retaining major structures and the active ancestor path.
- Source/text and executable/binary shapes coincide. Weak related groups use
  the same roof structure as strong sequences. Refine the restrained vocabulary.
- Camera duration and control sensitivity lack user controls; numerical input
  boundaries and interrupted transitions need additional coverage.
- Geometry preparation lacks timing/count instrumentation. Add inexpensive
  measurements and a synthetic stress executable, without a profiling framework.
- Stable-order merging erases from the middle of a vector repeatedly, making
  full loading quadratic even for standalone files. Index stable identities.

Known boundaries to preserve: bounded worlds at root/depth boundaries; no
watcher/refresh or filesystem index; byte-based search; no new alternate list
explorer; synchronous analysis after worker enumeration. Visual feel and theme
readability require manual X11 inspection.

## Implemented refinements

- Default oversubscribed pages now use nineteen entries plus Other entries in
  the twentieth slot, without an extra back row. Smaller directories can display
  twenty entries. Search and history use the same capacity rule. Tab/Shift+Tab
  cycle pages with wrapping and preserve the camera; arrows retain selection.
- Stable merging indexes member IDs instead of repeatedly searching/erasing the
  remaining vector. A small cache of coordinates/dimensions preserves loaded
  anchors across page round trips without duplicating member lists in that cache.
- Summary nodes keep a preview page but do not recursively materialize further
  omitted-member summaries until entered. All entries remain reachable.
- Distant files use simplified geometry, distant labels are never generated,
  and nested drawing is bounded. Active ancestry bypasses depth pruning and the
  depth budget restarts at the current directory. Existing salience ranking and
  LOD hysteresis remain. Idle frames skip even LOD evaluation; revision-based
  geometry uploads remain. Vertex preparation reserves its known buffer size.
- Source uses an edged slab, text a document plate, executables an open frame,
  and binary files a prism. Strong sequences retain roof/support compounds;
  weaker inferred groups use a shared base. Existing conservative grouping
  heuristics were retained after reviewing their false-positive tests.
- Camera limits, smoothstep easing, 0.4-second default, interruption behavior,
  and parent-child zoom model are preserved. Session Settings expose animation
  enable/duration, orbit/zoom sensitivity, label density and nested detail depth.
  Invalid aspect ratios and nonfinite input deltas cannot poison camera matrices.
- The sidebar has no decorative title bar. Control bytes in object names are
  escaped for rows/labels; long selected labels fit inside the spatial viewport.
  Full names/paths remain in the integrated inspector.
- Settings shows the latest scene-preparation CPU time and box/label counts.
  This measures query/LOD/presentation preparation, not GPU time or whole frames.
  No dependency or profiling framework was added.

## Automated validation

Both established Debug and Release configurations built successfully, including
SDL/OpenGL/ImGui targets. All **six suites passed** in both configurations:
original camera, mock hierarchy, real filesystem, analysis, practical browser,
plus new refinement/stress tests. `git diff --check` passed.

New tests cover page/summary conservation, every item reachable across pages,
Tab wrapping without camera motion, loaded-anchor page round trips, idle frames
without LOD/geometry rebuilds, simplified distant geometry, label-density/depth
settings preserving the active context, zero-duration and interrupted camera
transitions, degenerate aspects, and displayed control bytes.

The synthetic fixture contains **14,719 cached entries**: 10,000 miscellaneous
files, 2,000 artifacts, forty 60-member sequences, 250 directories, a 64-level
tree, and long/control-byte/non-ASCII-byte names. It materialized **23 nodes**,
**57 boxes / 27 labels** at normal detail and **12 boxes / no labels** at far
LOD. One local run took approximately **1,418 ms Debug / 295 ms Release** for
fixture construction, analysis, hierarchy/presentation preparation and update.
These are diagnostic end-to-end CPU measurements, not frame timings or a
before/after speedup. No huge real filesystem was recursively scanned.

The real-filesystem suite additionally exercises a bounded 300-file concurrent
removal during enumeration and preserves unusual/long filenames. Existing
broken-symlink, inaccessible-path, vanished-metadata, stale-history, unavailable
launcher and cancellation coverage remains. Mode-000 permission checks may be
skipped when the process has privileges to read the directory.

GUI smoke launch was attempted and stopped with **`Initialize SDL2: x11 not
available`**. No GUI behavior is claimed to have been visually verified here.

## Remaining limitations

- Enumeration is asynchronous; analysis/merging after a result remain on the main
  thread. Extremely large listings can still cause a preparation pause.
- No automatic refresh/watchers or fine-grained cache eviction. History/world/page
  caches retain memory; stale metadata is revalidated on activation/navigation.
- Visual rebasing beyond the existing root/depth boundary remains deferred.
- Previews are bounded samples rather than globally ranked listings. Search is
  cache-only and byte-based; no Unicode normalization or indexing is added.
- Settings last for the session. GPU/frame profiling and visual tuning require a
  real X11 display. Kernel-blocked filesystem calls can delay worker shutdown.

## Focused manual X11 checklist

1. Open a large mixed directory. Check nineteen visible objects plus Other entries
   in the final grid slot; Tab/Shift+Tab visit every page and wrap without camera
   motion. Arrows, mouse, sidebar and inspector must agree on selection. Enter
   Other entries and return; verify page/selection restoration and compactness.
2. Navigate three directory levels, reverse mid-transition, focus files, zoom to
   limits, orbit after settling, and reset. Contents should exist before entry,
   preserve anchors, and maintain continuous parent-child zoom. Change aspect
   ratio and inspect at minimum/maximum window sizes.
3. Zoom slowly through LOD boundaries. Major structures and some child contents
   should remain visible; detail and labels should reveal without excessive
   flicker. Reduce nested depth/label density in Settings while inside a child;
   the active context must remain usable.
4. Inspect small and large sequences, including later member pages. Verify numeric
   order, individual identity/metadata and cohesive roof/support geometry. Compare
   weak temporal groups and build artifacts for clutter. Compare source/text and
   executable/binary silhouettes, semantic colors, and selected states.
5. Search for an omitted member, cycle F3 across pages, filter hidden/artifact
   entries, inspect, and restore history. Ordinary interactions must preserve
   coordinates. Select very long and unusual names; confirm legible rows and the
   selected label, with the complete name/path available in the inspector.
6. Adjust animation duration (including zero) and input sensitivities. Confirm
   responsive interruption, usable zoom/orbit limits, and sensible defaults.
   Observe preparation counts/timing in Settings while browsing dense content.
7. Try a broken link, invalid/inaccessible path, disappearing entry, stale history,
   missing opener and missing configured terminal. Errors must remain in the UI.
   Confirm terminal cwd and successful opening on the configured desktop. Esc must
   only cancel transient state; close through the window manager normally.
