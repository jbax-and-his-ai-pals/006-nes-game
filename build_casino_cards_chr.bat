@echo off
setlocal
set CC65_HOME=C:\tools\cc65
set PATH=%CC65_HOME%\bin;%PATH%
if not exist build mkdir build

ca65 -o build\casino_cards_chr0.o src\chr.s
if errorlevel 1 goto :failed
ca65 -o build\casino_cards_house.o src\house.s
if errorlevel 1 goto :failed
ca65 -o build\casino_cards_font.o src\font.s
if errorlevel 1 goto :failed
ca65 -o build\casino_cards_border.o src\box_border.s
if errorlevel 1 goto :failed
ca65 -o build\casino_cards_suits.o src\casino_cards.s
if errorlevel 1 goto :failed
ld65 -C cfg\nes_mmc3_chr_bank.cfg -o build\casino_cards_chr.bin build\casino_cards_chr0.o build\casino_cards_house.o build\casino_cards_font.o build\casino_cards_border.o build\casino_cards_suits.o
if errorlevel 1 goto :failed

echo.
echo Casino cards CHR bank OK: build\casino_cards_chr.bin
endlocal
exit /b 0

:failed
echo.
echo Casino cards CHR bank build FAILED.
endlocal
exit /b 1
