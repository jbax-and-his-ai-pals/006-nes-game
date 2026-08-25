@echo off
setlocal

set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%
REM cl65 drops per-file scratch objects wherever TEMP/TMP resolve to; if
REM either is unset or unwritable in the calling environment it silently
REM falls back to the current directory, littering the project root with
REM PID-named *.o files. Pointing both at a dedicated, gitignored folder
REM keeps that fallback contained no matter what shell invokes this script.
if not exist build\tmp mkdir build\tmp
set TEMP=%~dp0build\tmp
set TMP=%~dp0build\tmp

if not exist build mkdir build

cl65 -t nes -Oirs -C cfg\nes_mmc3_fixed.cfg -m build\fixed_engine.map -o build\fixed_engine.nes src\startup.s src\music.s src\chr.s src\house.s src\font.s src\npc.s src\box_border.s src\item.s src\enemy.s src\sword.s src\critter.s src\portal.s src\furniture.s src\title_portrait.s src\neon_avenue.s src\player_avatar.s src\taxi.s src\bar_interior.s src\title_tongue.s src\traffic_bones.s src\token.s src\title_messages.s src\v2_title_main.c src\v2_runtime.c src\v2_scene_ui.c src\v2_dialogue.c src\v2_input.c src\v2_inventory.c src\v2_player_actor.c src\v2_regulars.c src\v2_traffic.c src\v2_vram_queue.c src\v2_game_state.c src\rng.c src\font.c src\fixed_scene_stubs.c
if errorlevel 1 (
    echo.
    echo Fixed-engine layout build FAILED.
    exit /b 1
)

python tools\check_fixed_engine_budget.py build\fixed_engine.map
if errorlevel 1 (
    echo.
    echo Fixed-engine budget check FAILED.
    exit /b 1
)

echo.
echo Fixed-engine layout build OK: build\fixed_engine.nes
endlocal
