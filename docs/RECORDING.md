# Automated feature tour and video recording

This presentation-only tour drives fv's real browser actions without keyboard
or mouse input. It creates its own filesystem fixture and leaves personal files
alone. The ordered narration is in [FEATURE_TOUR.txt](FEATURE_TOUR.txt):
31 numbered features plus
the unnumbered opening and closing titles. The 55 underlying actions include
routine steps without captions. Terminal setup shares one caption; descent and
ascent through four nested levels share another, without a depth count in the
caption. A run takes approximately three minutes, plus loading and transition time.
There is no audio or synthesized voice.

The tour covers relevance/recency ranking, semantic shapes, focus and inspection,
orbit, zoom/reset, sequences and member selection, bounded large-directory pages,
Tab/Shift+Tab paging, search across pages, filters/hidden files, symlink metadata,
a caption-only explanation of file opening, a terminal working directory,
four levels down and back up with continuous grid refinement, history,
relative path entry and settings.

## Prerequisites

Build the application using the normal CMake instructions. Recording additionally
uses already-installed programs; nothing is downloaded or installed by these
scripts or the build:

| Requirement | Expected Void package | Purpose |
| --- | --- | --- |
| Python 3 | `python3` | Recorder orchestration and desktop demo helpers |
| FFmpeg and FFprobe, including `x11grab` and `libx264` | `ffmpeg` | Capture the X11 display, encode and validate MP4 |
| `urxvt` | Existing local terminal installation | Show the selected terminal directory |
| Native X11 access and OpenGL 3.3 | Existing application requirements | Render the actual application |
| Optional `Xvfb` | `xorg-server-xvfb` | Provide an isolated X11 display with GLX support |

Update the development-container image and rebuild it if tools are missing.
For an existing display, expose its X11 socket and authorization as usual.
Each capture dimension must be at least 720 pixels. A 1000x1000 window fits
comfortably on a 1920x1080 desktop and produces a square video for editing.
Choose a size that fits your display, leaving room for any panels or borders.

## Record on an existing X11 desktop

After C++ source changes, rebuild the existing application before recording:
the tour actions are part of the compiled executable. This is an incremental
build; changes only to the Python scripts do not require it.

```sh
cmake --build build -j2
python3 scripts/record_demo.py --check
python3 scripts/record_demo.py --output /tmp/spatial-feature-recording
```

The output directory must not exist. Use a dedicated workspace. Without `--size`,
the browser requests the full first display. With `--size WIDTHxHEIGHT`, it
requests a centered window of that size; the video records the actual client
rectangle after window-manager placement settles. External windows over that
rectangle are included. Other windows or notifications in that
rectangle would also appear. Do not move/resize the browser during capture.
Escape or closing the browser aborts the tour; aborted runs are not reported as
completed videos. The recorder quits FFmpeg and the application automatically.

## Square recording on i3

Add these rules to your i3 configuration, then reload it (`i3-msg reload`, or
your configured reload shortcut) before launching the recorder:

```i3
for_window [title="^fv - Feature tour$"] floating enable, border none
for_window [title="^fv: Terminal working directory$"] floating enable
```

