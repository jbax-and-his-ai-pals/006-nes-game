@echo off
setlocal

REM Build the Municipal Gym's CHR page: the shared common bank with a real
REM 2x2 backboard/hoop, a wood-plank court tile, and courtside equipment
REM (mat, weights, coach torso/legs) patched in at indices 60-70 (see
REM tools\gen_gym_chr.py). The gym maps CHR bank 8 (mmc3_use_gym_chr), so
REM this bank is byte-identical to common everywhere except those tiles.
REM Requires build\fixed_engine.nes (built by build_scene_gym.bat).

set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%
if not exist build mkdir build

python tools\gen_gym_chr.py build\fixed_engine.nes build\gym_chr.bin
if errorlevel 1 goto :failed

echo.
echo Gym CHR bank OK: build\gym_chr.bin
endlocal
exit /b 0

:failed
echo.
echo Gym CHR bank build FAILED.
endlocal
exit /b 1
