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


def load_manifest(image_prg: bytearray, image_chr: bytearray) -> list[str]:
    """Place named raw content into non-runtime banks and return a size report."""
    if not MANIFEST_PATH.exists():
        return ["No bank manifest found; PRG banks 2-61 and CHR banks 2-31 remain available."]

    manifest = json.loads(MANIFEST_PATH.read_text(encoding="ascii"))
    report = []
    used_prg: set[int] = set()
    used_chr: set[int] = set()
    for asset in manifest.get("assets", []):
        kind = asset["kind"]
        bank = asset["bank"]
        source = Path(asset["source"])
        offset = asset.get("offset", 0)
        data = source.read_bytes()
        target, bank_count, used = (image_prg, 64, used_prg) if kind == "prg" else (image_chr, 32, used_chr)
        if not 0 <= bank < bank_count:
            raise ValueError(f"{source}: {kind} bank {bank} is out of range")
        if kind == "prg" and bank in (0, 1, 62, 63):
            raise ValueError(f"{source}: PRG bank {bank} belongs to the fixed runtime")
        if kind == "chr" and bank in (0, 1):
            raise ValueError(f"{source}: CHR bank {bank} belongs to common/title graphics")
        if bank in used:
            raise ValueError(f"{source}: one asset per {kind} bank is required until bank modules are linked")
        if offset < 0 or offset + len(data) > BANK_SIZE:
            raise ValueError(f"{source}: does not fit {kind} bank {bank}")
        target[bank * BANK_SIZE + offset : bank * BANK_SIZE + offset + len(data)] = data
        used.add(bank)
        report.append(f"{kind.upper()} {bank:02d}: {len(data):4d}/{BANK_SIZE} bytes  {source}")
    return report or ["Manifest contains no banked assets yet."]


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: pack_mmc3.py CORE.NES OUTPUT.NES")
        return 2

    core = Path(sys.argv[1]).read_bytes()
    if len(core) != 16 + CORE_PRG_SIZE + CORE_CHR_SIZE:
        print("unexpected cc65 core ROM size")
        return 1
    if core[:4] != b"NES\x1a":
        print("core is not an iNES ROM")
        return 1

    prg = core[16 : 16 + CORE_PRG_SIZE]
    chr_data = core[16 + CORE_PRG_SIZE :]
    image_prg = bytearray([0xFF]) * PRG_SIZE
    # MMC3 begins in this same visible arrangement after the fixed boot stub
    # selects banks 0 and 1 at $8000/$A000; C000/E000 are fixed to 30/31.
    image_prg[0 * BANK_SIZE : 1 * BANK_SIZE] = prg[0 * BANK_SIZE : 1 * BANK_SIZE]
    image_prg[1 * BANK_SIZE : 2 * BANK_SIZE] = prg[1 * BANK_SIZE : 2 * BANK_SIZE]
    image_prg[62 * BANK_SIZE : 63 * BANK_SIZE] = prg[2 * BANK_SIZE : 3 * BANK_SIZE]
    image_prg[63 * BANK_SIZE : 64 * BANK_SIZE] = prg[3 * BANK_SIZE : 4 * BANK_SIZE]

    image_chr = bytearray(CHR_SIZE)
    image_chr[:CORE_CHR_SIZE] = chr_data
    # Bank 1 is the first independently selectable title bank. It starts as
    # a compatible copy so every current title tile ID remains valid; later
    # title-only art can replace this bank without touching gameplay CHR.
    image_chr[CORE_CHR_SIZE : 2 * CORE_CHR_SIZE] = chr_data
    try:
        report = load_manifest(image_prg, image_chr)
    except (KeyError, TypeError, ValueError, json.JSONDecodeError, OSError) as exc:
        print(f"bank manifest error: {exc}")
        return 1
    header = bytearray(b"NES\x1a")
    header.extend((32, 32, 0x43, 0x00))  # mapper 4, horizontal mirroring, battery RAM
    header.extend(b"\0" * 8)
    Path(sys.argv[2]).write_bytes(header + image_prg + image_chr)
    report.insert(0, "CHR 01: 8192/8192 bytes  title-compatible base bank")
    report.insert(0, "CHR 00: 8192/8192 bytes  common graphics base bank")
    Path("build/bank_report.txt").write_text("\n".join(report) + "\n", encoding="ascii")
    if len(header) + len(image_prg) + len(image_chr) != 16 + PRG_SIZE + CHR_SIZE:
        print("internal MMC3 size check failed")
        return 1
    print(f"Packed MMC3: {PRG_SIZE // 1024} KiB PRG, {CHR_SIZE // 1024} KiB CHR")
    print("Wrote build/bank_report.txt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
