@echo off
set /p comPort=Please enter the COM-Port (COM1):
esptool.exe --chip esp32 --port %comPort% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dout --flash_freq 80m --flash_size 4MB 0x1000 ./build/RoboTricklerUI.ino.bootloader.bin 0x8000 ./build/RoboTricklerUI.ino.partitions.bin 0xe000 ./boot_app0.bin 0x10000 ./build/RoboTricklerUI.ino.bin
echo -----
echo Done!
echo -----
pause




