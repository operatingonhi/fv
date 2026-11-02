# Implementation checkpoint

## Milestones and validation

- Milestones **1–6 complete and accepted** by the user. Scheduled milestone work
  is finished. Current work replaces floating scene names with inscriptions on
  object surfaces within DESIGN.md; its visual behavior needs separate X11 validation.
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
  labels; long scene names truncate to their object surfaces. Sidebar has no decorative
  window title bar. Esc cancels UI/search/pending validation and never quits.

## Tests and runtime

- Debug: `cmake --build build -j2`, `ctest --test-dir build --output-on-failure`.
- Release: same commands with `build-release`. Nine suites include the five
  preserved suites plus `refinement-stress`, `surface-inscriptions`, `grid-refinement`
  and `feature-tour`.
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


## Object-surface inscriptions

Current-page names now use cached, depth-tested OpenGL text meshes attached to
world-space faces. They tilt and scale with the objects, with no billboarding,
screen-space collision displacement or connector lines. Files have broader
semantic faces carrying small inscriptions directly; generated-file tiles use
their horizontal top face. No extra name panel or backing card is drawn.
Directory backdrops and their lettering are now 50 percent taller;
compound groups gain a shared title fascia. Semantic geometry remains. Default
grid spacing is 2.8 instead of 3.0, and file bodies have a taller minimum height.
The fixed current-directory viewport heading and current-level-only naming rule
remain. Search/filter dimming still applies. Names truncate with `...` at UTF-8
boundaries to fit a fixed surface width, while the sidebar/inspector keep full names.
At distance or oblique angles, inscriptions may become unreadable or occluded.

The renderer owns a fixed 64-pixel atlas of Dear ImGui's embedded scalable font,
with mipmapped minification replacing the original small default font bake;
no new font dependency or filesystem reads were added. Text meshes and GPU buffers
update on scene revisions rather than camera motion. A separate CPU-only suite
checks atlas building, truncation, text bounds, face winding, object attachment
and rebase scaling. There are now seven suites.

Manual X11 checks: compare file silhouettes and inscription sizes in a mixed-type
directory, inspect long/Unicode names and loading markers, orbit to confirm text
stays on the faces, zoom in/out, page through a dense directory, expand a series,
and descend/ascend through rebasing boundaries. Verify nested/sibling names remain
absent and full selected names remain available in the inspector. No visual
acceptance is claimed until those checks are performed.

Inscription validation: Debug and Release builds passed all seven suites;
`git diff --check` passed. GUI smoke launch stopped at
`Initialize SDL2: x11 not available`. Surface readability, occlusion, spacing,
and the revised silhouettes still require manual X11 inspection.


## Inscription integration follow-up

Removed the separate front name panels, generated-tile upright sign, and dark
text backing quads. Each category now places its inscription directly on its
existing geometry: document names above the horizontal divider, source on the
slab, executables on the upper beam, images/media on their panels, archives on
the top band, symlinks on the front linked form, generated tiles on top, and
other files on the prism face. File font height is 0.16 world units at unit
browsing scale, independent of salience. Directory backdrops are 3.6 units high
instead of 2.4 (grounded at zero), with text height increased from 1.45 to 2.175.
Directory picking bounds accommodate the taller backdrop. Group fascia sizes
and the current-level naming scope remain unchanged.

Scene lettering uses a fixed high-resolution bake of the already embedded
scalable font plus mipmapped minification. It remains texture-based text; visual
sharpness at working distances and oblique angles requires X11 inspection.
Tests check glyph-only meshes, horizontal tile winding, all ten file categories
at low/medium/high salience, absence of added name geometry, document divider
clearance, directory proportions and rebase scaling.

Integration follow-up validation: Debug and Release builds passed all seven
suites; `git diff --check` passed. GUI smoke launch stopped at
`Initialize SDL2: x11 not available`. Check the document lettering above its
divider, relative file/directory font sizes, taller backdrops, top-facing tile
lettering and sharpness during zoom/orbit under X11.


## Consistent widths, lettering, and current-directory title

Visible file faces, directory floors and compound regions now share a common
width. Horizontal gaps are one eighth of the visible width; default column
centers remain 2.8 units apart, so visible widths are about 2.489 units and gaps
about 0.311. The 94-percent directory inset is accounted for explicitly. Files
retain their semantic forms, widened in place, and have sufficient height for
the common lettering. Salience retains its ranking and brightness effects;
width no longer varies by salience or category.

