"""Pack the fixed core and manifest-owned content into an MMC3 image."""

import json
from pathlib import Path
import sys

CORE_PRG_SIZE = 32 * 1024
CORE_CHR_SIZE = 8 * 1024
PRG_SIZE = 512 * 1024
CHR_SIZE = 256 * 1024
BANK_SIZE = 8 * 1024
MANIFEST_PATH = Path("assets/banks/manifest.json")


def read_fixed_engine(path: Path) -> bytes:
    """Return the 16 KiB PRG payload from a fixed-engine validation ROM."""
    image = path.read_bytes()
    expected = 16 + 2 * BANK_SIZE + CORE_CHR_SIZE
    if len(image) != expected or image[:4] != b"NES\x1a":
        raise ValueError(f"{path}: not a 16 KiB fixed-engine NES image")
    return image[16 : 16 + 2 * BANK_SIZE]


def load_manifest(image_prg: bytearray, image_chr: bytearray) -> tuple[list[str], set[int]]:
    """Place named raw content and return its size report plus assigned CHR banks."""
    if not MANIFEST_PATH.exists():
        return (["No bank manifest found; PRG banks 2-61 and CHR banks 2-31 remain available."], set())

    manifest = json.loads(MANIFEST_PATH.read_text(encoding="ascii"))
    report = []
    used_prg: set[int] = set()
    used_chr: set[int] = set()
    for asset in manifest.get("assets", []):
        kind = asset["kind"]
        if kind not in ("prg", "chr"):
            raise ValueError(f"{asset.get('source', '<unknown>')}: unknown asset kind {kind!r}")
        banks = asset.get("banks")
        if banks is None:
            banks = [asset["bank"]]
        if not isinstance(banks, list) or not banks:
            raise ValueError(f"{asset.get('source', '<unknown>')}: banks must be a non-empty list")
        if any(not isinstance(bank, int) for bank in banks):
            raise ValueError(f"{asset.get('source', '<unknown>')}: banks must contain integers")
        if banks != list(range(banks[0], banks[0] + len(banks))):
            raise ValueError(f"{asset.get('source', '<unknown>')}: banks must be contiguous")
        source_name = asset["source"]
        source = Path(source_name)
        offset = asset.get("offset", 0)
        if not isinstance(offset, int):
            raise ValueError(f"{source}: offset must be an integer")
        target, bank_count, used = (image_prg, 64, used_prg) if kind == "prg" else (image_chr, 32, used_chr)
        if source_name == "@common_chr":
            if kind != "chr":
                raise ValueError("@common_chr may only be used for CHR assets")
            data = bytes(image_chr[:CORE_CHR_SIZE])
        else:
            data = source.read_bytes()
        for bank in banks:
            if not 0 <= bank < bank_count:
                raise ValueError(f"{source}: {kind} bank {bank} is out of range")
            if kind == "prg" and bank in (0, 1, 62, 63):
                raise ValueError(f"{source}: PRG bank {bank} belongs to the mapped runtime")
            if kind == "chr" and bank in (0, 1):
                raise ValueError(f"{source}: CHR bank {bank} belongs to common/title graphics")
            if bank in used:
                raise ValueError(f"{source}: {kind} bank {bank} is already assigned")
        capacity = BANK_SIZE * len(banks)
        if offset < 0 or offset + len(data) > capacity:
            raise ValueError(f"{source}: does not fit {kind} banks {banks}")
        first = banks[0] * BANK_SIZE + offset
        for index, byte in enumerate(data):
            target[first + index] = byte
        used.update(banks)
        label = f"{banks[0]:02d}" if len(banks) == 1 else f"{banks[0]:02d}-{banks[-1]:02d}"
        report.append(f"{kind.upper()} {label}: {len(data):4d}/{capacity} bytes  {source}")
    return (report or ["Manifest contains no banked assets yet."], used_chr)


