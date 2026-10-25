# Implementation checkpoint

## Milestones and validation

- Milestones **1–4 complete and manually accepted under X11**; user committed that baseline.
- **Milestone 5 implemented, pending manual GUI acceptance.** Adds read-only browser controls without an alternative explorer view. Milestone 6 has not begun.
- Latest Debug and Release builds **passed all five test suites**; `git diff --check` passed. GUI smoke launch stopped at `Initialize SDL2: x11 not available`, so Milestone 5 visuals/input and successful desktop/terminal launches remain unverified. Launch helpers and ASan runtime are unavailable here.

## Architecture and source map

- C++20, CMake, SDL2/native X11, OpenGL 3.3 Core, Dear ImGui submodule; no new third-party dependency.
- `filesystem/model.*`: stable IDs and cached metadata. `reader.*`: one cancellable worker for enumeration and path validation, tagged results/generations. Current directory fully loads; at most eight visible children receive 64-entry previews. No automatic symlink traversal or whole-tree scan.
- `analysis/analysis.*`: semantic shapes, bounded continuous recency, contextual artifact/hidden deweighting, numeric-token series, conservative related groups and artifact cohorts. `query.*`: cache-only filename search and semantic filters; no rendering or I/O.
- `layout/{layout,hierarchy}.*`: persistent local transforms, nested dioramas, stable anchors, 20-object pages plus ranked summary; priority-based disclosure with hysteresis. Preview geometry depth three, total nesting six including groups.
- `navigation/`: shared entry/group selection, contexts/pages, screen-direction neighbor scoring, lexical path parsing and bounded history. `graphics/`: existing camera transitions/orbit/zoom, drawing/picking. `ui/`: semantic presentation, dimming/disclosure, existing sidebar selector, path/search/filter controls, concise selection metadata and a titleless scrollable inspector within the sidebar (Inspect/Ctrl+I; Esc dismisses).
- `app/browser*`: orchestration, deferred ancestor updates, cached worlds and history view snapshots. Same-world navigation preserves nested zoom; unrelated paths and world-root/depth boundaries use controlled changes. `desktop.*`: isolated argv-based `posix_spawnp`, cwd setup, descriptor cleanup and nonblocking exit polling. `main.cpp`: SDL lifecycle/input/RAII.
- Tests: `tests/{camera,mock,filesystem,analysis,browser}_tests.cpp`; `ctest --test-dir build --output-on-failure`, also `build-release`. Includes dense fixtures, shared/directional selection, paths/history, cache-only queries and launch construction/failures.

## Controls and runtime

- Click scene/sidebar: select and focus; F: focus selection; arrows: screen-direction selection; Tab/Shift+Tab: next/previous focus; Home/End: first/last eligible page item.
- Enter/double-click: enter directory, expand group or open file; Backspace: parent; Page Up/Down: pages; Alt+Left/Right: history.
- Ctrl+L: path; Ctrl+F: search; F3/Shift+F3: matching objects across pages; Ctrl+Shift+F: filters; Ctrl+H: hidden toggle; Ctrl+I: inspector; Ctrl+Shift+T: terminal here.
- Esc cancels transient UI/pending navigation and clears search; filters persist. No quit key. Window-manager/SDL close shuts down normally.
- Right-drag orbit, wheel zoom, R overview; orbit cannot interrupt transitions, zoom can. Editors/popups consume scene keys.
- `./build/spatial-navigator [directory]`, default cwd; optional `--smoke-test [capture.ppm]` follows path.
- Files use `xdg-open` (Void runtime package `xdg-utils`). Terminal: `FSN_TERMINAL`, then `TERMINAL`, else `xdg-terminal-exec`/`xterm` fallback. Quoted argv, no shell expansion; child cwd is selected actual directory or current directory.

## Limits and deferred work

- Search: case-insensitive filename substring in current cached context/all pages plus two cached descendant levels; incomplete previews indicated, no search-triggered scan/index. Nonmatches dim/unselectable; matching containers retain context. Filters preserve coordinates.
- History: 64 directory/group visits, shared world caches, selection/page/query/camera restoration; invalid targets report errors and are skipped later. Snapshots have no watcher/automatic refresh or fine-grained cache eviction.
- Explicit link activation resolves targets; previews never traverse links. Cached metadata may become stale; activation/path/history validate targets. Kernel-blocked calls can delay worker shutdown.
- Starting-root parent navigation/depth overflow creates another bounded world; seamless rebasing remains deferred. Preview samples are not globally ranked listings.
- Intentionally deferred: seamless rebasing, refresh/watchers/cache eviction, Unicode-aware search/indexing, profiling and further visual/performance refinement. No alternative list explorer, internal viewers/editors, destructive operations, ML or advanced dependency analysis.

## Next step

Manually validate Milestone 5 using the README checklist: keyboard/mouse/sidebar consistency, opening, queries/filters, paths/history, inspector, terminal cwd, nested animations and normal window-manager close. After acceptance and explicit authorization, next milestone is **6 — Refinement**. Read AGENTS.md and DESIGN.md first.
