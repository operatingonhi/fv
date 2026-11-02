# Current handoff

Updated 2026-11-02. Read this with README.md, DESIGN.md and AGENTS.md.
Source/tests and current Git state remain authoritative.

## GitHub publication

The project is published to `git@github.com:operatingonhi/fv.git` on `master`,
which tracks `origin/master`. The user explicitly requested keeping `master`.
SSH authentication, fetch and push succeeded after the user corrected the key.

- The remote initially contained only MIT LICENSE commit `a280522` on `main`.
  Merge commit `2d17172` joins that history with the local project history.
  LICENSE is byte-for-byte identical to the original GitHub file.
- Local implementation commits and the original remote commit are retained;
  no force-push was used. Inspect `git log -1` for the current documentation tip.
- Dear ImGui remains a pinned Git submodule at
  `4e99c0ab1a28c2b3d1f250ec0266dbf1e455ef58`; build directories and Python caches
  remain ignored.
- GitHub's default branch remains the original license-only `main`. The project
  is on `master`; setting the repository's default branch to `master` in GitHub
  settings remains a separate step. Do not rename the local branch to `main`.

Publication changed Git history and documentation only. No application source
changed, so the C++ builds and runtime tests were not rerun. Documentation checks
cover local Markdown links and `git diff --check`; final Git checks cover remote
tip equality, retained ancestry, LICENSE and the submodule reference.

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

## Tour and recording preferences

The current tour in `src/app/demo.cpp` has 55 actions, 31 numbered descriptions,
“A guided tour of the Jurassic Explorer” and “Tour over” as unnumbered titles.
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

Last code-change validation: Debug and Release builds passed all nine CTest
suites; all eight Python recorder checks passed. Generated narration matched the
checked-in text byte for byte. Coverage includes forty-level navigation/rebasing,
stable anchors, bounded large directories, grouping, inscriptions, reversible
grid fades, full four-level tour, harmless launch success/failure, square capture
offsets and refusal of a WM size override. Stress tests use 14,719 cached entries.
These are CPU/process/mocked-capture checks, not GUI/video acceptance.

The user has run recording locally and reported the terminal opened/closed.
The latest i3/square capture and final video have not been visually verified by
this agent. OpenSSH is installed and GitHub access works. Availability of
FFmpeg/FFprobe, `urxvt`, Xvfb and a working native X11 display has not been
rechecked in the rebuilt container; verify these before recording or GUI tests.

Remaining implementation limits: main-thread analysis after asynchronous reads
can pause on huge listings; caches retain memory; no automatic refresh/watchers,
fine-grained eviction, Unicode-normalized search, persisted settings or usage
learning. A blocked kernel filesystem call can delay worker shutdown.

Documentation cleanup consolidates the obsolete milestone audit and historical
follow-ups into these current references. `MILESTONE6.md` and generated Python
bytecode caches were removed; useful source/tests, ImGui and recording docs were
retained. Cleanup validation checked thirteen local Markdown links, documented
CLI flags, nine registered CTest suites, eight recorder test definitions,
narration parity in both builds and `git diff --check`. No C++ code changed and
the already-passing code test suites were not rerun for this documentation edit.
