@echo off
echo !!! Bitte sichern Sie alle Ihre Pulverprofile bevor Sie hier weiter machen !!!
echo Dieses Tool wird alle Daten auf der SD-Karte formatieren und loeschen.
pause
set /p driveLetter=Bitte geben Sie den Laufwerksbuchstaben Ihrer SD-Karte ein:
format %driveLetter%: /FS:FAT32 /V:R-Trickler /X /A:1024 /Q
xcopy /s/e/h/y/v .\SD-Files %driveLetter%:\
echo -----
echo Vorgang abgeschlossen!
echo -----
pause
