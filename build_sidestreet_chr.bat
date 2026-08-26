@echo off
setlocal

REM Build the Pawn Shop/Laundromat side street's CHR page: the shared
REM common bank with a thin transparent-backed vertical line patched in at
REM tile 2 (see tools\gen_sidestreet_chr.py). The side street maps CHR bank
REM 6 (mmc3_use_sidestreet_chr), which the manifest would otherwise point at
REM the common clone where tile 2 is the Piss Corner regulars' facial
REM overlay -- so this bank is byte-identical to common everywhere except
REM that one tile.
REM Requires build\fixed_engine.nes (built by build_scene_sidestreet.bat).

set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%
if not exist build mkdir build

python tools\gen_sidestreet_chr.py build\fixed_engine.nes build\sidestreet_chr.bin
if errorlevel 1 goto :failed

echo.
echo Side Street CHR bank OK: build\sidestreet_chr.bin
endlocal
exit /b 0

:failed
echo.
echo Side Street CHR bank build FAILED.
endlocal
exit /b 1
