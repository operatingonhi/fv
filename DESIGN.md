# Product design

## Intent and scope

fv is a small native Linux/X11 filesystem navigator inspired by fsv and the
navigator shown in Jurassic Park. Filesystem hierarchy should feel like
nested places, and prominence should direct attention toward useful files.
The working application uses C++20, SDL2, OpenGL 3.3 Core, Dear ImGui and CMake.
Milestones 1–6 are complete; mock data remains for regression tests only.

Keep the interface primarily 2.5D: a shallow perspective view, planar directory
floors, simple semantic geometry, restrained terminal-inspired colors, and short
responsive animation. Do not introduce first-person flight, uncontrolled roll,
detailed models, a game engine, an embedded browser or heavy visual effects.
The application and executable are named `fv`.

## Spatial continuity and loading

A child directory is a smaller floor/backdrop containing visible child geometry.
Entering it enlarges that existing region; going up reveals its enclosing place.
Focus, group expansion/collapse and search-result navigation should animate.
Use explicit camera start/target states, elapsed time, easing and duration;
transitions must remain deterministic and interruptible. The current default is
0.4 seconds, with bounded yaw/pitch/zoom, reset, session sensitivities and an
option to disable animation. Unrelated path/world jumps may use controlled cuts.

Use parent-relative transforms and pinned layout anchors. Deep real-directory
navigation rebases around three nested levels to preserve numeric precision;
map the displayed camera and grid frame so crossing boundaries does not jump.
Ascent can restore a cached parent or promote the immediate parent backdrop.
If parent data is pending, keep the current scene usable; Esc can cancel ascent.
History retains up to 64 visits with page, selection, query and camera state.

Draw the current context and bounded descendants plus the immediate
parent/siblings, without grandparent objects. Parent context must also load at
startup and unrelated path jumps; align it around existing current-directory
coordinates without moving objects or the camera. At `/` there is no parent.
Choose a parent page that represents the current directory individually, even
when hidden or low-salience.
Sibling contents are unnamed bounded previews, not interactive current entries.

One cancellable worker enumerates directories and validates paths. Fully load the
active directory and every immediate directory on its displayed page; use at
most 64 entries for sibling/fallback previews. Do not recursively scan descendants
or follow discovered symlinks. Load deeper contents as navigation requires them.
Cache results on the main thread; distinguish loading, empty and unavailable
contents. Defer updates that would regroup an active ancestor until return.
Filesystem errors remain ordinary UI-visible conditions, not fatal exceptions.

## Layout, density and relevance

A default page occupies five columns and four rows: up to twenty objects, or
nineteen plus a navigable Other entries summary when oversubscribed. Every entry
must remain reachable by paging or expansion. Summaries inherit relevance from
omitted entries; do not recursively materialize all omitted members in previews.
Tab/Shift+Tab wrap pages without moving the camera. Search and history use the
same page-capacity rule.

Equal visible widths and consistent lettering proportions support readability.
Full pages spread across the floor with small balanced margins. Partial pages
fill from the front in center-first column order. Preserve loaded anchors and
page-return positions. Natural dispersion is deferred until repacking can
preserve nested navigation continuity; do not leave sparse holes in a full-page
grid as a substitute. Object/font sizes stay independent of slot spacing.

Analysis determines semantic categories, salience and groups before layout or
rendering. Heuristics must be deterministic, understandable and replaceable.
Modification recency decays continuously; generated, binary and hidden entries
receive contextual penalties rather than exclusion. Relevance affects ranking,
brightness and disclosure; it must not continually reposition loaded objects.
Current defaults and numeric weights live in `AnalysisConfig` and
[src/analysis/analysis.cpp](src/analysis/analysis.cpp), not duplicated prose.

Numeric sequences require a shared filename pattern/category/extension and a
consecutive varying token, including tokens inside stems. Preserve numeric order
and individual member identity. Reject ambiguous duplicates, overflow and version
patterns conservatively. Strong series use cohesive floor/roof/support geometry
with selectable members. Other related-name/time families and artifact cohorts
use a shared base. Timestamp proximity alone is insufficient for related groups.

## Geometry, names and background

| Category | Form / terminal-inspired color |
| --- | --- |
| Directory | Nested floor and taller title backdrop / blue |
| Source; text | Edged code slab; document plate with divider / warm; pale |
| Image; media | Wide panel with foot; offset panels / magenta; violet |
| Executable; binary | Open frame; prism / green; subdued green |
| Archive; generated | Stacked bands; low tile / red; gray |
| Symlink; special | Linked offset forms; compact marker / cyan; muted |

File, directory and group names are depth-tested inscriptions on existing
surfaces, never billboards or separate name cards. Text-file names sit above the
slab divider. Use consistent font height and text capacity relative to visible
width. The current-directory backdrop title scales with the directory and is the
largest scene text; retain its fixed viewport heading too.

Only the current context and its direct page entries have names. Child/sibling
contents keep geometry without names, even during search or at full detail.
Current-page names remain present independently of salience or nested detail;
filters may dim them. Truncate at UTF-8 boundaries with `...`; full names and
paths remain in conventional UI. At distance/oblique angles text may become
small or occluded. Cached meshes use ImGui's embedded scalable font baked into a
high-resolution mipmapped atlas, not new font files or dependencies.

Bound nested drawing with hysteretic apparent-scale LOD. Simplify distant files
and retain major groups, the active path and immediate child dioramas' bounded
pages. No filesystem queries, analysis or layout repacking on camera motion;
update presentation/LOD when needed and upload geometry on scene revisions.

