# Banked Dispatcher Contract

## Fixed Engine

PRG banks 62 and 63 will hold `FIXED_ENGINE` and
`FIXED_ENGINE_RODATA` at `$C000-$FFDD`, with the existing MMC3 boot/vector
tail at `$FFDE-$FFFF`. This region owns reset, NMI, music, UI services, game
state, and the high-level dispatcher.

`mmc3_select_prg_pair(first_bank)` is a fixed-engine fastcall routine. It
maps `first_bank` at `$8000-$9FFF` and `first_bank + 1` at `$A000-$BFFF`.
Only fixed code may call it.

## Scene Call

1. The dispatcher records the requested scene and its entry argument in RAM.
2. The dispatcher calls `mmc3_select_prg_pair()` from fixed PRG.
3. A fixed assembly trampoline performs `JSR $8000`, where each scene image
   provides a small entry stub at its first byte.
4. Banked scene code may call fixed APIs and return normally to the trampoline.
5. The scene entry's `A` result returns through the trampoline, so fixed code
   decides the next scene and changes banks again.

The trampoline, not a banked scene, owns the return address across a PRG bank
change. A scene must never call `mmc3_select_prg_pair()` itself.

`mmc3_call_mapped_scene()` is now implemented as that fixed trampoline. The
Park bank pair is linked as a real `$8000` image with an entry stub that calls
`run_park()` and returns to fixed code. `tools/gen_fixed_api.py` turns the
fixed-engine linker map into absolute API aliases for that scene link.

## Current Stage

`build_fixed_engine.bat` now links the real fixed engine at `$C000-$FFFF` and
enforces its fixed-window budget. It uses link-only scene stubs, so it is a
layout validation artifact rather than the playable ROM.
`build_scene_park.bat` compiles Park independently against the generated fixed
API map. `build_fixed_layout.bat` packs both products into their physical MMC3
locations: fixed engine at banks 62-63 and Park at banks 03-04. The remaining
runtime work is to route the production dispatcher through this real scene
rather than the transitional mapped runtime. The fixed-layout validation image
already has a narrow manual smoke route: after title start, its Avenue stub
maps banks 05-06 and calls the real Avenue through the fixed trampoline. It is
not the production game flow.

Multi-entry scenes use `banked_scene_arg` in fixed RAM. For example, Mart
entry value `2` selects its interior loop, while values `0` and `1` select its
exterior with the appropriate arrival spawn. This keeps all same-module entry
points inside one mapped pair without a second mapper transition.
