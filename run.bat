@echo off
setlocal

call "%~dp0build.bat"
if errorlevel 1 exit /b 1

set MESEN=%LOCALAPPDATA%\Microsoft\WinGet\Packages\SourMesen.Mesen2_Microsoft.Winget.Source_8wekyb3d8bbwe\Mesen.exe

if not exist "%MESEN%" (
    echo Mesen not found at %MESEN%
    echo Install it with: winget install --id SourMesen.Mesen2 -e
    exit /b 1
)

start "" "%MESEN%" "%~dp0game.nes"
endlocal
