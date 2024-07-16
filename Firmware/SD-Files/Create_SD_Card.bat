@echo off
echo !!! Please backup all your powder profiles before proceeding !!!
echo This tool will format and delete all data on the SD card.
pause
set /p driveLetter=Please enter your SD card drive letter:
format %driveLetter%: /FS:FAT32 /V:R-Trickler /X /A:1024 /Q
xcopy /s/e/h/y/v .\SD-Files %driveLetter%:\
echo -----
echo Operation completed!
echo -----
pause
