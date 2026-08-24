@echo off
setlocal

set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%

if not exist build mkdir build

call "%~dp0build_fixed_engine.bat"
if errorlevel 1 exit /b 1
python tools\gen_fixed_api.py build\fixed_engine.map build\fixed_api.s
if errorlevel 1 exit /b 1

cl65 -c -t nes -Oirs -o build\v2_scene_bar.o src\v2_scene_bar.c
if errorlevel 1 goto :failed
ca65 -o build\scene_bar_entry.o src\scenes\scene_bar_entry.s
if errorlevel 1 goto :failed
ca65 -o build\fixed_api.o build\fixed_api.s
if errorlevel 1 goto :failed
ld65 -C cfg\nes_mmc3_scene.cfg -m build\scene_bar_real.map -o build\scene_bar_real.bin build\scene_bar_entry.o build\v2_scene_bar.o build\fixed_api.o %CC65_HOME%\lib\nes.lib
if errorlevel 1 goto :failed
python tools\check_scene_module.py build\scene_bar_real.bin build\scene_bar_real.map
if errorlevel 1 goto :failed

echo.
echo Real Bar scene build OK: build\scene_bar_real.bin
endlocal
exit /b 0

:failed
echo.
echo Real Bar scene build FAILED.
endlocal
exit /b 1
