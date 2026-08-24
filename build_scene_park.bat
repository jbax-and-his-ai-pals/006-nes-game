@echo off
setlocal

set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%

if not exist build mkdir build

call "%~dp0build_fixed_engine.bat"
if errorlevel 1 exit /b 1

python tools\gen_fixed_api.py build\fixed_engine.map build\fixed_api.s
if errorlevel 1 exit /b 1

cl65 -c -t nes -Oirs -o build\v2_scene_park.o src\v2_scene_park.c
if errorlevel 1 goto :failed
ca65 -o build\scene_park_entry.o src\scenes\scene_park_entry.s
if errorlevel 1 goto :failed
ca65 -o build\fixed_api.o build\fixed_api.s
if errorlevel 1 goto :failed
ld65 -C cfg\nes_mmc3_scene.cfg -m build\scene_park_real.map -o build\scene_park_real.bin build\scene_park_entry.o build\v2_scene_park.o build\fixed_api.o %CC65_HOME%\lib\nes.lib
if errorlevel 1 goto :failed
python tools\check_scene_module.py build\scene_park_real.bin build\scene_park_real.map
if errorlevel 1 goto :failed

echo.
echo Real Park scene build OK: build\scene_park_real.bin
endlocal
exit /b 0

:failed
echo.
echo Real Park scene build FAILED.
endlocal
exit /b 1
