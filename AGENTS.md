# Agent implementation rules

## Start here

Read [README.md](README.md), [DESIGN.md](DESIGN.md) and
[PROJECT_STATUS.md](PROJECT_STATUS.md) before substantial work. README is the
usage guide; DESIGN is the product contract; PROJECT_STATUS is the current
handoff. Keep the status document a current snapshot, not an appended change log.

The application already uses real filesystem data. Milestones 1–6 are complete;
do not reapply the old mock-only milestone restrictions. Mocks remain important
for regression tests. Scope changes to the user's request and avoid unrelated
refactoring. Preserve a buildable repository and document intentional design
changes. Git history retains superseded milestone details.

## Platform and dependencies

- Keep C++20, SDL2, OpenGL 3.3 Core, Dear ImGui and readable target-based CMake.
  Native Linux/X11 is required; do not silently switch to Wayland or headless
  rendering. Do not replace the stack without a specific technical reason.
- System packages belong in the Void Linux Podman image. Never install/download
  supplemental programs during development or from CMake/scripts. If a required
  dependency is absent, stop dependent work, identify the program and expected
  Void package, explain why it is needed and let the user rebuild the image.
  Do not introduce a workaround for the missing dependency.
- Use the standard library/existing components before adding dependencies. Keep
  any intentional vendoring small; additional Git submodules require approval.
- `third_party/imgui` is an existing Git submodule. Compile its sources and the
  standard SDL2/OpenGL3 backends directly. Do not use FetchContent, download it
  during configuration/build or replace it with a system ImGui package. Preserve
  its upstream license and pinned revision.

## Layer boundaries

| Layer | Responsibility / boundary |
| --- | --- |
| `filesystem/` | Paths, metadata, errors, reader/cache and worker; no rendering knowledge |
| `analysis/` | Categories, salience, artifacts, series/groups and cached query matching; ordinary data, no drawing |
| `layout/` | Stable nested placements, transforms, bounded pages and disclosure; no filesystem scans |
| `graphics/` | Shaders, geometry/buffers, text, camera and picking; no semantic filesystem heuristics |
| `navigation/` | Shared selection, directory/group contexts, paths and history; request semantic focus/navigation |
| `ui/` | Semantic presentation and conventional controls/inspector; no filesystem traversal |
| `app/` | Orchestrate workers, browser state, SDL lifecycle, demo and isolated desktop launching |

The existing single cancellable filesystem worker is sufficient unless a concrete
need proves otherwise. Do not add thread pools or bury asynchronous/animation
state in unrelated rendering code. Keep ownership explicit with RAII, const
correctness, value types and understandable APIs; avoid clever templates and
speculative abstractions.

## Behavior to preserve

- Keep shallow 2.5D navigation, limited orbit/zoom, reset and explicit short,
  deterministic, interruptible transitions. No WASD free flight or uncontrolled
  roll. Preserve parent/child zoom continuity and camera/grid mapping at rebases.
- Preserve loaded anchors, bounded dense-directory pages, expandable groups and
  progressive detail. Never draw thousands of equally prominent objects or
  recursively scan the whole filesystem at startup. Do not move stable objects
  when previews complete or queries change.
- Keep salience deterministic and replaceable, modification recency continuous,
  generated/hidden penalties distinct from exclusion, and grouping conservative.
  These decisions belong in analysis, not shaders or OpenGL code.
- Use simple semantic geometry, restrained colors and object-surface names per
  DESIGN. Keep only current-level names, matching lettering proportions and
  full metadata in the inspector. Do not restore floating/billboard labels.
- Avoid per-frame filesystem access, unnecessary allocations, analysis/layout
  rebuilds or static geometry uploads. Cache where useful; profile before adding
  complicated optimization.
- Treat permission failures, missing entries/mounts, unreadable directories and
  broken/cyclic links as normal runtime conditions. Never terminate unnecessarily
  or hide failures as successful empty listings. Keep symlink behavior explicit.
- No destructive user-file operations without a user request. Test/demo fixture
  writes must be disposable and refuse existing outputs. Desktop launch uses argv,
  not shell interpolation; distinguish success messages from failures.
- Keep the normal application independent of recording tools. The recorder must
  capture actual rendering, preserve failed outputs, and never alter user desktop
  associations or terminal preferences. Retain caption-only editor demonstration.

## Verification and documentation

For code changes, build after meaningful edits and run applicable independent
logic tests. README contains Debug/Release and Python commands. Do not require
OpenGL for pure analysis/layout tests. The feature-tour test uses harmless
`/bin/true` and `/bin/false` helpers, not graphical editors/terminals.

For documentation-only changes, check links, examples, generated narration parity
where relevant, and `git diff --check`; an unchanged C++ build need not be rerun.
Do not claim visual behavior or video capture was verified without actually
running and inspecting it on X11. Report the tested scope and remaining limits.
Keep dependency requirements and environment-specific blockers in the current
handoff rather than preserving obsolete “latest” results across documents.

## GitHub repository

The project is published to `git@github.com:operatingonhi/fv.git` on `master`,
which tracks `origin/master`. Keep the branch name `master` per the user's
request. The original GitHub MIT LICENSE commit is preserved through a merge
of both histories; retain that history and license. Never force-push or reset
away local work. GitHub's default branch remains the original license-only
`main`; see PROJECT_STATUS.md for the publication state.
