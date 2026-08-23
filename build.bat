@echo off
setlocal

set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%

python tools\gen_title_messages.py
if errorlevel 1 (
    echo.
    echo Title-message generation FAILED.
    exit /b 1
)

if not exist build mkdir build
pushd build

cl65 -t nes -Oirs -C ..\cfg\nes_mmc3_core.cfg -m game_core.map -o ..\game_core.nes ..\src\startup.s ..\src\music.s ..\src\chr.s ..\src\house.s ..\src\font.s ..\src\npc.s ..\src\box_border.s ..\src\item.s ..\src\enemy.s ..\src\sword.s ..\src\critter.s ..\src\portal.s ..\src\furniture.s ..\src\title_portrait.s ..\src\neon_avenue.s ..\src\player_avatar.s ..\src\taxi.s ..\src\bar_interior.s ..\src\title_tongue.s ..\src\traffic_bones.s ..\src\token.s ..\src\title_messages.s ..\src\v2_title_main.c ..\src\v2_runtime.c ..\src\v2_vram_queue.c ..\src\v2_game_state.c ..\src\v2_casino.c ..\src\rng.c ..\src\scene_casino_data.c ..\src\font.c
if errorlevel 1 (
    popd
    echo.
    echo Build FAILED.
    exit /b 1
)
popd

python tools\pack_mmc3.py game_core.nes game.nes
if errorlevel 1 (
    echo.
    echo MMC3 packing FAILED.
    exit /b 1
)

echo.
echo Build OK: game.nes
endlocal