File, directory and group inscriptions use the directory title proportions:
14.8 units of text width to 2.175 units of nominal font height at directory
scale. This gives equal font sizes and text capacity for equal-width shapes.
All names remain on their existing geometric faces, without separate cards.

The current directory now also has a full-scale inscription on its own
backdrop, using the same metrics as its miniature child directories. It scales
with the browsing context and is the largest scene text. The fixed viewport
heading remains. Current-level entry naming and hidden nested/sibling names
are unchanged. Tests cover the one-eighth gap, common title metrics, face fit
for all file types, and title promotion when entering directories.

Consistent-width validation: Debug and Release builds passed all seven suites;
`git diff --check` passed. GUI smoke launch stopped at
`Initialize SDL2: x11 not available`. Manual X11 checks: compare horizontal gaps
and displayed name lengths across mixed types, confirm matching entry font sizes,
inspect the large current-directory backdrop title at startup and after entering
or leaving directories, and check truncation and readability during orbit/zoom.


## Directory-floor distribution

Relaxed the one-eighth-width gap rule. Object sizes, semantic geometry and font
metrics remain unchanged. Columns and rows now independently span the directory
floor with a configurable half-unit margin around placement bounds. The default
five-column/four-row page uses approximately 3.09 units between columns and
4.12 between rows, replacing the previous uniform 2.8-unit pitch and large rear
border. The grid is centered on the directory floor, including even-column
configurations. Shared diorama dimensions now live alongside the layout config.

Object width is configured independently from slot spacing. Sparse pages retain
the full page grid, so incoming entries, page changes and cached branch copying
preserve stable anchors. Tests cover balanced margins on all four sides, even
column counts, unchanged sizes, and stable sparse-to-full placement.

Floor-distribution validation: Debug and Release builds passed all seven suites;
`git diff --check` passed. GUI smoke launch stopped at
`Initialize SDL2: x11 not available`. Manual X11 checks: compare front/back/side
margins on a full page, inspect spacing among mixed file and directory types,
and page/enter/return to verify the established sizes and text remain unchanged.


## Sparse-directory dispersion

Partially filled directory/group pages now choose a dispersed subset of the
existing floor grid. The row count follows page occupancy; each populated row
spans available columns, and multiple rows span front-to-back depth. Singletons
use a middle grid row and central column. Full pages keep their previous grid,
and object/font sizes are unchanged. Last pages include the remainder summary
in their displayed-object count before choosing positions.

All chosen positions come from the same fixed slot pool. Metadata updates pin
known anchors, prefer dispersed free slots for new entries, and then fill other
unused slots. This avoids moving a directory the user may already be browsing.
Consequently a completed preview can retain a different order of positions from
a fresh layout of the same entries. Page caching and navigation use those anchors.
Tests cover occupancies one through twenty, front/back and side coverage,
non-overlap, and stable sparse-preview promotion to a full page.

Sparse-dispersion validation: Debug and Release builds passed all seven suites;
`git diff --check` passed. GUI smoke launch stopped at
`Initialize SDL2: x11 not available`. Manual X11 checks: inspect directories with
one, two, three, five, nine and nineteen displayed entries, the last page of a
large directory, and a preview completing while browsing. Confirm dispersion,
unchanged sizes, stable loaded anchors and continuous entry/return.


## Sparse-layout fallback

Restored front-first placement for partial pages, as requested in preference to
sparse selections from the full-directory grid. Removed the occupancy-based
slot selector. Full-page spacing, all object/font sizes and anchored navigation
are unchanged. Newly loaded entries fill free slots from the front.

Natural dispersion was considered, but arbitrary sparse placements can fragment
the available floor so a full page no longer fits without moving existing
objects. The current cache preserves nested-directory anchors and camera
continuity rather than animating layout repacking. A natural layout therefore
needs a separate repacking transition (or a different density policy); it is
deferred instead of retaining the rejected sparse-grid approximation.

Regression coverage now checks that every partial occupancy is a prefix of the
accepted full-page layout, that loading preserves anchors without overlaps,
and that a short final page stays in the front row with its summary accessible.

Sparse-fallback validation: Debug and Release builds passed all seven suites;
`git diff --check` passed. GUI smoke launch stopped at
`Initialize SDL2: x11 not available`. Manual X11 checks: confirm short directories
and partial final pages fill from the front, full directories retain accepted
spacing, and entering/returning preserves the established geometry and text.


## Continuous background-grid refinement

