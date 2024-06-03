@echo off
set /p driveLetter=Please enter your SD-Card Drive Letter:
format %driveLetter%: /FS:FAT32 /V:R-Trickler /X /A:1024 /Q
xcopy /s/e/h/y/v .\SD-Files %driveLetter%:\
echo -----
echo Done!
echo -----
pause