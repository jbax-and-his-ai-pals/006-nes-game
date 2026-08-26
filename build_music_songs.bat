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
python tools\gen_fixed_api.py build\fixed_engine.map build\fixed_api.s
if errorlevel 1 exit /b 1

REM The music bank is a banked data module (all songs as fixed-stride blocks),
REM not a scene, so it is not checked by tools/check_scene_module.py. It lands
REM at $8000 via the scene cfg; music_select_song reads song 0's block there.
ca65 -t nes -o build\music_songs.o src\music_songs.s
if errorlevel 1 goto :failed
ld65 -C cfg\nes_mmc3_scene.cfg -m build\music_songs.map -o build\music_songs_real.bin build\music_songs.o %CC65_HOME%\lib\nes.lib
if errorlevel 1 goto :failed

echo.
echo Music bank build OK: build\music_songs_real.bin
endlocal
exit /b 0

:failed
echo.
echo Music bank build FAILED.
endlocal
exit /b 1
