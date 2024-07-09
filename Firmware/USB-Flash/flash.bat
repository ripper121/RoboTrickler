@echo off
echo ----------------------------------------
echo Please Connect the Robo-Trickler via USB
echo ----------------------------------------
pause
echo ----------------------------------------
echo Erase Flash
echo ----------------------------------------
esptool.exe --chip esp32 erase_flash
echo ----------------------------------------
echo Flash Robo-Trickler Firmware
echo ----------------------------------------
esptool.exe --chip esp32 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dout --flash_freq 80m --flash_size 4MB 0x1000 ./RoboTricklerUI.ino.bootloader.bin 0x8000 ./RoboTricklerUI.ino.partitions.bin 0xe000 ./boot_app0.bin 0x10000 ./RoboTricklerUI.ino.bin
echo ----------------------------------------
echo Done!
echo ----------------------------------------
pause




