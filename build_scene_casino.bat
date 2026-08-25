@echo off
setlocal
set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%
REM See build_fixed_engine.bat's comment: keeps cl65's scratch objects out
REM of the project root if TEMP/TMP would otherwise resolve badly.
if not exist build\tmp mkdir build\tmp
set TEMP=%~dp0build\tmp
set TMP=%~dp0build\tmp
if not exist build mkdir build
call "%~dp0build_fixed_engine.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_casino_cards_chr.bat"
if errorlevel 1 exit /b 1
python tools\gen_fixed_api.py build\fixed_engine.map build\fixed_api.s
if errorlevel 1 exit /b 1
cl65 -c -t nes -Oirs -o build\v2_scene_casino.o src\v2_scene_casino.c
if errorlevel 1 goto :failed
cl65 -c -t nes -Oirs -o build\scene_casino_data.o src\scene_casino_data.c
if errorlevel 1 goto :failed
REM v2_casino.c (blackjack/poker/slot game logic) lives in this bank, not
REM the fixed engine -- flush support pushed the fixed engine's shared
REM budget over its limit, and this logic is only ever used here anyway.
REM See v2_game_state.c's comment on the one exception (casino_reset()'s
REM logic stays inlined there, since the fixed engine can't reach into an
REM unmapped scene bank the way scene code reaches into the fixed engine).
REM rng.c itself stays only in the fixed engine (still linked there) --
REM v2_casino.c reaches rng_seed()/rng_next() through the same fixed API
REM trampoline as everything else here, not a second compiled copy.
cl65 -c -t nes -Oirs -o build\v2_casino.o src\v2_casino.c
if errorlevel 1 goto :failed
ca65 -o build\scene_casino_entry.o src\scenes\scene_casino_entry.s
if errorlevel 1 goto :failed
ca65 -o build\fixed_api.o build\fixed_api.s
if errorlevel 1 goto :failed
ld65 -C cfg\nes_mmc3_scene.cfg -m build\scene_casino_real.map -o build\scene_casino_real.bin build\scene_casino_entry.o build\v2_scene_casino.o build\scene_casino_data.o build\v2_casino.o build\fixed_api.o %CC65_HOME%\lib\nes.lib
if errorlevel 1 goto :failed
python tools\check_scene_module.py build\scene_casino_real.bin build\scene_casino_real.map
if errorlevel 1 goto :failed
echo Real Casino scene build OK: build\scene_casino_real.bin
endlocal
exit /b 0
:failed
echo Real Casino scene build FAILED.
endlocal
exit /b 1
