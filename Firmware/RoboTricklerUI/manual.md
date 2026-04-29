# Erste Schritte

Stand: Firmware 2.10.

1. Verbinde die Steuereinheit mit der Waage via RS-232 Stecker.
2. Verbinde die Steuereinheit mit dem Trickler.
3. Stelle sicher, dass sich die SD-Karte in der Steuereinheit befindet.
4. Schalte die Waage an und nulle diese (TARE).
5. Verbinde die Steuereinheit mit dem Netzteil.


Im Display der Steuerung sollte nun das gleiche Gewicht wie auf der Waage angezeigt werden. 

Falls dies nicht der Fall ist, überprüfe die [Einstellungen der Waage](https://github.com/ripper121/RoboTrickler/wiki/Anleitung#waagen) und die [Konfiguration](https://github.com/ripper121/RoboTrickler/wiki/Anleitung#konfiguration) ("protocol" & "baud").

Der Robo-Trickler sollte jetzt mit einem Standard (avg) Pulverprofil starten. Stelle das Zielgewicht im Display ein und drücke auf Start. Starte erst, wenn die Waage mit leerer Pulverpfanne genullt ist (TARE).

Wähle im Profil-Tab ein passendes Pulver aus. Falls das gewünschte Pulver nicht vorhanden ist, kann zum testen das "avg" Pulverprofil genommen werden. Das "avg" Pulverprofil ist ein Durchschnitt vieler Pulver und sollte mit jedem Pulver funktionieren, das aber recht langsam. Deswegen erstellt für jedes Pulver ein eigenes Profil!

**Für jedes Pulver MUSS ein [eigenes Profil angelegt](https://github.com/ripper121/RoboTrickler/wiki/Anleitung#erstellen) werden, um ein optimales Trickeln zu gewährleisten!**

**Jede Waage MUSS vor dem Gebrauch warm Laufen, je nach Modell bis zu 1h!
Ansonsten "driftet" der gewogene Wert und die Waage geht nicht auf 0 wenn man die Pulverpfanne leer auf die Waage stellt.**

![screen](https://github.com/ripper121/RoboTrickler/assets/11836272/988fc66b-6269-4b3c-8205-689d55ea32a8)

# Pulverprofil

## Erstellen

Video Anleitung:

[![youtube video](https://img.youtube.com/vi/Y3HOAB9iIfA/0.jpg)](https://www.youtube.com/watch?v=Y3HOAB9iIfA)


Zum Erstellen eines eigenen Pulverprofils wird das "calibrate" Profil benutzt, welches sich in der calibrate.txt befindet.

```json
{
   "1": {
      "weight": 0.000,
      "steps": 20000,
      "speed": 200,
      "alarmThreshold": 1,
      "measurements": 20
   }
}
```

Das "calibrate" Profil dreht den Trickler 100x (20000 Schritte / 200 = 100U) mit der Geschwindigkeit von 200U/min.

Um das "calibrate" Profil zu benutzen, wähle dieses im "Profile Tab" auf der Steuerung mit den Pfeiltasten aus.

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/a941bd0a-9c93-40d5-b2b6-a844727b5865)

1. Stelle das Gewicht, das getrickelt werden soll, auf >3 Gramm oder >40 Grain ein.
2. Drücke auf Start.
3. Lasse den Trickler mindestens 3x mit dem calibrate Profil laufen, wenn du den Trickler-Tank frisch mit Pulver aufgefüllt hast, damit das Tricklerrohr richtig gefüllt ist.
4. Notiere dir, wie viel der Trickler bei einem Durchgang (100 Umdrehungen) getrickelt hat.
5. Öffne die profileGenerator.html von der SD-Karte (oder im Webbrowser, wenn der Trickler mit Wifi verbunden ist).

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/67fcc33f-f18f-44a9-9f25-5ea08cc997fe)

6. Trage beim "Profile Generator" den notierten Wert bei "Weight of Calibration run (20000 Steps)" ein.
7. Wähle die Einheit, mit der du getrickelt hast (Unit Gramm / Grain).
8. Klicke auf "Generate Profile".
9. Es wird ein neues Pulverprofil erzeugt.
10. Trage einen Profilnamen ein, z.B. das Kürzel des Pulvers, bei "Vihtavuori n140" z.B. n140.
11. Klicke auf Download.
12. Kopiere das heruntergeladene Profil in das Hauptverzeichnis der SD-Karte.
13. Stecke die SD-Karte wieder in den Trickler und starte diesen neu (Strom ab und wieder dran).
14. Jetzt sollte euer neues Profil im "Profile Tab" auswählbar sein.

Du kannst natürlich auch das Pulverprofil komplett selbst erstellen. Der Generator erzeugt nur eine Basis, auf die man aufbauen kann.

Noch ein paar Hinweise:

* Wenn man zu wenig "steps" angibt (abhängig vom Motor im Trickler), kann es sein, dass sich der Trickler gar nicht bewegt.
* Die "speed" sollte zwischen 10 und 200 liegen, sonst kann es sein, dass sich der Trickler nicht bewegt.
* Niedrigere Trickel-Geschwindigkeiten ("speed") fördern meist eine höhere Pulvermenge.
* In einem Profil kann bei jedem Schritt eine andere Geschwindigkeit benutzt werden, z.B. am Anfang eine langsame Geschwindigkeit für viel Pulver und am Ende eine hohe Geschwindigkeit für nur ein Körnchen.
* Zu viele "measurements" und es kann sehr lange dauern, bis ein Schritt bearbeitet ist. Am Anfang reichen meist 2 Messungen, am Ende sollten es mindestens 5 sein.

## Gramm / Grain

Wenn du die Einheit der Waage umstellst, muss auch das Pulverprofil angepasst werden.

Hierzu musst du bei jedem Schritt im Profil das "weight" auf Grain oder Gramm umrechnen/umstellen.

Um ein Profil von Gramm auf Grain umzustellen, multipliziere die Gramm mit 15,432.

Du kannst auch für jedes Pulver jeweils für Gramm und Grain ein eigenes [Profil](https://github.com/ripper121/RoboTrickler/wiki/Anleitung#pulverprofil) anlegen, z.B. RS14Gramm.txt und RS14Grain.txt, und diese dann in der [config.txt](https://github.com/ripper121/RoboTrickler/wiki/Anleitung#konfiguration) bei "profile" umstellen.

Gramm Profil:

```json
{
   "1": {
      "weight": 0.001,
      "steps": 3600,
      "speed": 200,
      "measurements": 20
   }
}
```

Umgebaut auf Grain:

```json
{
   "1": {
      "weight": 0.015,
      "steps": 3600,
      "speed": 200,
      "measurements": 20
   }
}
```

## Aufbau

Beispiel N135 Pulverprofil mit 12 Schritten:

```json
{
    "1": {
        "weight": 46.297,
        "tolerance": 0,
        "alarmThreshold": 1,
        "stepsPerUnit": 155,
        "steps": 7165,
        "speed": 200,
        "measurements": 5,
        "reverse": false
    },
    "2": {
        "weight": 30.865,
        "steps": 4777,
        "speed": 200,
        "measurements": 5,
        "reverse": false
    },
    "3": {
        "weight": 15.432,
        "steps": 2388,
        "speed": 200,
        "measurements": 5,
        "reverse": false
    },
    "4": {
        "weight": 7.716,
        "steps": 1194,
        "speed": 200,
        "measurements": 5,
        "reverse": false
    },
    "5": {
        "weight": 3.858,
        "steps": 597,
        "speed": 200,
        "measurements": 5,
        "reverse": false
    },
    "6": {
        "weight": 1.929,
        "steps": 299,
        "speed": 200,
        "measurements": 5,
        "reverse": false
    },
    "7": {
        "weight": 0.965,
        "steps": 149,
        "speed": 200,
        "measurements": 5,
        "reverse": false
    },
    "8": {
        "weight": 0.482,
        "steps": 75,
        "speed": 200,
        "measurements": 5,
        "reverse": false
    },
    "9": {
        "weight": 0.241,
        "steps": 37,
        "speed": 200,
        "measurements": 5,
        "reverse": false
    },
    "10": {
        "weight": 0.121,
        "steps": 19,
        "speed": 200,
        "measurements": 10,
        "reverse": false
    },
    "11": {
        "weight": 0.06,
        "steps": 10,
        "speed": 200,
        "measurements": 15,
        "reverse": false
    },
    "12": {
        "weight": 0,
        "steps": 10,
        "speed": 200,
        "measurements": 15,
        "reverse": false
    }
}
```

* "ZAHL": Nummer des Schrittes, es sind 1 bis 16 Schritte möglich, diese müssen aufsteigend nummeriert sein.
* number: Welcher Trickler verwendet werden soll, 1 oder 2. Standard ist 1, wenn der Parameter weggelassen wird.


* weight: Das Gewicht, bei dem dieser Schritt ausgeführt wird. Entfernung zum Zielgewicht.
* steps: Schritte, die der Trickler ausführen soll (200 = 1 Umdrehung).
* speed: Geschwindigkeit, mit der sich der Trickler dreht.
* measurements: Anzahl gleicher Messergebnisse von der Waage, die kommen müssen bis zum nächsten Trickeln.
* tolerance: Die Abweichung, die zum Zielgewicht erlaubt ist. Nur in Schritt 1 definieren!
* alarmThreshold: Gibt einen Alarm aus, wenn der Zielwert um die verwendete Einheit überschritten wird. Nur in Schritt 1 definieren!
* reverse: true, Drehrichtung umkehren, Standard ist false.
* stepsPerUnit: Schritte, die es braucht, um 1 Gramm oder 1 Grain zu trickeln. Anhand dieses Wertes wird der erste Trickelschritt berechnet, um möglichst schnell zum Zielgewicht zu kommen. Wird stepsPerUnit weggelassen oder auf 0 gesetzt, wählt er den nächst passenden Schritt vom Profil aus. Nur in Schritt 1 definieren!

Hinweis: Die alten Profiloptionen "oscillate" und "acceleration" werden ab Firmware 2.10 nicht mehr verwendet. Alte Profile mit diesen Feldern werden weiterhin gelesen, die Werte haben aber keine Wirkung.


## Mehrere Trickler

Über den Parameter "number" im Pulverprofil wird festgelegt, welcher Trickler benutzt wird. Wird "number" nicht angegeben, benutzt er standardmäßig Trickler 1, um kompatibel mit alten Profilen zu bleiben.

Statt eines 2. Tricklers kann man hier auch ein Pulverfüllgerät mit einem Schrittmotor versehen und diesen im Profil genauso benutzen.

Pulverprofil-Beispiel für 2 Trickler, in den Schritten 1-5 benutzt er Trickler 2 und in den restlichen Trickler 1:

```json
{
   "1": {
      "number": 2,
      "weight": 1,
      "steps": 2000,
      "speed": 200,
      "tolerance": 0.000,
      "alarmThreshold": 0.100,
      "measurements": 2
   },
   "2": {
      "number": 2,
      "weight": 0.5,
      "steps": 1000,
      "speed": 200,
      "measurements": 2
   },
   "3": {
      "number": 2,
      "weight": 0.25,
      "steps": 500,
      "speed": 200,
      "measurements": 2
   },
   "4": {
      "number": 2,
      "weight": 0.125,
      "steps": 250,
      "speed": 200,
      "measurements": 2
   },
   "5": {
      "number": 2,
      "weight": 0.062,
      "steps": 124,
      "speed": 200,
      "measurements": 2
   },
   "6": {
      "number": 1,
      "weight": 0.031,
      "steps": 62,
      "speed": 200,
      "measurements": 2
   },
   "7": {
      "number": 1,
      "weight": 0.015,
      "steps": 30,
      "speed": 200,
      "measurements": 2
   },
   "8": {
      "number": 1,
      "weight": 0.007,
      "steps": 14,
      "speed": 200,
      "measurements": 5
   },
   "9": {
      "number": 1,
      "weight": 0.003,
      "steps": 6,
      "speed": 200,
      "measurements": 15
   },
   "10": {
      "number": 1,
      "weight": 0.001,
      "steps": 5,
      "speed": 200,
      "measurements": 20
   }
}
```

## Profil Ablaufplan

[Mermaidchart](https://www.mermaidchart.com/d/766e35d2-3566-4ae9-9ccd-6fdcd064747b)

<img width="789" height="1600" alt="image" src="https://github.com/user-attachments/assets/32d6d247-d452-4469-940e-2ce495263db8" />


# SD-Karte

Falls die SD-Karte defekt ist oder beim Bearbeiten Fehler aufgetreten sind, können die SD-Karten-Daten [hier](https://github.com/ripper121/RoboTrickler/releases) neu heruntergeladen werden. Die SD-Karte muss mit FAT32 formatiert sein!

Unter Windows kannst du auch die "Create_SD_Card.bat" nutzen. Gib den Laufwerksbuchstaben ein und die SD-Karte wird automatisch erstellt.

Video Anleitung:

[![youtube video](https://img.youtube.com/vi/QKmBHEd7rD4/0.jpg)](https://www.youtube.com/watch?v=QKmBHEd7rD4)

1. Stecke die Micro SD Karte in den PC

2. Download Etcher
   -> https://etcher.balena.io/#download-etcher

3. Download das neuste SD-Files.img
   -> https://robo-trickler.de

4. Start Etcher

5. Wähle das SD-Files.img aus

6. Flash

7. Fertig


## Konfiguration

```json
{
  "wifi": {
    "ssid": "MeinWlanName",
    "psk": "MeinWlanPassword",
    "IPStatic": "",
    "IPGateway": "",
    "IPSubnet": "",
    "IPDNS": ""
  },
  "scale": {
    "protocol": "GG",
    "customCode": "",
    "baud": 9600
  },
  "profile": "calibrate",
  "weight": 2.540,
  "beeper": "done",
  "fw_check": true
}
```

* wifi
  * ssid: Trage hier deinen WLAN-Namen ein. Nur 2.4GHz WLAN wird unterstützt!
  * psk: Trage hier dein WLAN-Passwort ein. Bei offenem WLAN einfach leer lassen.
  * IPStatic: Optional, statische IP-Adresse.
  * IPGateway: Wenn statische IP-Adresse verwendet wird, muss die Gateway-IP ausgefüllt werden.
  * IPSubnet: Wenn statische IP-Adresse verwendet wird, muss die Subnet-IP ausgefüllt werden.
  * IPDNS: Optional, DNS-Server-IP, z.B. 8.8.8.8.
* scale
  * protocol: Unterstützte Waagenprotokolle: G&G "GG", Sartorius (Denver) "SBI", Kern "KERN", Kern ABT "KERN-ABT", Steinberg "STE", A&D "AD" und "CUSTOM".
  * baud: Die Geschwindigkeit, mit der die Waage Daten per RS-232 überträgt, meistens 9600 baud.
  * customCode: Nur in Verbindung mit protocol:"CUSTOM", falls eine nicht unterstützte Waage verwendet wird, kann hier das Kommando eingetragen werden, mit dem Daten von der Waage angefordert werden. Dieses Kommando ist meist im Handbuch der Waage zu finden.
* profile: Der Name des Pulverprofils. Wenn der Dateiname "MeinPulver.txt" ist, muss hier "MeinPulver" eingetragen werden.
* weight: Das zuletzt eingestellte Gewicht zum Trickeln.
* beeper: Es gibt drei Modi: 1. Bei Abschluss des Trickelns piepen ("done"), 2. Beim Drücken auf den Touchscreen ("button"), 3. Beides ("both").
* fw_check: true prüft, ob eine neue Firmware verfügbar ist und zeigt dies im Display an. Standard-Einstellung ist true.

Nicht mehr verwendete Felder wie "log_measurements" oder "debug_log" können aus alten config.txt-Dateien entfernt werden. Firmware 2.10 liest diese Werte nicht ein.

Auf der SD-Karte befindet sich die configGenerator.html, welche das Erstellen einer Konfiguration erleichtert. Der configGenerator wird auch via Webserver bereitgestellt.

Wenn die Konfiguration fertig ist, kopiere den Text in deine config.txt oder gehe auf Download und kopiere die config.txt direkt auf die SD-Karte.

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/4f03a9cb-fd73-49ec-b0ca-dec5daa9c527)


## Firmware Update

Es gibt zwei Möglichkeiten, ein Firmware-Update durchzuführen:

1. Kopiere die update.bin auf die SD-Karte.
2. Führe das Update via Webbrowser -> "FW Update" durch.

Die neueste Firmware findest du [hier](https://github.com/ripper121/RoboTrickler/releases).

## Deserialization failed

Prüfe die config.txt und das verwendete Pulverprofil mit [dieser Seite](https://jsonformatter.curiousconcept.com). Die Seite zeigt Formatierungsfehler in den Dateien an und korrigiert sie ggf.

Wenn ein ausgewähltes Pulverprofil nicht gelesen werden kann, benennt die Firmware die Datei in ".cor.txt" um, wählt automatisch "calibrate" aus und startet neu.

# Wifi (WLAN)

Um den Wifi-Modus zu aktivieren, trage SSID (Wifi-Name) und PSK (Wifi-Passwort) in die "config.txt" ein.
!!! Nur 2.4GHz Wifi wird unterstützt !!!

```json
{
  "wifi": {
    "ssid": "Mein Wifi Name",
    "psk": "Mein Wifi Password",
    "IPStatic": "",
    "IPGateway": "",
    "IPSubnet": "",
    "IPDNS": ""
  },
  "scale": {
    "protocol": "GG",
    "customCode": "",
    "baud": 9600
  },
  "profile": "n135",
  "weight": 40.000,
  "beeper": "done",
  "fw_check": true
}
```

Der Trickler sollte sich dann beim Starten mit dem WIFI verbinden und dies im Display anzeigen "Connect to Wifi...".
Bei erfolgreicher Verbindung steht im "Info Tab" die IP-Adresse, über die der Trickler erreichbar ist.

Je nachdem welchen Router du hast, solltest du den Trickler jetzt über http://robo-trickler.local oder http://robo-trickler erreichen. Wenn das nicht funktioniert, benutze die IP-Adresse, die im "Info Tab" erscheint, und gib diese IP im Browser ein, z.B. http://192.168.178.22.


![image](https://github.com/ripper121/RoboTrickler/assets/11836272/6d1b3d82-a14a-4d59-b1f0-4ec83f4624f1)

## File Browser

Mit dem File Browser kannst du die Dateien auf der SD-Karte bequem über den Webbrowser bearbeiten, ohne jedes Mal die SD-Karte herausnehmen zu müssen. Änderungen an der Konfiguration / Profilen werden erst nach einem Neustart übernommen. Dafür ist der Reboot-Button da.

!!! Jede Änderung an einer Config-Datei oder Pulverprofil wird erst nach einem Neustart des Tricklers übernommen !!!

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/e3c420b0-bd87-42ac-ae9b-8e6ad72f0107)

## Pulverprofil Generator

Hilft bei der Erstellung von Pulverprofilen.

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/67fcc33f-f18f-44a9-9f25-5ea08cc997fe)

## Config.txt Generator

Hilft bei der Erstellung und Anpassung der config.txt.

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/4f03a9cb-fd73-49ec-b0ca-dec5daa9c527)

## Web API

Diese Endpunkte können im Browser oder aus einer eigenen Steuerung aufgerufen werden:

* GET http://robo-trickler.local/getWeight: aktuelles Gewicht lesen.
* GET http://robo-trickler.local/getTarget: Zielgewicht lesen.
* GET http://robo-trickler.local/setTarget?targetWeight=WERT: Zielgewicht setzen. Für 40gr -> http://robo-trickler.local/setTarget?targetWeight=40
* GET http://robo-trickler.local/getProfile: aktuelles Profil lesen.
* GET http://robo-trickler.local/getProfileList: Liste der erkannten Profile lesen.
* GET http://robo-trickler.local/setProfile?profileNumber=NUMMER: Profil über die Nummer aus der Profilliste wählen.
* GET http://robo-trickler.local/system/start: Trickeln starten.
* GET http://robo-trickler.local/system/stop: Trickeln stoppen.
* GET http://robo-trickler.local/reboot: Trickler neu starten.

## Reboot

Um den Trickler neu zu starten, klicke auf "Reboot" im Webbrowser oder rufe http://robo-trickler.local/reboot auf.

## Firmware Update

Es gibt zwei Möglichkeiten, ein Firmware-Update durchzuführen:

1. Kopiere die update.bin auf die SD-Karte.
2. Führe das Update via Webbrowser -> "FW Update" oder http://robo-trickler.local/fwupdate durch.

Die neueste Firmware findest du [hier](https://github.com/ripper121/RoboTrickler/releases).

# Waagen

## G&G

Es sollten alle G&G Waagen mit RS232 kompatibel sein.

Eine Empfehlung für Wiederlader direkt von G&G: https://gandg.de/download/anleitungen/Wiederlader%20Infobrosch%C3%BCre.pdf

### Einstellungen

Man sollte alle Filter ausschalten und die Sensibilität auf maximal stellen. Falls der Gewichtswert zu sehr schwanken sollte, spiele etwas mit C1 und C2.

Anleitung für G&G Waagen: https://gandg.de/index.php/downloads

Beispiel Einstellung für die PLC100:

```
C1 - 0 Sensibilität
C2 - 0 Schwingungsfilter
C3 - 6 Baudrate (9600) oder 0 (9600 baud, stream mode), 1 (9600 baud, auto print on stable)
C4 - 27 Gerätenummer
C5 - 0 Autom. Abschaltung
C6 - 0 Belegung der Print-Taste
C7 - 0 Hinteres Display abschalten
```

Video Anleitung:

[![youtube video](https://img.youtube.com/vi/GhTdLqd6Yn4/0.jpg)](https://www.youtube.com/watch?v=GhTdLqd6Yn4)


### RS232 Converter

Male Adapter

Jumper: RXD Links, TXD Rechts

![image](https://github.com/user-attachments/assets/44a68c1d-5b82-4271-a65c-3e39c4b497fd)

Lötpunkte Unterseite PCB (X Geschlossen, Leer Offen):

![image](https://github.com/user-attachments/assets/5b458bc9-3f59-49bc-8881-647bff3cbd19)


### PLC Serie

Empfehlung: die G&G PLC100BC, max. 100g und 0,001g Messbereich reichen aus. Diese habe ich selbst im Einsatz.

* https://waage-shop.com/PLC-Feinwaagen-Tischwaage-Praezisionswaagen_10
* https://www.amazon.de/PLC-Baureihe-Pr%C3%A4zisionswaage-Industriewaage-Tischwaage-Batteriebetrieb/dp/B00ZCRLPY6

### JJ-B Serie

Empfehlung: die G&G JJ100B, max. 100g und 0,001g Messbereich reichen aus. Die G&G JJ200B wurde auch schon getestet.

* https://waage-shop.com/JJ-B-Praezisionwaage-Laborwaage
* https://www.amazon.de/JJ100B-Pr%C3%A4zisionswaage-Laborwaage-Industriewaage-Tischwaage/dp/B004S5V09I

### JJ-BC

Wer es ganz genau haben will, kann diese Serie benutzen. 0,1 mg Messbereich.

* https://waage-shop.com/JJ-BC-Industrie-Analysenwaage-mit-externer-Justierung-120g-01mg-JJ124BC
* https://www.amazon.de/Industrie-Analysenwaage-Pr%C3%A4zisionwaage-Feinwaage-Laborwaage/dp/B00AQZEPZK

## Sartorius

Es sollten alle Sartorius Waagen mit RS232 kompatibel sein. Ich habe das Übertragungsprotokoll Sartorius Balance Interface (SBI) implementiert.

* SI-603A habe ich persönlich (sehr altes Modell, wurde noch von Denver vertrieben).

### RS232 Converter

Female Adapter

Jumper: RXD Links, TXD Rechts

![image](https://github.com/user-attachments/assets/b5c6bbb2-9b60-4f15-b5c3-37ae580ae52b)

Lötpunkte Unterseite PCB (X Geschlossen, Leer Offen):

![image](https://github.com/user-attachments/assets/5b458bc9-3f59-49bc-8881-647bff3cbd19)

### Einstellungen

* DAT.PROT. - SBI
* BAUD - 9600
* PARITY - NONE
* HANDSHK. - NONE
* DATABIT - 8 BITS
* KOM. AUSG. - AUTO.OHN
* ABBRUCH - AUS
* AUTO.ZYK - JEDER
* FORMAT - 16 ZEICH.
* AUTO.TARA - AUS
* DEZ.ZEICH. - DEZ.PUNKT

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/2763ccd4-52f9-4e05-be47-212e6be39fe4)
![image](https://github.com/ripper121/RoboTrickler/assets/11836272/ad07b19b-2c40-4f48-90c5-792e7eefb256)
![image](https://github.com/ripper121/RoboTrickler/assets/11836272/59d735af-b699-481b-b775-00e516723e08)

https://www.sartorius.com/download/492950/manual-cubis-mce-precision-balances-wmc6024-d-data.pdf

Adapter für Waagen mit 25 Pin Port:

https://www.amazon.de/Nedis-Serielles-Kabel-9-Pin-Buchse-Vernickelt/dp/B0CW1SLW3G


Einstellungen für neue Waagen:

https://github.com/ripper121/RoboTrickler/blob/main/Doc/Sartorius_Trickler_Parameter.pdf

## Kern

### RS232 Converter

Male Adapter

Jumper: RXD Rechts, TXD Links

![image](https://github.com/user-attachments/assets/44a68c1d-5b82-4271-a65c-3e39c4b497fd)

Kern 440-21a, Kern PCB 100-3, Kern ABT-120 4NM und Kern EG-220 3NM wurden erfolgreich getestet.

5-Pol Stecker:

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/5ded86e4-43c8-4a66-bfd4-8bf8fca7fd6b)

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/9330e2ba-fd3d-4b44-8cd4-ee5ada302e3a)

<img width="721" height="313" alt="image" src="https://github.com/user-attachments/assets/60f05ad0-8968-4775-8157-455d723be5fe" />

`1	rot	RXD
2	weiss	GND
3	gelb
4	gruen	TXD
5	blau
`

https://www.amazon.de/dp/B000LB4Q7G

Lötpunkte Unterseite PCB (X Geschlossen, Leer Offen):

![image](https://github.com/user-attachments/assets/48750e42-f598-4837-9e98-9678695f4e9f)


### Einstellungen

[Betriebsanleitung Kern 440](https://dok.kern-sohn.com/manuals/files/German/440-BA-d-1643.pdf)

[Betriebsanleitung Kern PCB 100-3](https://www.kern-sohn.com/manuals/files/French/PCB-BA-def-1617.pdf)

Einstellungen für Kern PCB 100-3, die Kern 440-21a hat leider keine Filter Einstellung:

![KernSettings](https://github.com/ripper121/RoboTrickler/assets/11836272/0d9a26fe-68dd-4759-9979-cf476ec7c7a0)

## A&D

### RS232 Converter

Female Adapter

Jumper: RXD Links, TXD Rechts

![image](https://github.com/user-attachments/assets/b5c6bbb2-9b60-4f15-b5c3-37ae580ae52b)

Lötpunkte Unterseite PCB (X Geschlossen, Leer Offen):

![image](https://github.com/user-attachments/assets/9f027fbe-3737-4327-8e98-811adefcde9d)


### Einstellungen

https://www.aandd.jp/products/manual/balances/gxa_com_de.pdf

* Stream-Modus
* Baudrate 9600
* Datenbits 8 Bits
* Parität KEINE (Datenbitlänge 8 Bits)
* Stoppbits 1 Bit
* Code ASCII  

https://www.aandd.jp/products/manual/balances/gp_de.PDF

Kapitel  9.3

* dougt-> Prt -> 3 (Streamermodus/Intervallspeichermodus)  
* dougt-> int -> 0 (Jede Messung)  
* dougt-> PUSE -> 0 (Keine Pause)  
* 5iF-> bPS ->  4 (9600 bps)
* 5iF->btPr -> 2  (8 bits, keine)
* 5iF->t-UP ->  0 (Keine Begrenzung)

Optional:

* bASFnc -> Cond 0 (FAST )
* bASFnc -> 5Pd -> 1 (10-mal/Sekunde)    
* bASFnc -> Pnt -> 0 (Punkt (.))

## Steinberg

SBS-LW-200A
- 2COM
- 9600 brt

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/ad7d7a08-e925-4a65-bd4b-26a1941fcecf)

### RS232 Converter

Male Adapter

Jumper: RXD Links, TXD Rechts

![image](https://github.com/user-attachments/assets/44a68c1d-5b82-4271-a65c-3e39c4b497fd)

Lötpunkte Unterseite PCB (X Geschlossen, Leer Offen):

![image](https://github.com/user-attachments/assets/f6042e03-561b-42ac-8850-440e07cd8451)


# Flash via USB

Lade die usb-flash.zip herunter: https://github.com/ripper121/RoboTrickler/releases/latest

Schließe die Steuerung mit dem USB-Kabel an den PC an und verbinde die Steuerung mit dem Netzteil.


Für Windows:

Entpacke die usb-flash.zip und öffne die flash.bat, dann drücke auf Enter. 


Für Mac und Linux:

Entpacke die usb-flash.zip benutze die esptool.py (dafür gibt es bereits viele Anleitung im Internet).

Download: [esptool.py](https://github.com/espressif/esptool)

esptool.py --chip esp32 erase_flash

esptool.py --chip esp32 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dout --flash_freq 80m --flash_size 4MB 0x1000 ./RoboTricklerUI.ino.bootloader.bin 0x8000 ./RoboTricklerUI.ino.partitions.bin 0xe000 ./boot_app0.bin 0x10000 ./RoboTricklerUI.ino.bin


# Hardware Aufbau

## Trickler
Fülle den Standfuß mit etwas Schwerem, z.B. Bleischrot oder Gips.

Prüfe vor der Montage der Lager, ob das Alurohr in die Lager passt. Sollte es nicht passen, klemme das Alurohr in einen Akkuschrauber/Bohrmaschine und schleife mit Sandpapier den Durchmesser des Rohres herunter. Prüfe regelmäßig, bis alles passt.

Gib vor der Montage der Lager einen Tropfen Sekundenkleber an die Stelle, wo die Lager sitzen werden. Genau so an die Stelle der Motor-Kupplung, in die das Alurohr kommt. Trickler-Körper und Standfuß werden ebenfalls miteinander verklebt.

![image](https://github.com/user-attachments/assets/04f270dc-bacb-49ec-a030-07b381d57200)

3D-Ansicht: https://a360.co/3uPOsYw

## Anschluss an die Waage:

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/402d324c-5ebe-4109-9119-ce106cf7c60f)

## RS232 Konverter

**!!! Verpolung beschädigt den RS232 Stecker !!!**

Hier die Korrekte Verkabelung:

```
| Steuerung | RS232 Stecker |
|-----------|---------------|
| 3V3       | VCC           |
| GND       | GND           |
| SDA       | TXD           |
| SCL       | RXD           |
```

![image](https://github.com/user-attachments/assets/7d2dbbee-4da4-4fb7-a685-2082077151fe)

![image](https://github.com/user-attachments/assets/5992eca3-eed1-496d-ad51-701ceb9727d9)


## Motor Treiber Anschluss

**!!! Verpolung beschädigt den Motor Treiber !!!**

Hier sieht man wie der Motor Treiber richtig rum gesteckt ist (verschiedene Modelle):

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/cd5a5732-4a88-4232-9ace-a2b0bba3e675)

## Motor Treiber Einstellungen

Standard Treiber ist der A4988 mit Full Step:

![image](https://github.com/user-attachments/assets/1453375b-e0a7-45d3-9df5-898fa958f221)

**!!! Achtung manche als A4988 gekennzeichnete Treiber sind in Wirklichkeit TMC2208, man merkt es das der Motor sehr langsam dreht. !!!**

Einstellung für andere Treiber.

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/1617481f-c859-44ee-93a6-0f5c7c211055)

Wenn man eine andere Schrittgröße möchte muss das auch in der Config.txt angepasst werden.

## Gehäuse Aufbau

Kann verschraubt werden, ich verklebe es aber einfach mit Sekundenkleber.

![Case_2](https://github.com/user-attachments/assets/c7cf622f-5486-4696-9e57-ce4c6e4f7e5f)
![Case_1](https://github.com/user-attachments/assets/d891660f-0360-420c-9ee1-87ce05dc7131)

3D Gedruckte Version:

![IMG_20260322_115602](https://github.com/user-attachments/assets/7cf09f91-00d7-4b26-9afe-46f5fa91de24)
![IMG_20260322_120513](https://github.com/user-attachments/assets/94311673-e704-42bd-b006-51ae1d500c21)
![IMG_20260322_120442](https://github.com/user-attachments/assets/e71a2e28-6594-45c5-934f-b6418307989c)
![IMG_20260322_120419](https://github.com/user-attachments/assets/0d701aa7-aaa2-47bb-86e0-ff581bd2e924)



## Alurohr Passung

Falls das Alurohr nicht in die Lager passt (die Toleranzen sind bei den Rohren leider nicht immer optimal), muss man es etwas nachbearbeiten.

Dazu das Rohr einfach in eine Bohrmaschine einspannen und mit Schleifpapier bearbeiten.

![20240710_212914](https://github.com/user-attachments/assets/5b6ddeb4-6d4e-43b2-a49b-fde387b6b977)

![20240710_212923](https://github.com/user-attachments/assets/2413a764-6fdf-4191-a5a3-37876a219bc0)


