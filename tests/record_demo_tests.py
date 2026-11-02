#!/usr/bin/env python3
"""Recorder unit checks; no GUI, recording, package installation or downloads."""
import importlib.util
import os
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("record_demo", REPO / "scripts/record_demo.py")
recorder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recorder)


class RecorderTests(unittest.TestCase):
    def test_missing_tools_are_reported_without_subprocesses(self):
        with patch.object(recorder.shutil, "which", return_value=None), \
             patch.object(recorder, "x11_available", return_value=False), \
             patch.object(recorder.subprocess, "run") as run:
            problems = recorder.preflight(REPO / "build/fv", ":99", True)
        self.assertTrue(any("ffmpeg" in problem for problem in problems))
        self.assertTrue(any("xorg-server-xvfb" in problem for problem in problems))
        run.assert_not_called()

    def test_private_desktop_environment(self):
        before = os.environ.copy()
        with tempfile.TemporaryDirectory(prefix="spatial recording test ") as temporary:
            output = Path(temporary)
            with recorder.desktop_environment(output, ":77") as env:
                self.assertEqual(env["DISPLAY"], ":77")
                self.assertEqual(env["XDG_CURRENT_DESKTOP"], "X-Generic")
                self.assertTrue(env["FSN_TERMINAL"].endswith("launch-terminal"))
                self.assertFalse((Path(env["XDG_CONFIG_HOME"]) / "mimeapps.list").exists())
                self.assertFalse(list(Path(env["XDG_DATA_HOME"]).rglob("*.desktop")))
        self.assertEqual(os.environ.copy(), before)

    def test_terminal_helper_uses_recorder_terminal(self):
        helper_spec = importlib.util.spec_from_file_location("demo_desktop", REPO / "scripts/demo_desktop.py")
        helper = importlib.util.module_from_spec(helper_spec)
        helper_spec.loader.exec_module(helper)
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "demo-root/deep-tree"
            target.mkdir(parents=True)
            (Path(temporary) / "ready.txt").write_text("1000 1000 460 40")
            with patch.dict(os.environ, {"SPATIAL_DEMO_OUTPUT": temporary}), \
                 patch.object(helper.sys, "argv", ["demo_desktop.py", "launch-terminal", str(target)]), \
                 patch.object(helper.subprocess, "call", return_value=0) as launch:
                self.assertEqual(helper.main(), 0)
            command = launch.call_args.args[0]
            self.assertEqual(command[0], "urxvt")
            self.assertIn("show-terminal", command)
            self.assertEqual(command[command.index("-geometry") + 1], "60x16+680+370")
            self.assertEqual(launch.call_args.kwargs["cwd"], target)
            with patch.dict(os.environ, {"SPATIAL_DEMO_OUTPUT": temporary}), \
                 patch.object(helper.sys, "argv", ["demo_desktop.py", "launch-file", str(target)]):
                with self.assertRaises(RuntimeError):
                    helper.main()

    def test_completed_recording_does_not_require_document_launch_marker(self):
        self.check_recording(None, "1280 720 0 0")

    def test_square_recording_captures_window_at_its_desktop_offset(self):
        self.check_recording("1000x1000", "1000 1000 460 40")

    def test_window_manager_size_override_stops_before_capture(self):
        self.check_recording("1000x1000", "1280 720 0 0", expect_mismatch=True)

    def check_recording(self, size, geometry, expect_mismatch=False):
        class Process:
            def __init__(self, finished):
                self.returncode = 0 if finished else None
            def poll(self):
                return self.returncode
            def communicate(self, *_args, **_kwargs):
                self.returncode = 0
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "recording"
            binary = REPO / "build/fv"
            args = type("Args", (), {"binary": binary, "xvfb": None, "check": False,
                                      "output": output, "timeout": 10, "size": size})()
            commands = []
            def spawn(command, **_kwargs):
                commands.append(command)
                if command[0] == str(binary.resolve()):
                    if size:
                        self.assertEqual(command[-2:], ["--demo-size", size])
                    else:
                        self.assertNotIn("--demo-size", command)
                    (output / "ready.txt").write_text(geometry)
                    (output / "completed.txt").write_text("Complete")
                    (output / "terminal-opened.txt").write_text(str(output / "demo-root/deep-tree"))
                    return Process(True)
                Path(command[-1]).write_bytes(b"test video placeholder")
                return Process(False)
            probe = subprocess.CompletedProcess([], 0, stdout=json.dumps({"streams": [{"codec_name": "h264"}], "format": {"duration": "2"}}))
            with patch.dict(os.environ, {"DISPLAY": ":77"}), \
                 patch.object(recorder, "preflight", return_value=[]), \
                 patch.object(recorder.subprocess, "Popen", side_effect=spawn), \
                 patch.object(recorder.subprocess, "run", return_value=probe), \
                 patch.object(recorder.time, "sleep"):
                if expect_mismatch:
                    with self.assertRaisesRegex(RuntimeError, "Window manager changed"):
                        recorder.record(args)
                    self.assertEqual(len(commands), 1)
                    return
                recorder.record(args)
            self.assertTrue((output / "tour.mp4").exists())
            self.assertFalse((output / "file-opened.txt").exists())
            width, height, x, y = geometry.split()
            capture = commands[1]
            self.assertEqual(capture[capture.index("-video_size") + 1], f"{width}x{height}")
            self.assertEqual(capture[capture.index("-i") + 1], f":77+{x},{y}")

    def test_output_refuses_reuse(self):
        with tempfile.TemporaryDirectory() as temporary:
            existing = Path(temporary) / "recording"
            existing.mkdir()
            marker = existing / "keep.txt"
            marker.write_text("keep")
            args = type("Args", (), {"binary": REPO / "build/fv", "xvfb": None,
                                      "check": False, "output": existing})()
            with patch.object(recorder, "preflight", return_value=[]):
                with self.assertRaises(FileExistsError):
                    recorder.record(args)
            self.assertEqual(marker.read_text(), "keep")

    def test_check_creates_no_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "unused"
            args = type("Args", (), {"binary": REPO / "build/fv", "xvfb": None,
                                      "check": True, "output": output})()
            with patch.object(recorder, "preflight", return_value=[]):
                recorder.record(args)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