The floor grid refines continuously with camera distance. Each subdivision is
256-way, approximately three directory scales. Fade minor lines in on approach
and out on ascent/zoom-out while retaining major lines. Preserve phase, scale
and opacity across rebases/history. Antialias subpixel lines and fade distant
edges. The grid follows the browsing floor, not a selected object's height.

## Practical navigation

Keyboard, scene picking and sidebar rows share selection. The integrated,
scrollable inspector shows cached metadata, symlink targets/errors, category,
salience and group explanations. Dear ImGui supplies practical controls; the
filesystem scene itself is OpenGL geometry. Settings are session-only.

Search is a case-insensitive literal filename substring over cached current
contents/all pages and up to two cached descendant levels. Report incomplete
scope; do not scan recursively or build an index. Dim nonmatches and prevent
their selection while preserving ancestor context and layout coordinates.
F3/Shift+F3 can cross pages to matches. Filters cover kinds/categories,
hidden entries and artifacts; Esc clears search but filters persist until reset.

Path entry accepts absolute, relative and `~`/`~/` locations without shell
expansion. Worker validation resolves explicit symlink activation. Regular files
open with `xdg-open`; directories navigate; special files are not opened.
Terminal launch uses the selected real directory, otherwise the current one.
Launch argv directly without a shell, close inherited descriptors and poll child
status without waiting. Distinguish successful status notices from actual launch
failures. Normal Esc cancels transient state and does not quit.

## File operations

Explicit file actions support one selected regular file, directory or symlink.
Synthetic groups must be expanded to select a real member. Cut records the
source path and cached device/inode/change-time identity in an internal session
clipboard; it does not alter the filesystem. Paste moves that entry into the current real
directory. Ctrl+X/Ctrl+V, Ctrl+M and Delete mirror the sidebar controls and must
not intercept editing shortcuts in text fields. Esc clears a pending cut or
cancels a confirmation, without cancelling an operation already in progress.

Move asks for an existing destination directory; names are retained. Delete asks
for explicit confirmation of the captured source and permanently removes it,
recursively for directories. There is no Trash or undo. Symlinks, including
broken links, are moved/unlinked without traversing their targets. Root paths,
special files, stale/replaced sources and directory-into-descendant moves are
refused. Existing destination names, including dangling symlinks, are never
replaced; Linux renameat2 with RENAME_NOREPLACE enforces that at publication.

The existing single filesystem worker performs mutations alongside reads and
path validation. Cross-filesystem moves stage a private complete copy, verify
that source metadata/tree membership has not changed, publish without replacement
and only then remove the source. Copies preserve permissions, modification times
and link target text; ownership, ACLs, extended attributes and hard-link topology
are not preserved. Concurrent external writers remain a limit; this is not a
filesystem transaction. Copy failures retain the source. Partial deletion or
source-removal failures report errors and identify any complete destination copy.
Successful moves or deletion of the cut source clear the matching clipboard;
failures retain it.

Navigation actions wait while a mutation is running; camera rendering stays
responsive. Confirmations bind a captured source rather than a mutable selection.
Results invalidate obsolete worker reads and cached history worlds, refresh the
current real directory and previews, and preserve surviving layout anchors and
the camera/grid frame. Active groups return to their real directory before
regrouping. Explicit refreshes prune removed entries; ordinary preview/full-read
disappearance still retains an error entry. No automatic filesystem watcher is
introduced.

## Demonstration and recording

The C++ tour creates a fresh disposable real-filesystem fixture and drives normal
browser actions. It has 55 actions and 31 numbered features plus the unnumbered
“A guided tour of fv” and “Tour over”. Generate
[docs/FEATURE_TOUR.txt](docs/FEATURE_TOUR.txt) from that same plan. Omit silent
actions from the text; shared feature captions span multiple actions. Four nested
levels demonstrate descent/ascent with one description and no depth count.
Wait for loading/camera settling; fail clearly on missing targets or timeouts.

Explain file opening with a selected document and inspector, without launching
an editor. Recording uses a private, self-closing `urxvt` helper for terminal cwd;
`--demo-no-launch` disables that external step for previews. Demo creation must
refuse existing reserved output files and never modify personal files.

The optional Python recorder uses installed FFmpeg and real X11 rendering;
Xvfb is optional. Explicit sizes, including square/nearly square, request a
centered borderless demo window and capture its actual stable client rectangle.
Reject a requested size changed by the WM; place the terminal over the capture.
On i3, narrowly matched floating rules preserve geometry. Preserve failed-capture
logs/partial video and only finalize MP4 after completion, helper cwd validation
and FFprobe checks. Never replace rendering with synthetic frames or add these
tools as normal application build dependencies.

## Limits and future work

No Trash/undo, multi-selection, external file clipboard, watcher/automatic refresh,
filesystem-wide index, Unicode-normalized search, persistent settings/usage
learning, user theme
system or `LS_COLORS` integration exists. These are future work, not requirements
to implement during maintenance. Keep future behavior local and transparent.
Analysis/merging still run on the main thread after worker reads and may pause
for huge listings. Cached pages/worlds retain memory; blocked kernel I/O may
delay worker shutdown. GPU/frame profiling and visual acceptance require X11.
Source and tests define exact defaults; [PROJECT_STATUS.md](PROJECT_STATUS.md)
records what has actually been validated and the next authorized task.

Manual X11 checks should cover inscription readability/truncation while orbiting,
grid fades and deep boundary reversals, dense pages/sequence expansion, shared
selection/search/history, successful desktop opening/terminal cwd, and square
window/terminal placement during an actual recording.
