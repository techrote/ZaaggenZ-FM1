#!/usr/bin/env python3
"""Pinned Fune preset/song transfer utility; explicit push overwrites one chosen slot.

  python tools/fune_slots.py list
  python tools/fune_slots.py --port COM4 pull preset 1 backup.fune
  python tools/fune_slots.py check build/fune-presets/01-elastic-bloom.fune
  python tools/fune_slots.py --port COM4 push build/fune-presets/01-elastic-bloom.fune 1
"""
from pathlib import Path
import runpy
import sys
from fune_transport import configure_pinned_console

SCRIPT = Path(__file__).resolve().parents[1] / "third_party/fune/platforms/fm1hw/fm1_slots.py"
if __name__ == "__main__":
    configure_pinned_console()
    sys.argv[0] = str(SCRIPT)
    runpy.run_path(str(SCRIPT), run_name="__main__")
