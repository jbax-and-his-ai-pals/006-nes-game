@echo off
setlocal

REM Build the Galaxy Lanes & Arcade CHR page: the shared common bank with the
REM alley road line re-patched at tile 2 plus the new neon-marquee tiles at
REM indices 60-65 (see tools\gen_arcade_chr.py). The arcade maps CHR bank 7
REM (mmc3_use_arcade_chr), so this bank is byte-identical to common everywhere
REM except the road line and the neon facade art -- letting the scene drop the
REM bar's flat sign plate without touching any CHR the rest of the game uses.
REM Requires build\fixed_engine.nes (built by build_scene_arcade.bat).

set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%
if not exist build mkdir build

python tools\gen_arcade_chr.py build\fixed_engine.nes build\arcade_chr.bin
if errorlevel 1 goto :failed

echo.
echo Arcade CHR bank OK: build\arcade_chr.bin
endlocal
exit /b 0

:failed
echo.
echo Arcade CHR bank build FAILED.
endlocal
exit /b 1
