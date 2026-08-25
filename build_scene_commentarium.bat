@echo off
setlocal

set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%

if not exist build mkdir build

call "%~dp0build_fixed_engine.bat"
if errorlevel 1 exit /b 1
python tools\gen_dumb_comments.py
if errorlevel 1 exit /b 1
python tools\gen_fixed_api.py build\fixed_engine.map build\fixed_api.s
if errorlevel 1 exit /b 1

cl65 -c -t nes -Oirs -o build\v2_scene_commentarium.o src\v2_scene_commentarium.c
if errorlevel 1 goto :failed
ca65 -o build\scene_commentarium_entry.o src\scenes\scene_commentarium_entry.s
if errorlevel 1 goto :failed
ca65 -o build\fixed_api.o build\fixed_api.s
if errorlevel 1 goto :failed
ld65 -C cfg\nes_mmc3_scene.cfg -m build\scene_commentarium_real.map -o build\scene_commentarium_real.bin build\scene_commentarium_entry.o build\v2_scene_commentarium.o build\fixed_api.o %CC65_HOME%\lib\nes.lib
if errorlevel 1 goto :failed
python tools\check_scene_module.py build\scene_commentarium_real.bin build\scene_commentarium_real.map
if errorlevel 1 goto :failed

echo Real Commentarium scene build OK: build\scene_commentarium_real.bin
endlocal
exit /b 0

:failed
echo Real Commentarium scene build FAILED.
endlocal
exit /b 1
