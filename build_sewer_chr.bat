@echo off
setlocal
set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%
if not exist build mkdir build

ca65 -o build\sewer_chr0.o src\chr.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_house.o src\house.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_font.o src\font.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_frame.o src\sewer_frame.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_item.o src\item.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_enemy.o src\enemy.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_sword.o src\sword.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_critter.o src\critter.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_portal.o src\portal.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_furniture.o src\furniture.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_portrait.o src\title_portrait.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_avenue.o src\neon_avenue.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_avatar.o src\player_avatar.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_taxi.o src\taxi.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_bar.o src\bar_interior.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_tongue.o src\title_tongue.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_bones.o src\traffic_bones.s
if errorlevel 1 goto :failed
ca65 -o build\sewer_key.o src\sewer_key.s
if errorlevel 1 goto :failed
ld65 -C cfg\nes_mmc3_chr_bank.cfg -o build\sewer_chr.bin build\sewer_chr0.o build\sewer_house.o build\sewer_font.o build\sewer_frame.o build\sewer_item.o build\sewer_enemy.o build\sewer_sword.o build\sewer_critter.o build\sewer_portal.o build\sewer_furniture.o build\sewer_portrait.o build\sewer_avenue.o build\sewer_avatar.o build\sewer_taxi.o build\sewer_bar.o build\sewer_tongue.o build\sewer_bones.o build\sewer_key.o
if errorlevel 1 goto :failed

echo.
echo Sewer CHR bank OK: build\sewer_chr.bin
endlocal
exit /b 0

:failed
echo.
echo Sewer CHR bank build FAILED.
endlocal
exit /b 1
