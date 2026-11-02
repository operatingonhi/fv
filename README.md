# Spatial filesystem navigator

A lightweight native Linux/X11 filesystem browser with a retro-futuristic 2.5D
scene, nested directories, animated navigation and relevance-based presentation.
The guided tour calls it **Jurassic Explorer**; the executable is
`spatial-navigator`. Milestones 1–6 are complete; the application uses real files.

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
| Optional recording | Python 3, `ffmpeg`/`ffprobe` with x11grab and libx264, `urxvt` |
| Optional isolated recording | `Xvfb` from `xorg-server-xvfb` |

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
python3 -B tests/record_demo_tests.py
./build/spatial-navigator                 # current directory
./build/spatial-navigator ~/projects      # optional starting path
```

For Release, use `build-release` and `-DCMAKE_BUILD_TYPE=Release`. Rebuild after
C++ changes; Python-script-only changes do not require rebuilding. The nine CTest
suites and eight recorder tests need no display. Successful tests do not establish
visual correctness. Optional X11 smoke test:

```sh
./build/spatial-navigator ~/projects --smoke-test /tmp/spatial-smoke.ppm
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
- No destructive file operations, automatic refresh/watchers, filesystem-wide
  index or persistent preferences are implemented. Very large listings can pause
  main-thread analysis after asynchronous enumeration.

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
| Esc | Cancel transient UI/search/pending navigation; does not quit normal browsing |

Close the normal application through the window manager. Regular files open via
`xdg-open`. Terminal precedence is `FSN_TERMINAL`, then `TERMINAL`, then
`xdg-terminal-exec` with `xterm` fallback. Commands support quoted arguments but
no shell expansion. Missing helpers and failed child processes report errors.

## Automated tour and video

The tour has 55 actions, 31 numbered descriptions and unnumbered opening/closing
titles. It traverses four nested directory levels. Routine actions run without
captions; descent/ascent share “Descend and ascend through nested directories.”
File opening is explained without launching an editor; the recorder's `urxvt`
terminal closes automatically. Normal manual opening remains available.

```sh
./build/spatial-navigator --demo /tmp/spatial-preview --demo-no-launch
python3 scripts/record_demo.py --check
python3 scripts/record_demo.py --size 1000x1000 --output /tmp/spatial-square-recording
```

Use a fresh output directory. `--size` sets the window and capture dimensions;
`1000x900` is a nearly square alternative. Without it the window requests the
full first display. i3 needs floating rules for the demo and terminal. Captures
include other windows overlapping the recorded client area. See
[docs/RECORDING.md](docs/RECORDING.md) for i3 rules, Xvfb, outputs and diagnostics;
[docs/FEATURE_TOUR.txt](docs/FEATURE_TOUR.txt) is generated from the C++ tour plan.
