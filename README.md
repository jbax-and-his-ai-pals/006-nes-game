# Jerb's Game

An NES adventure-comedy rebuild in C for a 512 KiB PRG / 256 KiB CHR MMC3
cartridge. The active game currently includes the animated title screen,
Neon Avenue and All-Nite Mart exteriors, the sewer route, Jimbles's Bar and
Piss Corner, All-Nite Mart's interior, and Wrecked 'Em Casino.

The previous action-RPG prototype is archived for reference. It is not linked
into the active ROM.

## Build and Run

The project uses cc65, expected at `C:\tools\cc65`.

```text
build.bat
run.bat
test.bat
```

`build.bat` produces `game.nes`; `run.bat` builds and opens it in Mesen.
`test.bat` runs the host-side regression suite for hardware-independent
modules. Emulator testing remains required for PPU, OAM, mapper, and timing
behavior.

## Source of Truth

- [CURRENT_ARCHITECTURE.md](CURRENT_ARCHITECTURE.md): active runtime,
  capacity status, constraints, and next engineering milestones.
- [REBUILD_ROADMAP.md](REBUILD_ROADMAP.md): playable rebuild scope and
  content order.
- [MAPPER.md](MAPPER.md): MMC3 cartridge layout and bank rules.
- [archive/](archive/): preserved material from the retired prototype.

## Editing Content

- `assets/title_messages.txt` controls title portrait speech. The build
  regenerates `src/title_messages.s` from it.
- Most current scene behavior, dialogue, collision, and sprite staging live
  in `src/v2_title_main.c`. The next engineering milestone is splitting that
  file into reusable scene modules before the town grows further.
- `assets/title_portrait_reference.png` and its NES-ready derivatives are
  processed by `tools/gen_title_portrait.py`.
- New scene assets should receive an explicit PRG/CHR allocation in
  `assets/banks/manifest.json` once the fixed-bank scene runtime exists.
