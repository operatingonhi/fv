# Implementation checkpoint

## Milestones and validation

- **Milestones 1–3:** complete and manually accepted by the user under X11. This includes nested dioramas, continuous parent-child zoom, orbit, semantic geometry/colors, grouping, salience, and real filesystem integration.
- **Milestone 4:** implemented; **manual GUI validation and acceptance pending**. Debug and Release builds pass all four test suites, including 600-file and mixed 621-entry filesystem fixtures. No Milestone 5 implementation has begun.
- This execution environment cannot connect to X11; that is not a build failure. Never infer visual validation from automated tests. ASan runtime is unavailable.

## Architecture and source map

- Stack: C++20, CMake, SDL2/native X11, OpenGL 3.3 Core, Dear ImGui submodule. No additional third-party dependencies.
- `src/filesystem/model.*`: main-thread metadata cache with stable parent/entry IDs. `reader.*`: one cancellable `std::jthread`, non-recursive `std::filesystem` reads. Active directories load fully; at most eight visible child directories get 64-entry previews. `mock_filesystem.*` remains for tests.
- `src/analysis/analysis.*`: semantic categories/shapes; semantic baseline plus bounded recency (default .24, seven-day half-life); contextual artifact/hidden modifiers; cached directory characteristics; mean/peak group scores. Numeric-token series check padding, versions, sizes, parent/type; related groups require name family, extension, timestamps and compatible sizes. Eight leftover artifacts trigger an expandable group. No ML or persistent usage tracking.
- `src/layout/layout.*`, `hierarchy.*`: persistent parent-relative transforms and cached pages; 20 objects plus ranked remainder summary. Existing anchors remain stable during updates. Disclosure priority is separate from position; LOD uses hysteresis. Visual preview depth is three; total nesting cap is six, including groups/summaries. These limits do not trigger filesystem traversal.
- `src/app/browser.*`: loading/analysis/navigation orchestration. Enclosing updates are deferred while their groups/descendants are active, then applied on return. Cached child statistics update parent scores without moving dioramas.
- `src/navigation/navigation.*`: context, selection, pages and parent stack. `graphics/camera.*`: scale-aware transitions/orbit/zoom; `renderer.*`, `picking.*`: drawing/picking. `ui/presentation.*`, `overlay.*`: geometry, labels, metadata/status and group explanations. `app/main.cpp`: CLI, SDL events and RAII cleanup.
- Tests: `tests/{camera,mock,filesystem,analysis}_tests.cpp`; run `ctest --test-dir build --output-on-failure`. Classification/salience stay outside rendering; analysis/layout perform no filesystem I/O.

## Controls

- Click/F: focus; double-click/Enter: enter directory or expand group. Files only focus.
- Tab: next object; Backspace/Parent: enclosing context; Page Up/Down: pages.
- Right-drag: bounded orbit; wheel: zoom; R: overview; animation checkbox affects subsequent transitions. Orbit cannot interrupt transitions; zoom can.
- Window-manager close/SDL quit exits cleanly. **Esc is unassigned; no custom quit key.**
- Launch: `./build/spatial-navigator [directory]` (default: cwd). Optional `--smoke-test [capture.ppm]` follows the directory argument.

## Limits and deferred work

- Starting path is the world root; cannot navigate above it or past the nesting cap.
- Discovered symlinks show cyan linked geometry, target/errors, and unavailable link size/mtime; they are not traversed. An explicit starting path may be a directory symlink.
- No watcher/automatic refresh. Previews are samples, not globally ranked listings. Promotion preserves known ordering; ranked summaries expose important omitted entries. Heuristics deliberately leave ambiguous sequences/groups separate; no scored entry is excluded.
- Kernel-blocked filesystem calls can delay worker shutdown despite cooperative cancellation.
- File opening, search/filtering, broader history, destructive operations, deeper rebasing and advanced clustering remain deferred.

## Next step

Manually validate Milestone 4 using source repositories, build outputs, photos/sequences, session exports and miscellaneous dense directories. Check ranking, false positives, artifact/member access, summaries, LOD flicker, label clutter and spatial stability. Detailed checks: `README.md`.

Next planned milestone: **5 — Practical File Navigation**, only after Milestone 4 acceptance and explicit authorization. Read `AGENTS.md` and `DESIGN.md` before further code changes.
