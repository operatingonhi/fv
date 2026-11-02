#!/usr/bin/env python3
"""Self-closing terminal helper used only by record_demo.py."""
import os
from pathlib import Path
import subprocess
import sys
import time


def main():
    output = Path(os.environ["SPATIAL_DEMO_OUTPUT"]).resolve()
    root = output / "demo-root"
    mode = sys.argv[1]
    if mode not in ("launch-terminal", "show-terminal"):
        raise RuntimeError(f"Unknown helper mode: {mode}")
    target = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else Path.cwd().resolve()
    if target != root / "deep-tree":
        raise RuntimeError(f"Refusing an unexpected demo target: {target}")
    if mode == "launch-terminal":
        width, height, x, y = map(int, (output / "ready.txt").read_text().split())
        # Keep a compact terminal inside the captured client area, even when
        # that area is a square centered within a widescreen desktop.
        geometry = f"60x16+{x + max(12, (width - 560) // 2)}+{y + max(12, (height - 340) // 2)}"
        command = ["urxvt", "-geometry", geometry,
                   "-title", "fv: Terminal working directory", "-e", sys.executable,
                   str(Path(__file__).resolve()), "show-terminal", str(target)]
        return subprocess.call(command, cwd=target)
    if Path.cwd().resolve() != target:
        raise RuntimeError("Terminal did not inherit the selected working directory")
    print("A shell running in the selected directory:\n", flush=True)
    subprocess.run(["/bin/sh", "-c", "pwd; printf '\\nDirectory entries:\\n'; ls"], check=True)
    (output / "terminal-opened.txt").write_text(str(target))
    print("\nThis demonstration window closes automatically.", flush=True)
    time.sleep(7)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print(f"Demo desktop helper: {error}", file=sys.stderr)
        sys.exit(1)
