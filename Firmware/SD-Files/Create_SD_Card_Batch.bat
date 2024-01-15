@echo off
set /p driveLetter=Please enter your SD-Card Drive Letter:
:loop
format %driveLetter%: /FS:FAT32 /Q /V:R-Trickler /X
xcopy /s/e/h/y/v .\SD-Files %driveLetter%:\
echo -----
echo Done!
echo -----
pause
goto loop