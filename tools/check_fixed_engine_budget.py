"""Keep the future MMC3 fixed-bank engine within its 16 KiB window."""

from pathlib import Path
import re
import sys

FIXED_CAPACITY = 0x3FDE  # $C000-$FFDD; BOOT and vectors occupy the remainder.
MIN_FREE_BYTES = 2048


def segment_size(map_text: str, name: str) -> int:
    match = re.search(
        rf"^{name}\s+[0-9A-F]{{6}}\s+[0-9A-F]{{6}}\s+([0-9A-F]{{6}})",
        map_text,
        re.MULTILINE,
    )
    if not match:
        raise ValueError(f"could not locate {name}")
    return int(match.group(1), 16)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_fixed_engine_budget.py GAME_RUNTIME.MAP")
        return 2
    text = Path(sys.argv[1]).read_text(encoding="ascii")
    try:
        used = segment_size(text, "FIXED_ENGINE")
        used += segment_size(text, "FIXED_ENGINE_RODATA")
    except ValueError as error:
        print(f"fixed-engine budget: {error}")
        return 1
    free = FIXED_CAPACITY - used
    print(f"Fixed engine: {used}/{FIXED_CAPACITY} bytes used; {free} bytes free")
    if free < MIN_FREE_BYTES:
        print(f"fixed-engine budget: need at least {MIN_FREE_BYTES} bytes free")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
