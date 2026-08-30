@echo off
setlocal

python tools\gen_title_messages.py
if errorlevel 1 exit /b 1

call "%~dp0build_scene_park.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_park_chr.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_scene_avenue.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_scene_bar.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_scene_mart.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_mart_chr.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_sewer_chr.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_scene_sewer.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_scene_casino.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_scene_zoo.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_scene_commentarium.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_music_songs.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_scene_arcade.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_arcade_chr.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_scene_sidestreet.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_sidestreet_chr.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_scene_gym.bat"
if errorlevel 1 exit /b 1
call "%~dp0build_gym_chr.bat"
if errorlevel 1 exit /b 1

python tools\pack_mmc3.py build\fixed_engine.nes game.nes
if errorlevel 1 exit /b 1

echo.
echo Build OK: game.nes
endlocal
