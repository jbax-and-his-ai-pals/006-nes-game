@echo off
setlocal

REM Build All-Nite Mart's CHR page: the shared common bank with corrugated
REM siding and a roofline cap patched in at indices 60-61 (see
REM tools\gen_mart_chr.py) -- the same tiles the Gym's own page uses,
REM reused under the Mart's own gray/green palette. The Mart maps CHR bank
REM 9 (mmc3_use_mart_chr), so this bank is byte-identical to common
REM everywhere except those two tiles.
REM Requires build\fixed_engine.nes (built by build_scene_mart.bat).

set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%
if not exist build mkdir build

python tools\gen_mart_chr.py build\fixed_engine.nes build\mart_chr.bin
if errorlevel 1 goto :failed

echo.
echo Mart CHR bank OK: build\mart_chr.bin
endlocal
exit /b 0

:failed
echo.
echo Mart CHR bank build FAILED.
endlocal
exit /b 1
