#!/usr/bin/env python3
"""Record the real X11 feature tour. Never installs tools or downloads anything."""
import argparse
import ctypes
import ctypes.util
from contextlib import contextmanager, ExitStack
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time
import tempfile

REPO = Path(__file__).resolve().parents[1]
PACKAGES = {"ffmpeg": "ffmpeg", "ffprobe": "ffmpeg", "urxvt": "urxvt",
            "Xvfb": "xorg-server-xvfb"}


def x11_available(display):
    library = ctypes.util.find_library("X11")
    if not library:
        return False
    x11 = ctypes.CDLL(library)
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    connection = x11.XOpenDisplay(display.encode())
    if not connection:
        return False
    x11.XCloseDisplay(connection)
    return True


def preflight(binary, display, virtual=False):
    problems = []
    for program in ["ffmpeg", "ffprobe", "urxvt"] + (["Xvfb"] if virtual else []):
        if not shutil.which(program):
            problems.append(f"Missing {program}; expected Void package: {PACKAGES[program]}.")
    if not binary.is_file() or not os.access(binary, os.X_OK):
        problems.append(f"Build the application first: {binary}")
    if not virtual and (not display or not x11_available(display)):
        problems.append(f"Cannot connect to X11 display {display or '(unset)'}; provide DISPLAY, its socket and authorization.")
    if not problems:
        encoders = subprocess.run(["ffmpeg", "-hide_banner", "-encoders"], capture_output=True, text=True, check=True)
        if "libx264" not in encoders.stdout:
            problems.append("FFmpeg needs its libx264 encoder for the MP4 recording.")
    return problems


@contextmanager
def desktop_environment(output, display):
    # Isolate demo configuration and the terminal helper from user settings.
    with tempfile.TemporaryDirectory(prefix="spatial-demo-xdg-") as temporary:
        yield _desktop_environment(output, display, Path(temporary))


def _desktop_environment(output, display, private):
    env = os.environ.copy()
    env.update(DISPLAY=display, SPATIAL_DEMO_OUTPUT=str(output), XDG_CURRENT_DESKTOP="X-Generic",
               XDG_DATA_HOME=str(private / "data"), XDG_CONFIG_HOME=str(private / "config"),
               XDG_CACHE_HOME=str(private / "cache"))
    applications = Path(env["XDG_DATA_HOME"]) / "applications"
    applications.mkdir(parents=True)
    Path(env["XDG_CONFIG_HOME"]).mkdir()
    Path(env["XDG_CACHE_HOME"]).mkdir()
    # Keep the self-closing terminal helper private to this recording.
    interpreter = private / "python3"
    interpreter.symlink_to(sys.executable)
    helper = private / "demo_desktop.py"
    shutil.copyfile(REPO / "scripts/demo_desktop.py", helper)
    # The browser's command parser accepts quoted arguments, without a shell.
    def command_quote(value):
        return '"' + str(value).replace("\\", "\\\\").replace('"', '\\"') + '"'
    env["FSN_TERMINAL"] = f"{command_quote(interpreter)} {command_quote(helper)} launch-terminal"
    return env


def stop(process, group=False):
    if process is None or process.poll() is not None:
        return
    try:
        if group:
            os.killpg(process.pid, signal.SIGTERM)
        else:
            process.terminate()
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        try:
            if group:
                os.killpg(process.pid, signal.SIGKILL)
            else:
                process.kill()
        except ProcessLookupError:
            pass
        process.wait(timeout=5)


