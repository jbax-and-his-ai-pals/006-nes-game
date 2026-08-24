@echo off
setlocal

call "%~dp0build.bat"
if errorlevel 1 exit /b 1

python tools\pack_mmc3.py game_runtime.nes build\fixed_layout.nes build\fixed_engine.nes
if errorlevel 1 (
    echo.
    echo Fixed-layout packing FAILED.
    exit /b 1
)

echo.
echo Fixed-layout validation image OK: build\fixed_layout.nes
endlocal
