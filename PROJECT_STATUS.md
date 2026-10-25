# Implementation checkpoint

## Milestones and validation

- Milestones **1–5 complete, manually tested and accepted** by the user.
- **Milestone 6 implemented; manual X11 validation remains.** Scope is targeted
  refinement of the accepted implementation. No later milestone work, new
  dependency, alternate explorer, destructive operation, or architectural rewrite.
- See `MILESTONE6.md` for the audit, changes, measured stress results, limitations,
  and focused manual checklist. Source and tests remain authoritative.

## Architecture and current behavior

- C++20, SDL2/native X11, OpenGL 3.3 Core, Dear ImGui submodule and explicit CMake
  targets. Existing filesystem/analysis/layout/graphics/navigation/UI boundaries
  are retained.
- One cancellable worker enumerates directories and validates paths. Active
  directories fully load; eight visible children receive at most 64-entry
  previews. No automatic link traversal or whole-tree scan.
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
  Distant files simplify, distant labels are omitted, and nested traversal is
  bounded. Idle frames skip LOD and geometry rebuilding. Scene preparation time
  and box/label counts appear in Settings; GPU uploads remain revision-based.
- Session Settings expose animation enable/duration, orbit/zoom sensitivity,
  label density and nested detail depth. Hidden/artifact controls remain in
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
filesystem index, or seamless rebasing across bounded worlds. Analysis/merging
still run on the main thread after asynchronous enumeration. Extremely large
listings can therefore cause a preparation pause. Cached history/worlds retain
memory. Kernel-blocked filesystem calls may delay worker shutdown. Previews are
samples, not globally ranked listings. Settings are session-only. Visual feel,
clutter, labels, grouping silhouettes, and window-manager behavior require manual
X11 inspection before accepting Milestone 6.