def record(args):
    binary = args.binary.resolve()
    display = args.xvfb or os.environ.get("DISPLAY", "")
    problems = preflight(binary, display, bool(args.xvfb))
    if problems:
        raise RuntimeError("\n".join(problems) + "\nNo packages were installed and no recording was started.")
    if args.check:
        print("Recording prerequisites are available; no recording or output directory was created.")
        return
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    virtual = app = encoder = None
    desktop_stack = ExitStack()
    try:
        if args.xvfb:
            if not re.fullmatch(r":[1-9][0-9]{0,3}", display):
                raise RuntimeError("Use a free local display such as :99 for --xvfb")
            number = display[1:]
            if Path(f"/tmp/.X11-unix/X{number}").exists() or Path(f"/tmp/.X{number}-lock").exists():
                raise RuntimeError(f"Refusing an existing X server on {display}")
            with (output / "xvfb.log").open("w") as log:
                virtual = subprocess.Popen(["Xvfb", display, "-screen", "0", (args.size or "1920x1080") + "x24", "-nolisten", "tcp"], stdout=log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 15
            while not x11_available(display):
                if virtual.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError(f"Xvfb could not start; see {output / 'xvfb.log'}")
                time.sleep(0.1)
        env = desktop_stack.enter_context(desktop_environment(output, display))
        application_command = [str(binary), "--demo", str(output), "--demo-wait"]
        if args.size:
            application_command.extend(["--demo-size", args.size])
        with (output / "application.log").open("w") as log:
            app = subprocess.Popen(application_command,
                                   env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        deadline = time.monotonic() + 50
        ready = output / "ready.txt"
        while not ready.exists():
            if app.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError(f"Application did not become ready; see {output / 'application.log'}")
            time.sleep(0.05)
        width, height, x, y = map(int, ready.read_text().split())
        if width < 720 or height < 720:
            raise RuntimeError("Use a recording window of at least 720x720; 1000x1000 is recommended for a square video")
        if args.size and f"{width}x{height}" != args.size:
            raise RuntimeError("Window manager changed the requested recording size; float the demo window in i3 (see docs/RECORDING.md)")
        partial = output / "tour.partial.mp4"
        command = ["ffmpeg", "-hide_banner", "-n", "-f", "x11grab", "-draw_mouse", "0",
                   "-framerate", "30", "-video_size", f"{width}x{height}", "-i", f"{display}+{x},{y}",
                   "-an", "-vf", "pad=ceil(iw/2)*2:ceil(ih/2)*2", "-c:v", "libx264", "-preset", "veryfast",
                   "-crf", "18", "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(partial)]
        with (output / "ffmpeg.log").open("w") as log:
            encoder = subprocess.Popen(command, env=env, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=log, text=True)
        time.sleep(1)
        if encoder.poll() is not None:
            raise RuntimeError(f"FFmpeg stopped before capture began; see {output / 'ffmpeg.log'}")
        (output / "start").write_text("Capture running.\n")
        deadline = time.monotonic() + args.timeout
        while app.poll() is None:
            if encoder.poll() is not None:
                raise RuntimeError("FFmpeg stopped during the tour; see ffmpeg.log")
            if time.monotonic() > deadline:
                raise RuntimeError("Feature tour exceeded the recording timeout")
            time.sleep(0.1)
        encoder.communicate("q\n", timeout=45)
        if app.returncode != 0 or not (output / "completed.txt").exists():
            raise RuntimeError("The feature tour did not complete; see application.log")
        if encoder.returncode != 0:
            raise RuntimeError("FFmpeg could not finalize the video; see ffmpeg.log")
        for name, target in [("terminal-opened.txt", output / "demo-root/deep-tree")]:
            marker = output / name
            if not marker.exists() or Path(marker.read_text()) != target:
                raise RuntimeError(f"Desktop demonstration was not verified: {name}")
        result = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
                                 "stream=codec_name,width,height:format=duration", "-of", "json", str(partial)],
                                capture_output=True, text=True, check=True)
        metadata = json.loads(result.stdout)
        if not metadata.get("streams") or float(metadata.get("format", {}).get("duration", 0)) <= 0:
            raise RuntimeError("The encoded video has no valid video stream")
        (output / "video-info.json").write_text(json.dumps(metadata, indent=2) + "\n")
        partial.rename(output / "tour.mp4")
        print(f"Video: {output / 'tour.mp4'}\nNarration: {output / 'FEATURE_TOUR.txt'}\nTimeline: {output / 'tour.tsv'}")
    finally:
        stop(app, group=True)
        stop(encoder)
        stop(virtual)
        desktop_stack.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=REPO / "build/fv")
    parser.add_argument("--output", type=Path, default=Path("demo-recording-" + datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")))
    parser.add_argument("--check", action="store_true", help="Check dependencies and X11 access without recording")
    parser.add_argument("--xvfb", metavar=":99", help="Use a new isolated X11 display instead of the current desktop")
    parser.add_argument("--size", help="Recording window size, e.g. 1000x1000; also sizes Xvfb (default: full existing display, or 1920x1080 on Xvfb)")
    parser.add_argument("--timeout", type=float, default=600)
    args = parser.parse_args()
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("Use a positive finite timeout")
    if args.size and (not re.fullmatch(r"[1-9][0-9]{2,4}x[1-9][0-9]{2,4}", args.size) or
                      any(value < 720 or value > 16384 for value in map(int, args.size.split("x")))):
        parser.error("Use WIDTHxHEIGHT with each dimension between 720 and 16384 pixels, e.g. 1000x1000")
    if args.xvfb and not re.fullmatch(r":[1-9][0-9]{0,3}", args.xvfb):
        parser.error("Use a free local display such as :99 for --xvfb")
    record(args)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.SubprocessError, KeyboardInterrupt) as error:
        print(f"Recording stopped: {error}", file=sys.stderr)
        sys.exit(1)
