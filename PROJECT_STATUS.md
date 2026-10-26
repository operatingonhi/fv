# Implementation checkpoint

## Milestones and validation

- Milestones **1–6 complete and accepted** by the user. Scheduled milestone work
  is finished. Current work is the requested post-milestone label/content visibility
  adjustment within DESIGN.md; its visual behavior needs separate X11 validation.
- See `MILESTONE6.md` for the audit, changes, measured stress results, limitations,
  and focused manual checklist. Source and tests remain authoritative.

## Architecture and current behavior

- C++20, SDL2/native X11, OpenGL 3.3 Core, Dear ImGui submodule and explicit CMake
  targets. Existing filesystem/analysis/layout/graphics/navigation/UI boundaries
  are retained.
- One cancellable worker enumerates directories and validates paths. Active
  directories and every immediate directory displayed on their current page fully
  load. There is no eight-directory/64-entry cutoff for those dioramas. Grandchild
  directories are represented without scanning their contents. No automatic link
  traversal or whole-tree scan.
- Analysis retains continuous recency, contextual artifact penalties, numeric
  series, conservative temporal groups, and artifact cohorts. Classification
  remains outside graphics. Source/text and executable/binary now differ in shape.
- Hierarchical transforms, stable preview anchors, and deferred ancestor updates
  remain. Loaded anchors also survive page round trips. Stable-order merging uses
  member lookup instead of repeated vector erases/searches.
- Default layout is at most twenty objects: nineteen ordinary objects plus
  navigable Other entries when oversubscribed; up to twenty without a summary.
  Search/history share the capacity rule. Tab/Shift+Tab wrap pages without moving
  the camera; arrows select. Page Up/Down retain bounded paging.
- Summaries have a preview page without eager recursive replication. Strong series
  retain a shared floor/roof/support structure and ordered addressable members;
  weaker inferred groups use a shared base. Member paging bounds large sequences.
- Hysteretic, apparent-scale LOD retains major structures and the active path.
  Distant files simplify; only the current browsing level supplies scene names. Current-directory
  and current-page names are mandatory, independent of LOD/filters/density. Child
  dioramas retain their complete bounded page, including groups/summaries. Loading
  or unavailable contents have markers; confirmed empty directories remain empty.
  Nested traversal is bounded. Idle frames skip LOD and geometry rebuilding. Scene preparation time
  and box/label counts appear in Settings; GPU uploads remain revision-based.
- Session Settings expose animation enable/duration, orbit/zoom sensitivity,
  nested detail depth. Hidden/artifact controls remain in
  Filters. Defaults preserve accepted camera limits, easing and navigation model.
- Shared selection, cached queries, history, integrated inspector, desktop opening
  and terminal cwd behavior remain. Control bytes are escaped in object rows and
  labels; selected long labels fit the spatial viewport. Sidebar has no decorative
  window title bar. Esc cancels UI/search/pending validation and never quits.

## Tests and runtime

- Debug: `cmake --build build -j2`, `ctest --test-dir build --output-on-failure`.
- Release: same commands with `build-release`. Six suites include the five
  preserved suites plus `refinement-stress`.
- New stress fixture: 14,719 entries, including 10,000 miscellaneous files, 2,000
  artifacts, forty 60-member sequences, 250 directories, and 64 nested levels.
  It tests cached preparation and bounded hierarchy/LOD without a real tree scan.
- Real filesystem tests include bounded concurrent disappearance, unusual/long
  names, broken links, permission failures where permitted, invalid paths,
  launcher failures and stale history. Mode-000 testing can be skipped when run
  with privilege to read it.
- Launch: `./build/spatial-navigator [directory]`; optional `--smoke-test`.
  Files use `xdg-open`; terminal uses `FSN_TERMINAL`, then `TERMINAL`, then
  `xdg-terminal-exec`/`xterm`. No shell expansion or custom quit key.

## Remaining boundaries

No watcher/automatic refresh, fine-grained cache eviction, Unicode-aware search,
or filesystem index. Analysis/merging
still run on the main thread after asynchronous enumeration. Extremely large
listings can therefore cause a preparation pause. Cached history/worlds retain
memory. Kernel-blocked filesystem calls may delay worker shutdown. Fallback focus
previews are samples. Settings are session-only. Visual feel,
clutter, labels, grouping silhouettes, and window-manager behavior require manual
X11 inspection for the latest requested visibility changes.


## Post-milestone visibility validation

Added regression coverage for mandatory current-page names at distant LOD/zero
nested label density, current-directory headings, every immediate-child placement
including summaries, pending-content markers, and all seventeen immediate
directories loading fully (one exceeds 64 entries) without a grandchild scan.
Large directories retain grouping and pagination; this does not render unbounded
numbers of individual shapes at once. Manual checks: zoom out and lower nested
label density; confirm all current-page names; compare empty, loading, unreadable,
and populated dioramas; Tab through pages, enter a child and return; inspect label
collisions and the directory heading under X11.

Latest visibility-change validation: Debug and Release builds passed all six
suites; `git diff --check` passed. GUI smoke launch again stopped at
`Initialize SDL2: x11 not available`; the new heading, label collision placement,
and diorama readability require manual X11 inspection.


## Directory-title follow-up

