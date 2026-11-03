# fv

fv is a lightweight native Linux/X11 filesystem browser inspired by fsv. It uses
a retro-futuristic 2.5D scene, nested directories, animated navigation and
relevance-based presentation to explore real files.

Read [DESIGN.md](DESIGN.md) for the product contract,
[AGENTS.md](AGENTS.md) for development rules, and
[PROJECT_STATUS.md](PROJECT_STATUS.md) for the current handoff and validation limits.

## Dependencies

Provide system packages when building the Void Linux development-container image;
the build and scripts never install them.

| Purpose | Packages / requirements |
| --- | --- |
| Build | `base-devel`, `cmake`, `SDL2-devel`, `MesaLib-devel`, `libglvnd-devel` |
| GitHub SSH access | Git and `openssh`, plus working GitHub SSH authentication |
| Run | Native X11 access (`DISPLAY`, socket and authorization), OpenGL 3.3 |
| Manual file opening | `xdg-open` from `xdg-utils`, configured desktop associations |

Dear ImGui is the existing `third_party/imgui` Git submodule, compiled directly
with its SDL2/OpenGL3 backends. Initialize it after cloning:

```sh
git submodule update --init third_party/imgui
```

## Build, test and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/fv                 # current directory
./build/fv ~/projects      # optional starting path
```

For Release, use `build-release` and `-DCMAKE_BUILD_TYPE=Release`. Rebuild after
C++ changes. The ten CTest suites need no display. Successful tests do not
establish visual correctness. Optional X11 smoke test:

```sh
./build/fv ~/projects --smoke-test /tmp/fv-smoke.ppm
```

## Current behavior

- One filesystem worker loads the active directory, its displayed immediate child
  directories, and bounded parent/sibling context without a whole-tree scan.
- Parent/child navigation zooms through existing nested geometry. Coordinate
  rebasing preserves continuity through deep navigation and ascent.
- Salience combines semantic usefulness, continuous modification recency and
  generated/hidden-file penalties. Sequences and conservative related groups form
  expandable structures. Dense contexts use bounded pages and Other entries.
- Names are attached to object faces. Only the current directory and its direct
  page entries have names; full filenames remain in the sidebar/inspector.
  Partial pages fill from the front; full pages span the directory floor.
- Search/filtering uses cached metadata, preserves placement and dims nonmatches.
  History restores context. Settings last for the session.
- Copy/paste, cut/paste, move and confirmed permanent deletion work on individual
  files, directories and symlinks. Completed moves remove the source object from
  cached parent/child views as well as refreshing the destination.
- No automatic refresh/watchers, filesystem-wide index or persistent preferences
  are implemented. Very large listings can pause main-thread analysis after
  asynchronous enumeration.

## Controls

| Input | Action |
| --- | --- |
| Click object/sidebar row; F | Select/focus; focus selection |
| Arrows; Home / End | Directional selection; first / last eligible page object |
| Enter / double-click; Backspace | Activate file/directory/group; parent context |
| Tab / Shift+Tab; Page Up / Down | Wrap pages; bounded previous / next page |
| Right-drag; wheel; R | Orbit; zoom; reset view |
| Ctrl+L; Alt+Left / Right | Path entry; back / forward history |
| Ctrl+F; F3 / Shift+F3 | Search; next / previous match |
| Ctrl+Shift+F; Ctrl+H | Filters; toggle hidden entries |
| Ctrl+I; Ctrl+Shift+T | Metadata inspector; terminal at selected/current directory |
| Ctrl+C; Ctrl+X | Copy selection; cut selection |
| Ctrl+V | Paste the copied or cut entry into the current directory |
| Ctrl+M; Delete | Move to a chosen directory; open permanent-delete confirmation |
| Esc | Cancel transient UI/search/pending navigation; does not quit normal browsing |

The sidebar also provides Copy, Cut, Paste, Move and Delete buttons. Expand groups
to select individual members. Copy and Cut use fv's internal clipboard and only
record the source. Paste duplicates a copied entry or moves a cut entry. A copied
entry stays available for repeated pastes into other directories; a successful
cut/paste clears the clipboard. Esc clears either clipboard mode or cancels a
confirmation. These shortcuts leave text editing to the active text field.

Copy and move destinations must be existing directories; entries keep their names
and existing destination names are never replaced. Moving between filesystems copies
the entry before removing its source. Permissions and modification times are
preserved by copies; ownership, extended attributes, ACLs and hard-link
relationships are not preserved by copies, including cross-filesystem moves.
Wait for the result before another navigation action. Failures appear in the
sidebar, including partial source removal after a completed copy.

Delete requires confirmation and permanently removes a directory and its
contents. It does not use Trash and has no undo. Copying, moving or deleting a
symlink acts on the link itself. Confirmation cancellation leaves files untouched.

Close the normal application through the window manager. Regular files open via
`xdg-open`. Terminal precedence is `FSN_TERMINAL`, then `TERMINAL`, then
`xdg-terminal-exec` with `xterm` fallback. Commands support quoted arguments but
no shell expansion. Missing helpers and failed child processes report errors.
