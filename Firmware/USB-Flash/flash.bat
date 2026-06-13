@echo off
setlocal
pushd "%~dp0"

echo ----------------------------------------
echo Please Connect the Robo-Trickler via USB
echo ----------------------------------------
pause

echo ----------------------------------------
echo Erase Flash
echo ----------------------------------------
"%~dp0esptool.exe" --chip esp32 erase_flash
if errorlevel 1 goto :error

echo ----------------------------------------
echo Flash Robo-Trickler Firmware and LittleFS
echo ----------------------------------------
"%~dp0esptool.exe" --chip esp32 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 8MB ^
  0x1000 "%~dp0RoboTricklerUI.ino.bootloader.bin" ^
  0x8000 "%~dp0RoboTricklerUI.ino.partitions.bin" ^
  0xe000 "%~dp0boot_app0.bin" ^
  0x10000 "%~dp0RoboTricklerUI.ino.bin" ^
  0x670000 "%~dp0littlefs.bin"
if errorlevel 1 goto :error

echo ----------------------------------------
echo Done!
echo ----------------------------------------
popd
pause
exit /b 0

:error
echo ----------------------------------------
echo Flash failed!
echo ----------------------------------------
popd
pause
exit /b 1



