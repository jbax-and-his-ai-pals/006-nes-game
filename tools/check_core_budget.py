"""Fail early when the currently mapped runtime window loses all headroom.

The MMC3 image is large, but code linked by cfg/nes_mmc3_runtime.cfg still shares
one 32 KiB CPU-visible window. This check makes that temporary constraint
explicit until scenes are moved to banks 2-61.
"""

from pathlib import Path
import re
import sys

RUNTIME_START = 0x8000
BOOT_START = 0xFFDE
MIN_FREE_BYTES = 48


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_core_budget.py GAME_RUNTIME.MAP")
        return 2
    text = Path(sys.argv[1]).read_text(encoding="ascii")
    match = re.search(r"__DATA_LOAD__\s+([0-9A-F]{6})", text)
    if not match:
        print("mapped-runtime budget: could not locate __DATA_LOAD__")
        return 1
    used_end = int(match.group(1), 16)
    free = BOOT_START - used_end
    capacity = BOOT_START - RUNTIME_START
    print(f"Mapped runtime: {capacity - free}/{capacity} bytes used; {free} bytes free")
    if free < MIN_FREE_BYTES:
        print(f"mapped-runtime budget: need at least {MIN_FREE_BYTES} bytes free")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
