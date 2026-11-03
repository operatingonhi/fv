# Current handoff

Updated 2026-11-03. Read this with README.md, DESIGN.md and AGENTS.md.
Source/tests and current Git state remain authoritative.

## GitHub publication

The project is published to `git@github.com:operatingonhi/fv.git` on `master`,
which tracks `origin/master`. The user explicitly requested keeping `master`.
SSH authentication, fetch and push succeeded after the user corrected the key.

- The remote initially contained only MIT LICENSE commit `a280522` on `main`.
  Merge commit `2d17172` joins that history with the local project history.
  LICENSE is byte-for-byte identical to the original GitHub file.
- Local implementation commits and the original remote commit are retained;
  no force-push was used. Inspect `git log -1` for the current tip.
- Dear ImGui remains a pinned Git submodule at
  `4e99c0ab1a28c2b3d1f250ec0266dbf1e455ef58`; build directories and Python caches
  remain ignored.
- The user set GitHub's default branch to `master` and deleted `main` through
  the website. Remote refs confirm both changes. Keep the branch name `master`.

## Project name and README

The application, executable and CMake project are named `fv`, inspired by `fsv`.
Window titles, usage text, scripts, narration and build/run examples use `fv`.
Internal `spatial` namespaces and the existing `FSN_TERMINAL` environment variable
remain unchanged. README describes normal browsing; the presentation-only tour
and its recording dependencies/commands remain in docs/RECORDING.md.

## Product state to preserve

Milestones 1–6 are complete and accepted. The native C++20/SDL2/X11/OpenGL 3.3/
ImGui browser uses real files; mocks are regression fixtures. Current code is
split across filesystem, analysis, layout, graphics, navigation, UI and app.

- One cancellable worker reads/validates paths. Active directories and displayed
  immediate children load fully; sibling/fallback contents use bounded previews.
  There is no recursive startup scan or automatic symlink traversal.
- Cached parent/sibling context, pinned anchors, deferred ancestor updates and
  page caching preserve placements. Three-level coordinate rebases, cached
  parent returns and unseen-parent promotion keep directory zoom continuous.
- Pages hold twenty objects, or nineteen plus Other entries. Series contain
  ordered selectable members; related/artifact cohorts remain reachable.
  Semantic usefulness, continuous recency and artifact/hidden penalties rank
  entries without permanently hiding them.
- Names are inscriptions on existing faces, with matching font/width proportions;
  the current-directory backdrop has the largest name. Only current-level names
  appear. Long names truncate; full metadata stays in the integrated inspector.
  Full pages span the floor; partial pages fill from the front. Natural sparse
  dispersion was rejected/deferred rather than approximated with grid holes.
- Procedural grid subdivisions fade in/out continuously with camera distance
  and preserve alignment through rebases/history. Geometry/text buffers update
  on scene revisions, not every camera frame.
- Cached search/filtering, shared selection, path entry, bounded history, desktop
  file opening and terminal cwd work through the normal browser paths. Esc does
  not quit normal browsing. Settings are session-only; there is no label-density
  setting, automatic refresh, persistent theme or filesystem-wide search index.

## File operations

Copy (Ctrl+C), cut (Ctrl+X), paste (Ctrl+V), move (Ctrl+M) and delete (Delete key)
are available through shared browser actions and sidebar controls. Actions operate
on one real selected file/directory/symlink; groups must be expanded. The internal session
clipboard distinguishes Copy from Cut. Paste duplicates Copy sources without
removing them and keeps them for repeated pastes; a successful cut/paste clears
its matching clipboard. Move asks for an existing destination directory. Delete
requires confirmation, is permanent and recursively removes directory contents;
there is no Trash or undo. Esc cancels a confirmation or either clipboard mode.