Subsubdirectory and deeper directory titles are suppressed, including during
search highlighting/full LOD. Immediate subdirectory titles and nested geometry
remain. The fixed current-directory heading draws after object labels. Directory
reads resolve display names for `.`, `..`, trailing separators, and `/` on the
worker while preserving the original filesystem path; an uncached path fallback
keeps the heading nonempty during initial loading. Added regression tests for
startup, ascending beyond the initial world, cached-parent return, trailing
separators, root metadata, and nested directory title promotion on entry.
Manual X11 checks: launch without a path argument, ascend to parents and `/`,
then enter a child; confirm the current heading and immediate subdirectory names
remain, while miniature directory titles do not obscure their contents.


## Parent/sibling context follow-up

Every newly opened real-filesystem world now requests its actual parent on the
existing worker, including startup, path jumps, and ascending beyond an unseen
parent. `app/browser_surroundings.cpp` composes a cached parent backdrop around
the existing CWD identity transform. Background results never reposition CWD
objects or move the camera. The active directory is excluded from backdrop
geometry because the main hierarchy supplies it and all descendants.

The parent's page is chosen to include the CWD individually, even if hidden or
low-salience. Other siblings remain represented through bounded pages, groups,
and remainder summaries. Displayed sibling directories get at most 64 immediate
entries each, without recursive traversal or automatic symlink following. No
sibling labels or picking targets are merged into the main presentation. The
root `/` has no parent. Parent-listing errors preserve a usable CWD and appear in
status. Existing bounded-world navigation/camera behavior remains; seamless
rebasing across those boundaries is still deferred.

The directory worker now carries an opaque cache scope so parent-context IDs
cannot collide with the primary filesystem cache. Generations reject stale
reads after jumps. History stashes/restores the backdrop and resumes unfinished
previews; Esc leaves background context loading intact. Background geometry
preparation occurs once per incoming result batch and is cached between changes.

Regression coverage includes transform consistency and branch exclusion,
startup/unseen-parent ascent, descent/return, history, symlink startup, stale
background results, Esc, a hidden CWD beyond the parent's first page, cache-scope
isolation, idle frames, and unreadable parent fallback where testable.
Manual X11 checks: start inside a directory with several siblings; verify the
surrounding plane/dioramas, then descend and ascend past the starting level.
Check that CWD names remain legible, sibling contents stay unnamed, background
arrival does not move the CWD/camera, and Back/Forward restores the right context.
Also inspect a dense parent and `/`; confirm groups/summaries keep geometry bounded.

Parent-context validation: Debug and Release builds passed all six suites;
`git diff --check` passed. GUI smoke launch stopped at
`Initialize SDL2: x11 not available`. Visual composition, camera framing, and
sibling readability at startup and unseen-parent ascents remain for manual X11
inspection; no GUI verification is claimed here.


## Current-level names only

Scene labels are now generated only for entries directly in the current browsing
context. Files, directories, compound groups, and remainder summaries in nested
or sibling directories retain geometry without names, regardless of LOD or search
highlighting. Entering a directory reveals its direct entry names. The fixed CWD
heading and current-page labels remain mandatory. Removed the obsolete nested
label-density setting and per-object label override. Tests cover nested files at
full detail/search highlighting, label promotion on entry, and both files and
subdirectories in a sibling while preserving their geometry.
Manual check: enter a directory with populated children and siblings, zoom and
search; verify only the CWD heading and its immediate entry names are displayed.

Current-level label validation: Debug and Release builds passed all six suites;
`git diff --check` passed. GUI smoke launch stopped at
`Initialize SDL2: x11 not available`; visual verification remains manual.


## Continuous ascent and bounded ancestor rendering

The jumps came from explicit non-animated camera resets at world-root/depth
boundaries. Real directory entry now rebases at three nested levels, cloning
cached metadata with remapped IDs and preserving local layout anchors, pages,
compound members and preview completeness. The incoming displayed camera pose
is transformed into the new frame before the normal zoom. Rebased layouts keep
the existing geometry depth cap; distant analyzed data can recreate pages lazily.

Up at a rebased root validates the parent and returns to its original cached
page/selection/group when that history state still exists. New child metadata is
grafted back before zooming out. Return links are numeric history handles, so
they do not retain worlds beyond the existing 64-entry history bound. Once a
handle has been evicted, the immediate parent backdrop is promoted to the active
world, retaining the old CWD branch and mapping the camera for the zoom-out.
Unseen parents use the same promotion after worker loading; the current scene
stays visible while waiting. Esc cancels a waiting ascent. Disabled/zero-duration
animation settings still take effect. Root `/` remains a no-op for Up.

Presentation now starts at the immediate parent of the CWD. The world's external
backdrop is merged only at its root; it is omitted when the current directory
already has a parent in the main hierarchy. Grandparent planes and objects are
not drawn. Current-level-only names and all existing navigation controls remain.
Unrelated absolute-path/history jumps still use controlled scene changes.

New tests traverse forty directory levels down and back up, including history
eviction, and compare projected directory positions at rebase/ascent boundaries.
They also check zoom midpoints, unknown-parent ascent, interrupted boundary
reversal, disabled animations, cancellation, returning into Other entries,
metadata/error/path/ID copying, anchor/page conservation, and absence of
higher-ancestor geometry. Visual camera feel still needs X11 inspection.

Continuous-ascent validation: Debug and Release builds passed all six suites,
including the forty-level traversal, interruption, group-return, cache-copy and
ancestor-culling regressions. `git diff --check` passed. GUI smoke launch stopped
at `Initialize SDL2: x11 not available`; no visual verification is claimed.
Manual X11 checks: descend past ten levels and repeatedly use Parent/Backspace;
ascend above the startup directory; reverse a zoom mid-transition; verify only
immediate sibling context is drawn and that names remain limited to the CWD.