Replaced the fixed root-grid line buffer with a procedural, antialiased floor
plane. A coarse lattice stays visible while its 256-way subdivisions fade in
with logarithmic camera distance and smooth easing. The default three-directory
scale reduction fully reveals the next level; zooming out reverses the fade.
At a level boundary the fully visible minor grid becomes the major grid on the
same coordinates. Subpixel lines and the distant patch edge fade to avoid aliasing.
Camera motion updates uniforms rather than rebuilding scene buffers.

A double-precision GridFrame holds the lattice origin/unit and is stored with
cached worlds. Deep rebases, cached-parent returns, unseen-parent promotions and
history preserve phase and apparent spacing. Powers of 256 normalize the frame
for bounded precision during deep traversal. Unrelated path jumps start a fresh
frame. Camera state now also carries the browsing floor height, interpolated
alongside scale/distance and mapped between frames; selecting a tall object does
not lift the background grid. Filesystem, analysis, layout and controls are unchanged.

The CPU-only grid suite covers monotonic/reversible fades, three-layer completion,
level boundaries, repeated descending/ascending coordinate maps and interrupted
floor transitions. Existing forty-level browser tests also compare grid phase,
opacity and apparent spacing across actual rebase, ascent and parent-promotion
boundaries. There are now eight automated suites.

Grid-refinement validation: Debug and Release builds passed all eight suites;
`git diff --check` passed. GUI smoke launch stopped at
`Initialize SDL2: x11 not available`; shader appearance is not visually verified.
Manual X11 checks: descend/ascend through at least ten levels, reverse a zoom
mid-transition, zoom with the wheel, focus a tall file, use Back/Forward, orbit
to shallow pitch and resize. Confirm gradual fine-line fading, stable major-line
alignment at rebase boundaries, a floor that stays beneath the scene, and no
objectionable shimmer or distant-edge pop.


## Automated feature tour and recording workflow

Added an explicit `--demo OUTPUT_DIRECTORY` mode using a new real-filesystem
fixture and the normal Browser actions. Its 74 ordered captions match
`docs/FEATURE_TOUR.txt`, generated from the same C++ plan. The tour waits for
loading and camera transitions, supports metadata/settings panels, slowly orbits
and zooms, traverses twelve levels down and up, pages through ninety unrelated
files, searches to a late-page match, demonstrates relevance/filters/sequence
members/symlinks/errors, launches a harmless document and terminal, and exercises
history/path entry. It exits automatically; Escape/window close aborts. Input
cannot steer the tour. `--demo-no-launch` explicitly labels disabled external
steps for previews/tests. Offline script/fixture generation needs no X11.

`scripts/record_demo.py` preflights already-installed tools, optionally starts its
own unused Xvfb display, waits for an application-ready handshake, starts X11
capture, then releases the tour. It uses private XDG association directories and
terminal settings, with self-closing xterm helpers for real xdg-open/terminal cwd
steps. The display capture includes those external windows. It refuses existing
output directories and only finalizes `tour.mp4` after a successful tour, helper
markers and FFprobe validation. Captions are burned into the real application;
there is no audio. Failed captures retain logs and a `.partial.mp4` if produced.
Normal browser builds gain no supplemental system dependencies.

The CPU-only `feature-tour` suite executes the full plan with external launching
disabled and checks deep traversal, groups, cross-page search, filters/inspection,
recency ranking, narration parity, root return and overwrite refusal. Separate
Python recorder checks verify missing-tool reporting, private associations,
overwrite refusal and check-only behavior. GUI/capture acceptance remains separate.

Recording is blocked in this container: `ffmpeg`, `ffprobe` (Void `ffmpeg`) and
`xterm` are missing, and SDL reports `x11 not available` for DISPLAY=:0. Recording
on an isolated display additionally needs `Xvfb` (Void `xorg-server-xvfb`). No
packages were installed/downloaded and no video has been generated. See
`docs/RECORDING.md` for commands after rebuilding/providing the environment.

Feature-tour validation: Debug and Release builds passed all nine suites,
including every one of the 74 tour steps with desktop launches disabled.
All five Python recorder checks passed, including real private xdg-open
association dispatch with a filename containing spaces (using a test probe,
not a GUI viewer). `git diff --check` passed. Recorder preflight still reports
missing FFmpeg/FFprobe and xterm and unavailable X11 access; no video was
produced and no GUI or encoding verification is claimed. No packages were
installed or downloaded.


## Caption-only editor demonstration

The tour's text-file step now focuses README.txt, opens its inspector and explains
manual desktop opening without launching an editor. The narration/caption state
this explicitly. Removed the editor helper modes, temporary text/plain desktop
association, xdg-utils recorder prerequisite and required file-opened marker.
Normal manual file opening and the browser's desktop launcher are unchanged.

