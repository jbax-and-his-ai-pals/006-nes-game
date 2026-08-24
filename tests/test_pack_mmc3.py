"""Host checks for manifest-owned MMC3 bank placement."""

import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import pack_mmc3  # noqa: E402


def check(condition: bool, label: str) -> None:
    if not condition:
        raise AssertionError(label)
    print(f"PASS {label}")


def images() -> tuple[bytearray, bytearray]:
    return (bytearray([0xFF]) * pack_mmc3.PRG_SIZE,
            bytearray(pack_mmc3.CHR_SIZE))


def load(temp: Path, assets: list[dict], prg: bytearray, chr_data: bytearray):
    manifest = temp / "manifest.json"
    manifest.write_text(json.dumps({"assets": assets}), encoding="ascii")
    previous = pack_mmc3.MANIFEST_PATH
    pack_mmc3.MANIFEST_PATH = manifest
    try:
        return pack_mmc3.load_manifest(prg, chr_data)
    finally:
        pack_mmc3.MANIFEST_PATH = previous


def expect_error(temp: Path, assets: list[dict], label: str) -> None:
    prg, chr_data = images()
    try:
        load(temp, assets, prg, chr_data)
    except ValueError:
        print(f"PASS {label}")
        return
    raise AssertionError(label)


def main() -> int:
    # Keep generated fixtures in the workspace. The Windows app sandbox can
    # deny Python writes to the system temp directory during test runs.
    temp = ROOT / "tests" / "out" / "pack_mmc3"
    temp.mkdir(parents=True, exist_ok=True)
    small = temp / "small.bin"
    small.write_bytes(bytes((0x12, 0x34, 0x56)))
    pair = temp / "pair.bin"
    pair.write_bytes(bytes(range(256)) * 64)

    prg, chr_data = images()
    report, used_chr = load(temp, [
        {"kind": "prg", "bank": 2, "source": str(small)},
        {"kind": "prg", "banks": [3, 4], "source": str(pair)},
    ], prg, chr_data)
    check(prg[2 * pack_mmc3.BANK_SIZE:2 * pack_mmc3.BANK_SIZE + 3] == small.read_bytes(),
          "single PRG asset is placed in its assigned bank")
    check(prg[3 * pack_mmc3.BANK_SIZE:5 * pack_mmc3.BANK_SIZE] == pair.read_bytes(),
          "paired PRG asset crosses its bank boundary intact")
    check(len(report) == 2 and not used_chr, "PRG-only manifest reports no assigned CHR banks")

    prg, chr_data = images()
    chr_data[:pack_mmc3.CORE_CHR_SIZE] = bytes([0xA5]) * pack_mmc3.CORE_CHR_SIZE
    _, used_chr = load(temp, [{"kind": "chr", "bank": 2, "source": "@common_chr"}], prg, chr_data)
    check(chr_data[2 * pack_mmc3.BANK_SIZE:3 * pack_mmc3.BANK_SIZE] ==
          bytes([0xA5]) * pack_mmc3.CORE_CHR_SIZE,
          "common CHR clone fills the selected scene page")
    check(used_chr == {2}, "CHR assignment is returned for reporting")

    fixed = temp / "fixed_engine.nes"
    fixed.write_bytes(b"NES\x1a" + bytes(12) + bytes([0x5A]) *
                      (2 * pack_mmc3.BANK_SIZE + pack_mmc3.CORE_CHR_SIZE))
    check(pack_mmc3.read_fixed_engine(fixed) == bytes([0x5A]) * (2 * pack_mmc3.BANK_SIZE),
          "fixed-engine image supplies exactly two PRG banks")

    expect_error(temp, [{"kind": "prg", "bank": 62, "source": str(small)}],
                 "fixed runtime PRG banks are protected")
    expect_error(temp, [
        {"kind": "prg", "bank": 2, "source": str(small)},
        {"kind": "prg", "bank": 2, "source": str(small)},
    ], "overlapping manifest banks are rejected")
    expect_error(temp, [{"kind": "prg", "banks": [3, 5], "source": str(small)}],
                 "non-contiguous paired banks are rejected")
    expect_error(temp, [{"kind": "bogus", "bank": 2, "source": str(small)}],
                 "unknown asset kinds are rejected")
    expect_error(temp, [{"kind": "prg", "bank": 2, "offset": "bad", "source": str(small)}],
                 "non-integer offsets are rejected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
