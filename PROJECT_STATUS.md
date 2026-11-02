# Current handoff

Updated 2026-11-02. Read this with README.md, DESIGN.md and AGENTS.md after the
container restart. Source/tests and current Git state remain authoritative.

## Next task: finish the authorized GitHub upload

The user requested publication to `git@github.com:operatingonhi/fv.git` and is
rebuilding the Void Linux container with `openssh`. After reopening it, they will
relaunch Codex and ask to continue. Resume that upload, not feature development.

State at handoff:

- `origin` is configured to that SSH URL; the local branch is `master`.
- The implementation is committed. `e55ca72` adds the recorder scripts, tour text,
  recording guide and ignore rules on top of `2bea4f0` (`V0.0`). The documentation
  consolidation/cleanup is a subsequent local commit; inspect `git log -1` and
  `git status` for the current tip instead of assuming an old hash is HEAD.
- Nothing has been fetched from or pushed to GitHub. `git ls-remote` failed with
  `cannot run ssh: No such file or directory`. Missing program: `ssh`; expected
  Void package: `openssh`. GitHub authentication/key/agent access is untested.
- The user reports the remote contains only an MIT license created on GitHub.
  Its actual contents and default branch have not been fetched or verified.
  Preserve that remote commit and LICENSE when joining histories.
- Build directories and Python caches are ignored. Dear ImGui remains a pinned
  upstream submodule; do not add its checkout as ordinary project files.

Resume sequence, already authorized by the user:

1. Inspect `git status --short`, `git log -3 --oneline`, `git branch -vv`,
   `git remote -v` and `git submodule status`. Preserve any new user edits.
2. Confirm `ssh` exists and GitHub authentication works through the user's normal
   SSH key/agent. Never print private keys or install packages in the running
   container. If credentials/access are unavailable, report the exact blocker.
3. Run `git ls-remote --symref origin HEAD` to learn the remote default branch,
   then `git fetch origin`. Inspect the remote commit and MIT license.
4. Merge `origin/<remote-default-branch>` with `--allow-unrelated-histories`,
   preserving local project history and the GitHub LICENSE. Resolve any actual
   conflicts without discarding either side; do not force-push or recreate the
   license from memory. Align the local branch name with the remote default if
   appropriate; do not assume it is `main`.
5. Push the integrated branch to the remote default branch and set its upstream.
   Verify the remote tip matches the local tip, the LICENSE is present, the
   submodule reference is retained and the working tree is clean.
6. Replace this pending-upload handoff and AGENTS's temporary GitHub handoff with
   the actual publication result/remaining work; commit and push those updates.
   No additional publication confirmation is required after the user asks to
   continue; upload authorization already exists.

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
this agent. In the pre-rebuild container, `ssh`, FFmpeg/FFprobe, `urxvt` and Xvfb
were absent; prior SDL startup reported `x11 not available`. Recheck the rebuilt
environment instead of assuming those package/display failures still apply.

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
