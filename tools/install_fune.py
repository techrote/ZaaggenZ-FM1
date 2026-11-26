#!/usr/bin/env python3
"""Use Fune's pinned plugin manager with a bounded large-image serial transport.

Device writes require an explicit install, off, on or erase command. `check` only
reads the local file; `list` only reads the device's plugin listing.

Examples (Fune firmware must already be installed):
  python tools/install_fune.py check build/fm1/zaaggenz.kpl
  python tools/install_fune.py list
  python tools/install_fune.py --port COM4 install build/fm1/zaaggenz.kpl
  python tools/install_fune.py off 1

Requires pyserial for device access. Keep the FM-1 stopped during installation.
"""
from pathlib import Path
import runpy
import sys
from fune_transport import configure_pinned_console

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "third_party/fune/platforms/fm1hw/fm1_plugin.py"
if __name__ == "__main__":
    configure_pinned_console()
    sys.argv[0] = str(SCRIPT)
    runpy.run_path(str(SCRIPT), run_name="__main__")