The existing filesystem worker performs I/O. Transfers refuse occupied destination
names atomically; copies and cross-filesystem moves use private staging and
source verification without following source symlinks. The worker checks
cached device/inode/change-time identity before mutation. In-progress actions
hold navigation until the result; rendering remains active. Failed paste retains
its clipboard source. Results invalidate old reads/history caches, prune removed
entries on explicit refresh and retain surviving anchors. Group mutation returns to the real
directory before regrouping; ordinary loading retains disappearance errors.
Successful moves/deletes additionally remove the confirmed source from active
and stashed cached hierarchies without regrouping an active ancestor. This fixes
the stale source graphic when pasting between parent/child contexts. Matching
includes the parent inode to preserve unrelated same-named hard links. Returning
to an invalidated parent requests a fresh listing. Copy preserves its source
graphics. Tests check generated scene boxes, cache contents, camera/grid
continuity, parent/child moves in both directions and deep rebase boundaries.

Copies (including cross-filesystem moves) preserve permissions, modification
times and symlink text, but not ownership, ACLs, extended attributes or hard-link
relationships.
Concurrent external changes cannot be made transactional. Permission errors may
leave partial deletion or a completed destination with source contents remaining;
these are reported as failures, never as successful moves. No multi-selection,
system file clipboard, automatic watcher or undo was added.

## Tour and recording preferences

The current tour in `src/app/demo.cpp` has 55 actions, 31 numbered descriptions,
“A guided tour of fv” and “Tour over” as unnumbered titles.
It descends/ascends four nested levels under one caption with no depth count.
Routine actions still execute silently; explicit skipped actions were removed.
`docs/FEATURE_TOUR.txt` must match generated output; edit the compiled plan when
changing actions/captions and regenerate the text.

The document step explains opening and shows details without starting an editor.
Manual opening still uses `xdg-open`. The terminal demonstration stays active;
the recorder specifically uses `urxvt`, showing cwd/entries and closing after
seven seconds. A previous abort was caused by treating a successful launch
notice as an error; the tour now checks a separate desktop-error channel.

The video is intended for a nearly square screen. `record_demo.py --size 1000x1000`
sizes the window and actual client-area capture; `1000x900` is another
option. The app flag is `--demo-size`. Reject WM size overrides before capture;
position the compact terminal relative to the recorded rectangle. i3 needs the
two floating rules in docs/RECORDING.md. Existing-display default requests the
full first display; optional Xvfb defaults to 1920x1080. Recording output must
be fresh. No supplemental programs may be downloaded/installed by the agent.

## Validation and remaining limits

File-operation validation: Debug and Release builds pass all ten CTest suites;
all eight Python recorder checks pass. The new suite uses disposable files and
tests real cross-filesystem moves between /tmp and the workspace, collisions,
symlinks, permissions/partial source removal, stale source identities, confirmation
cancellation, copy/paste reuse, cut/paste retry, stable anchors, group updates and deep cached
parent returns. Source-graphic regressions inspect hierarchy and generated scene
geometry in parent/child moves and verify separate hard links remain visible.
Generated narration matches both builds byte for byte. Local
Markdown links and `git diff --check` passed. Existing coverage includes
forty-level navigation/rebasing,
stable anchors, bounded large directories, grouping, inscriptions, reversible
grid fades, full four-level tour, harmless launch success/failure, square capture
offsets and refusal of a WM size override. Stress tests use 14,719 cached entries.
These are CPU/process/mocked-capture checks. A native X11/OpenGL smoke test also
passed and its captured sidebar was visually inspected for the Copy button and
file controls. Clipboard shortcuts and move/delete dialogs have not been driven
interactively on X11; behavior is covered through browser-action tests and
source removal is checked in generated scene geometry.

The user has run recording locally and reported the terminal opened/closed.
The latest i3/square capture and final video have not been visually verified by
this agent. OpenSSH is installed and GitHub access works; native X11/OpenGL
startup, rendering and shutdown passed the smoke test. Xvfb is absent; availability
of FFmpeg/FFprobe and `urxvt` has not been rechecked. Verify recording prerequisites
before making another video.

Remaining implementation limits: main-thread analysis after asynchronous reads
can pause on huge listings; caches retain memory; no automatic refresh/watchers,
fine-grained eviction, Unicode-normalized search, persisted settings or usage
learning. A blocked kernel filesystem call can delay worker shutdown.