The terminal demonstration remains active and self-closes after seven seconds.
Aligned its helper with the locally configured recorder prerequisite `urxvt`,
which previously differed from the helper's hard-coded `xterm` command. Tests
cover the caption-only document step even with external launches enabled, the
terminal helper command, rejection of legacy editor modes, and video-finalization
checks that require only the terminal marker.

Editor-step validation: Debug and Release builds passed all nine suites. All
six Python recorder checks passed, including finalization without an editor
marker and the retained urxvt terminal launch command. The checked-in narration
matches the compiled tour; `git diff --check` passed. GUI recording was not run
in this environment. Rebuild the application before rerunning the recorder so
the compiled tour uses the new caption-only document step.


## Successful terminal launch no longer aborts the tour

The tour incorrectly treated any nonempty browser notice as a desktop-launch
failure. A successful terminal launch sets a notice, so the tour aborted even
though the terminal helper ran correctly. Browser desktop errors now have a
separate accessor carrying validation, spawn and unsuccessful child-exit errors;
the tour checks that instead of ordinary status text. Manual launch notices and
the self-closing urxvt helper behavior remain unchanged.

A CPU regression reproduced the abort on the success notice before the fix.
The enabled tour now completes all steps using `/bin/true` as its harmless
terminal helper. Additional runs verify that `/bin/false` and a missing helper
still abort with the actual error. No graphical applications are needed for
these process-launch tests. Recording instructions now explicitly show the
incremental CMake build needed after changing compiled tour/browser code.

Terminal-launch validation: Debug and Release builds passed all nine C++ suites,
including successful and failed real helper processes. All six Python recorder
checks and `git diff --check` passed. GUI recording remains unverified here;
the existing environment lacks its recording tools and usable X11 access.


## Revised feature-tour narration

Applied the annotations in `docs/FEATURE_TOUR.txt` to both the compiled tour and
its generated narration. The opening is now the unnumbered “A guided tour of
the Jurassic Explorer”; the closing is “Tour over”. Removed the image-focus,
extra symlink-inspection and broken-link activation actions. Actions marked as
unnecessary to mention still run with no caption or caption panel. Filtering
starts with excluding generated artifacts. Terminal selection/launch shares
one description, and each twelve-level descent/ascent shares one description
without repeating level names or grid commentary.

The tour now contains 71 actions and 32 numbered features. Explicit narration
modes keep feature numbering independent from action numbering. Text output
omits silent actions and continuation entries; the diagnostic timeline retains
all actions and their actual captions. Tests cover silent/shared captions,
plain titles and removed actions alongside the existing full-tour coverage.

Narration-revision validation: Debug and Release builds passed all nine C++
suites, and all six Python recorder checks passed. Both executables generate
text matching `docs/FEATURE_TOUR.txt` byte for byte; `git diff --check` passed.
The revised captions and video have not been visually inspected under X11.


## Shorter traversal and square recordings

Reduced the demo fixture and its traversal to four nested levels in each
direction. One caption, “Descend and ascend through nested directories”, covers
the round trip without mentioning a depth count. The tour now has 55 actions and
31 numbered features. The uncaptioned inspection at the deepest level remains;
the combined traversal caption resumes when ascending.

The recorder's `--size WIDTHxHEIGHT` now sizes the demo window on an existing
desktop as well as the optional Xvfb display. The demo accepts `--demo-size`,
requests a centered borderless window, waits for stable geometry and reports its
actual client rectangle in the capture handshake. Explicit sizes changed by the
window manager are rejected before capture. Square captures down to 720x720 are
supported; 1000x1000 is recommended on a 1080-pixel-tall desktop. The terminal
helper uses compact geometry positioned relative to the recorded rectangle.

Documented i3 floating rules matching only the tour and helper terminal titles,
square/nearly-square commands, and the unchanged monitor resolution. Existing
default runs still request a display-sized window; an unspecified Xvfb size
still defaults to 1920x1080. No programs or dependencies were installed.

Short-tour/sizing validation: Debug and Release builds passed all nine C++ suites,
including reaching the fourth level, returning to the root, combined captions
across the silent inspection and size parsing. All eight Python recorder checks
passed, including square capture offsets, helper placement and rejection of a
window-manager size override. Both executables generate the checked-in narration
exactly; malformed CLI sizes fail before creating output or initializing SDL.
`git diff --check` passed. Actual i3 placement and video capture remain unverified
in this environment without usable X11 and the supplemental recording tools.