def main() -> int:
    if len(sys.argv) not in (3, 4):
        print("usage: pack_mmc3.py CORE.NES OUTPUT.NES [FIXED_ENGINE.NES]")
        return 2

    core = Path(sys.argv[1]).read_bytes()
    fixed_core = len(core) == 16 + 2 * BANK_SIZE + CORE_CHR_SIZE
    runtime_core = len(core) == 16 + CORE_PRG_SIZE + CORE_CHR_SIZE
    if not fixed_core and not runtime_core:
        print("unexpected cc65 runtime or fixed-engine ROM size")
        return 1
    if core[:4] != b"NES\x1a":
        print("core is not an iNES ROM")
        return 1

    prg_size = 2 * BANK_SIZE if fixed_core else CORE_PRG_SIZE
    prg = core[16 : 16 + prg_size]
    chr_data = core[16 + prg_size :]
    image_prg = bytearray([0xFF]) * PRG_SIZE
    # MMC3 begins in this same visible arrangement after the fixed boot stub
    # selects banks 0 and 1 at $8000/$A000; C000/E000 are fixed to 62/63.
    if runtime_core:
        image_prg[0 * BANK_SIZE : 1 * BANK_SIZE] = prg[0 * BANK_SIZE : 1 * BANK_SIZE]
        image_prg[1 * BANK_SIZE : 2 * BANK_SIZE] = prg[1 * BANK_SIZE : 2 * BANK_SIZE]
    if len(sys.argv) == 4:
        try:
            fixed_prg = read_fixed_engine(Path(sys.argv[3]))
        except OSError as exc:
            print(f"fixed-engine error: {exc}")
            return 1
        except ValueError as exc:
            print(f"fixed-engine error: {exc}")
            return 1
        image_prg[62 * BANK_SIZE : 64 * BANK_SIZE] = fixed_prg
    elif fixed_core:
        image_prg[62 * BANK_SIZE : 64 * BANK_SIZE] = prg
    else:
        image_prg[62 * BANK_SIZE : 63 * BANK_SIZE] = prg[2 * BANK_SIZE : 3 * BANK_SIZE]
        image_prg[63 * BANK_SIZE : 64 * BANK_SIZE] = prg[3 * BANK_SIZE : 4 * BANK_SIZE]

    image_chr = bytearray(CHR_SIZE)
    image_chr[:CORE_CHR_SIZE] = chr_data
    # Bank 1 is the first independently selectable title bank. It starts as
    # a stable clone so every current title tile ID remains valid; later
    # title-only art can replace this bank without touching gameplay CHR.
    image_chr[CORE_CHR_SIZE : 2 * CORE_CHR_SIZE] = chr_data
    try:
        report, used_chr = load_manifest(image_prg, image_chr)
    except (KeyError, TypeError, ValueError, json.JSONDecodeError, OSError) as exc:
        print(f"bank manifest error: {exc}")
        return 1
    header = bytearray(b"NES\x1a")
    header.extend((32, 32, 0x43, 0x00))  # mapper 4, horizontal mirroring, battery RAM
    header.extend(b"\0" * 8)
    Path(sys.argv[2]).write_bytes(header + image_prg + image_chr)
    if len(sys.argv) == 4:
        report.insert(0, "PRG 62-63: 16384/16384 bytes  fixed-engine validation image")
    report.insert(0, "CHR 01: 8192/8192 bytes  title graphics base clone")
    report.insert(0, "CHR 00: 8192/8192 bytes  common graphics base bank")
    report.append("Unlisted PRG banks remain available for banked scene modules.")
    remaining_chr = [bank for bank in range(2, 32) if bank not in used_chr]
    report.append("Available CHR banks: " + ", ".join(f"{bank:02d}" for bank in remaining_chr))
    Path("build/bank_report.txt").write_text("\n".join(report) + "\n", encoding="ascii")
    if len(header) + len(image_prg) + len(image_chr) != 16 + PRG_SIZE + CHR_SIZE:
        print("internal MMC3 size check failed")
        return 1
    print(f"Packed MMC3: {PRG_SIZE // 1024} KiB PRG, {CHR_SIZE // 1024} KiB CHR")
    print("Wrote build/bank_report.txt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
