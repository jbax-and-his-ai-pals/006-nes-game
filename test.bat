@echo off
setlocal enabledelayedexpansion

set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%
REM See build_fixed_engine.bat's comment: keeps cl65's scratch objects out
REM of the project root if TEMP/TMP would otherwise resolve badly.
if not exist tests\out mkdir tests\out
set TEMP=%~dp0tests\out
set TMP=%~dp0tests\out

set FAILED=0

call :run_test test_room "src\room.c tests\test_room.c"
call :run_test test_movement "src\movement.c src\room.c tests\test_movement.c"
call :run_test test_read_pad "tests\test_read_pad.c"
call :run_test test_font "src\font.c tests\test_font.c"
call :run_test test_rng "src\rng.c tests\test_rng.c"
call :run_test test_v2_casino "src\rng.c src\v2_casino.c tests\test_v2_casino.c"
call :run_test test_v2_vram_queue "src\v2_vram_queue.c tests\test_v2_vram_queue.c"
call :run_test test_quest "src\rng.c src\quest.c tests\test_quest.c"
call :run_test test_world "src\room.c src\world.c tests\test_world.c"

echo --- test_pack_mmc3 ---
python tests\test_pack_mmc3.py
if errorlevel 1 (
    echo   ^(exit code indicates failed checks^)
    set FAILED=1
)
echo.

echo.
if %FAILED%==1 (
    echo ==== SOME TESTS FAILED ====
    exit /b 1
) else (
    echo ==== ALL TESTS PASSED ====
)
endlocal
goto :eof

:run_test
set NAME=%~1
set SOURCES=%~2
echo --- %NAME% ---
cl65 -t sim6502 -o tests\out\%NAME%.bin %SOURCES%
if errorlevel 1 (
    echo   BUILD FAILED
    set FAILED=1
    goto :eof
)
sim65 tests\out\%NAME%.bin
if errorlevel 1 (
    echo   ^(exit code indicates failed checks^)
    set FAILED=1
)
echo.
goto :eof