Floating lets the application request a square client area; tiling would resize
it to the available tile. The terminal rule keeps its opening from resizing the
browser. These rules match only the demonstration windows. See the
[i3 guide on window rules](https://i3wm.org/docs/userguide.html#for_window).

Then run:

```sh
cmake --build build -j2
python3 scripts/record_demo.py --size 1000x1000 --output /tmp/spatial-square-recording
```

`--size 1000x900` is another option for a nearly square result. The recorder
captures that area directly; it does not stretch a widescreen scene. The compact
terminal is positioned relative to the capture rectangle. Keep the demo window
stationary during recording. If i3 changes the requested size, the run stops with
a diagnostic; check the floating rules and any configured floating size limits.
Your monitor resolution and aspect ratio remain unchanged.

## Record on an isolated X11 display

If Xvfb has been provided in the container image, no existing desktop or window
manager is needed:

```sh
python3 scripts/record_demo.py --xvfb :99 --size 1000x1000 --check
python3 scripts/record_demo.py --xvfb :99 --size 1000x1000 --output /tmp/spatial-feature-recording
```

Use an unused display number. The recorder refuses an existing display/socket,
starts only its own Xvfb with TCP disabled, and stops it afterward. SDL still
uses the native `x11` driver and the normal OpenGL renderer; there is no dummy or
replacement rendering backend. The X server must provide working GLX/OpenGL 3.3.
The `--check` command checks tools but does not start a virtual display.
Here `--size` also sets the virtual display size; the default without it is
1920x1080. A square virtual display avoids desktop tiling and fits a square
capture even when your physical display is shorter than the desired video.

## Outputs

- `tour.mp4`: silent 30 fps H.264 video, finalized only after a completed tour,
  a successful terminal working-directory marker and FFprobe validation.
- `FEATURE_TOUR.txt`: ordered narration matching the executable step plan.
- `tour.tsv`: action start times and captions relative to the tour start;
  uncaptioned actions have empty captions and combined features repeat their
  caption. Loading and camera transitions make timing variable. The video has
  a short initial lead-in.
- `demo-root/`: the real disposable sample directory tree used for the recording.
- `application.log`, `ffmpeg.log`, optionally `xvfb.log`: diagnostic output.
- `video-info.json`: encoded video metadata.

Incomplete recordings remain named `tour.partial.mp4`. Existing output paths are
never reused. No cleanup removes or overwrites an earlier recording.

The file-opening step selects `README.txt`, displays its inspector and explains
that regular files can be opened with their desktop association. It does not
launch an editor or call `xdg-open`, and no editor-launch marker is required.
Normal manual file opening remains unchanged.

The recorder sets private XDG locations and `FSN_TERMINAL` for its child processes;
it does not install a text/plain desktop association. The terminal step opens
`urxvt` and runs a shell's `pwd` and `ls` in the selected `deep-tree` directory.
That window closes automatically after seven seconds. Its marker verifies the
intended working directory. User desktop associations and terminal preferences
are unchanged.
Sample archive/object files provide metadata/classification fixtures and are
never executed or opened as archives.

## Run the tour without recording

```sh
./build/fv --demo /tmp/spatial-live-tour
./build/fv --demo /tmp/spatial-square-preview --demo-size 1000x1000
./build/fv --demo /tmp/spatial-preview --demo-no-launch
```

Each command needs a new output location or a location with none of the reserved
demo files. All commands keep file opening caption-only and need X11. Commands
without `--demo-no-launch` use normal terminal settings and launch a terminal;
`--demo-no-launch` labels the terminal step as disabled for previews. The program
exits automatically after all steps, and stops on a missing target or load timeout.

Offline commands do not require a display:

```sh
./build/fv --write-demo-script /tmp/FEATURE_TOUR.txt
./build/fv --prepare-demo /tmp/spatial-tour-fixtures
```

The recorder's `--demo-wait` handshake waits until the initial reads finish and
capture is running before beginning the tour. This flag is intended for the
recorder, not ordinary live previews.

## Validation limits

Run the recorder's eight CPU-only Python checks with:

```sh
python3 -B tests/record_demo_tests.py
```

The CPU-only `feature-tour` test runs all 55 actions against the generated real
filesystem with external launches disabled. It checks silent actions, shared
captions, unnumbered titles, removed actions, deep traversal, paging,
cross-page search, sequence expansion, inspection/filter controls, recency ranking,
return to the root, narration parity and overwrite refusal. It does not verify
GUI appearance, the X11 recorder, terminal windows or video encoding.
An additional run enables desktop launching with `/bin/true` in place of a
terminal and verifies the whole tour continues after a successful launch.
Missing helpers and nonzero helper exits are checked separately. Ordinary
status messages, including successful terminal launches, do not abort the tour.

Environment-specific blockers and the latest validation results are recorded in
[PROJECT_STATUS.md](../PROJECT_STATUS.md). Recheck prerequisites after rebuilding
the container; automated tests alone do not verify GUI placement or video capture.
