# Implementation checkpoint

## Milestones and validation

- **1 — Rendering foundation:** complete; user manually validated and accepted under X11.
- **2 — Mock spatial filesystem:** complete; user accepted nested dioramas, continuous parent-child zoom, orbit, semantic shapes/colors, compound sequences, grouping, and basic salience.
- **3 — Real filesystem integration:** implemented; Debug and Release builds and all three automated suites pass. Includes temporary fixtures with 600 files, metadata/errors, symlinks, progressive loading, and navigation regressions. **Manual GUI validation and user acceptance are still pending.**
- This execution environment cannot connect to X11. Do not treat that as a build failure or claim GUI verification from automated tests. ASan runtime is unavailable.

## Implemented architecture and source map

- C++20, CMake, native X11 through SDL2, OpenGL 3.3 Core, and Dear ImGui submodule. No new third-party dependency for filesystem loading.
- `src/filesystem/model.*`: main-thread metadata cache with stable entry/parent IDs; `reader.*`: non-recursive `std::filesystem` reader and one cancellable `std::jthread`. Active directories load fully; up to eight visible child directories receive 64-entry previews. Descendants load on navigation, not through startup recursion. `mock_filesystem.*` remains for regression fixtures.
- `src/analysis/analysis.*`: semantic categories/shapes, continuous recency with seven-day half-life, explicit low-interest weights, numeric-token sequences anywhere in filename stems, and conservative name/timestamp groups. Renderer contains no extension heuristics.
- `src/layout/layout.*`, `hierarchy.*`: persistent parent-relative transforms, nested dioramas, cached pages, and stable placement during data updates. Default page: 20 objects plus a remainder summary. Visual previews extend three levels; total nesting cap is six, including groups.
- `src/app/browser.*`: connects loading results, analysis, hierarchy, semantic navigation, and camera targets. Enclosing-directory updates wait while their groups/descendants are active, then apply on return to avoid detaching the current context.
- `src/navigation/navigation.*`: context/selection/page/parent stack. `graphics/camera.*`: explicit smoothstep transitions and scale-aware orbit/zoom/clipping; `renderer.*` and `picking.*`: drawing and picking. `ui/presentation.*`, `overlay.*`: semantic geometry, LOD, labels, metadata/status. `app/main.cpp`: CLI, SDL events, and RAII shutdown.
- Tests: `tests/camera_tests.cpp`, `mock_tests.cpp`, `filesystem_tests.cpp`; run with `ctest --test-dir build --output-on-failure`.

## Current controls

- Click: focus; double-click/Enter: enter directory or expand group. Regular files only focus.
- Tab: next object; F: focus selection; Backspace/Parent: enclosing context; Page Up/Down: pages.
- Right-drag: bounded orbit; wheel: zoom; R: current overview; checkbox: disable subsequent animations. Orbit cannot interrupt transitions; zoom can.
- Window-manager close/SDL quit: exit with resource cleanup. **Esc is unassigned; no custom quit key.**
- Run `./build/spatial-navigator [directory]`; default is the current working directory. Optional `--smoke-test [capture.ppm]` follows the directory argument.

## Limits and deferred work

- Starting path is the spatial-world root; cannot navigate above it or beyond the existing nesting cap.
- Discovered symlinks are selectable cyan linked forms with target/error metadata, never traversed. An explicitly supplied starting directory may be a symlink. Link size/mtime are unavailable rather than borrowed from targets.
- Snapshots and visited metadata are cached; no watcher or automatic refresh. Preview sampling is not globally salience-ranked; promotion preserves known anchors/order before appending new content.
- Kernel-blocked filesystem calls cannot be interrupted by the stop token and may delay worker shutdown.
- File opening, destructive operations, search/filtering, broader history, deeper rebasing, and advanced salience/clustering remain deferred.

## Next checkpoint

First manually validate Milestone 3 under X11: real nested contents and zoom continuity, preview completion, large-directory pages, both sequence patterns, shapes/colors, metadata, unreadable directories, symlinks, and normal window closing. Detailed checklist is in `README.md`.

Next planned milestone is **4 — Salience and Grouping refinement**, after Milestone 3 validation/acceptance and explicit authorization. No Milestone 4 implementation has begun. Read `AGENTS.md` and `DESIGN.md` before further code changes.
