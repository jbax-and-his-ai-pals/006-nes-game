@echo off
setlocal

REM Build the Moonlight Park CHR page: the shared common bank with the
REM sewer-key graphic at tile 255 (see tools\gen_park_chr.py). The park maps
REM CHR bank 2 (mmc3_use_park_chr), which the manifest would otherwise point
REM at the common clone where tile 255 is the jukebox token -- so this bank is
REM byte-identical to common everywhere except the key tile.
REM Requires build\fixed_engine.nes (built by build_scene_park.bat).

set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%
if not exist build mkdir build

python tools\gen_park_chr.py build\fixed_engine.nes build\park_chr.bin
if errorlevel 1 goto :failed

echo.
echo Park CHR bank OK: build\park_chr.bin
endlocal
exit /b 0

:failed
echo.
echo Park CHR bank build FAILED.
endlocal
exit /b 1
