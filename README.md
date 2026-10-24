# Spatial filesystem navigator

Milestone 1: a native X11, SDL2 / OpenGL 3.3 Core rendering prototype.
Two fixed mock directory platforms and four file prisms sit on a perspective
grid. Dear ImGui provides the path/status overlay. No real filesystem scanning,
file operations, analysis, salience, grouping, or layout engine is implemented.

## Dependencies on Void Linux

Provide these packages when building the development container image:

- `base-devel`: C++20 compiler, linker, and build tools
- `cmake`: project configuration and build
- `SDL2-devel`: window creation, X11 input, and OpenGL context creation
- `MesaLib-devel` and `libglvnd-devel`: desktop OpenGL headers and libraries

Running also requires access to an X11 server (`DISPLAY`, its socket and any
required authorization) and a working OpenGL 3.3 driver. The application requires
SDL's native `x11` backend; it does not fall back to Wayland or a headless backend.
No packages are installed by the build.

Dear ImGui is compiled directly from the existing Git submodule, including the
standard SDL2 and OpenGL3 backends. Initialize it after cloning:

```sh
git submodule update --init third_party/imgui
```

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/spatial-navigator
```

Camera/navigation tests do not create a window or require an OpenGL context.
They cover orbit/zoom limits, transition interpolation and interruption,
reset, projection, and mock directory focus requests.

With an accessible X11 display, an optional finite smoke run renders 90 frames,
animates focus, orbits, zooms, resizes the window, resets the view, checks for
visible scene pixels and OpenGL errors, and quits through SDL's event queue:

```sh
./build/spatial-navigator --smoke-test
# Optional PPM capture of the final frame, for visual inspection:
./build/spatial-navigator --smoke-test /tmp/spatial-milestone1.ppm
```

The smoke test complements manual inspection; it cannot establish camera feel
or readability. In the implementation session the Debug build and logic tests
passed, but launching with `DISPLAY=:0` failed with `Initialize SDL2: x11 not
available`. Rendering and interaction still require visual validation on an
accessible X11 display. The Release build and its logic tests also passed.
An optional AddressSanitizer/UndefinedBehaviorSanitizer test build could not
link because the container lacks `libasan_preinit.o` and `libasan`.

## Controls

- Right mouse drag outside the overlay: bounded yaw/pitch orbit
- Mouse wheel outside the overlay: bounded zoom
- Tab: animate focus to the next mock directory
- Overlay directory buttons: animate focus to that directory
- R or Reset view: animate back to the overview
- Animate navigation checkbox: enable/disable subsequent transitions
- Escape or window close: quit

Orbit input is ignored while a camera transition is active and resumes normally
after arrival. Zoom interrupts camera animation at its current state. Transitions
last 0.4 seconds with smoothstep easing. Roll is fixed; there is no free flight.

## Structure

- `app/`: SDL/OpenGL lifetime, event/render loop, fixed mock display fixtures
- `graphics/`: plain scene geometry, cached line buffers, shaders, camera math
  and explicit camera transition state
- `navigation/`: selected mock directory and semantic focus requests
- `ui/`: conventional ImGui overlay

The filesystem, analysis, and computed layout layers are deferred to their
specified milestones. See `DESIGN.md` for product behavior and milestone scope.
