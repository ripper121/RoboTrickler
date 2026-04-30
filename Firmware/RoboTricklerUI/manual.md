# Robo-Trickler Anleitung

Stand: Firmware 2.10.

## Erste Schritte

1. Verbinde die Steuereinheit mit der Waage über den RS-232-Stecker.
2. Verbinde die Steuereinheit mit dem Trickler.
3. Stelle sicher, dass sich die SD-Karte in der Steuereinheit befindet.
4. Schalte die Waage an und nulle diese mit leerer Pulverpfanne (TARE).
5. Verbinde die Steuereinheit mit dem Netzteil.

Im Display der Steuerung sollte nun das gleiche Gewicht wie auf der Waage angezeigt werden.

Falls dies nicht der Fall ist, überprüfe die [Einstellungen der Waage](https://github.com/ripper121/RoboTrickler/wiki/Anleitung#waagen) und die [Konfiguration](https://github.com/ripper121/RoboTrickler/wiki/Anleitung#konfiguration), besonders `scale.protocol` und `scale.baud`.

Der Robo-Trickler startet mit dem in `config.txt` eingetragenen Profil. Auf der SD-Karte ist standardmäßig `avg` vorhanden. Stelle das Zielgewicht im Display ein und drücke auf Start. Starte erst, wenn die Waage mit leerer Pulverpfanne genullt ist.

Wähle im Profil-Tab ein passendes Pulver aus. Falls das gewünschte Pulver nicht vorhanden ist, kann zum Testen das `avg` Pulverprofil genommen werden. `avg` ist ein Durchschnittsprofil und funktioniert mit vielen Pulvern, ist aber nicht optimal. Für gutes und schnelles Trickeln sollte jedes Pulver ein eigenes Profil bekommen.

**Für jedes Pulver muss ein eigenes Profil angelegt werden, um ein optimales Trickeln zu gewährleisten.**

**Jede Waage muss vor dem Gebrauch warm laufen, je nach Modell bis zu 1 Stunde. Sonst kann der angezeigte Wert driften und die Waage geht nach dem Leeren der Pulverpfanne nicht sauber auf 0 zurück.**

Weitere Informationen:

* [Automatisches Profil aus Kalibrierlauf erstellen](#automatisches-profil-aus-kalibrierlauf-erstellen)
* [Profil Generator](#profil-generator)

![screen](https://github.com/ripper121/RoboTrickler/assets/11836272/988fc66b-6269-4b3c-8205-689d55ea32a8)

# Pulverprofile

## Speicherort

Pulverprofile liegen ab Firmware 2.10 in diesen Ordnern auf der SD-Karte:

* `/profiles`

Der Profilname in `config.txt` enthält nur den Dateinamen ohne `.txt`. Beispiel:

```json
{
  "profile": "avg"
}
```

dazu gehört die Datei:

```text
/profiles/avg.txt
```

Die Profilliste im Display und über die Web-API enthält nur gültige Profile. Ungültige Profile werden beim Scannen ignoriert und im Display gemeldet. Wenn das aktuell ausgewählte Profil beim Start oder beim Starten des Trickelns nicht geladen werden kann, benennt die Firmware die Datei nach `.cor.txt` um, stellt auf `calibrate` um und startet neu.

## Automatisches Profil aus Kalibrierlauf erstellen

Das `calibrate` Profil liegt als `/profiles/calibrate.txt` auf der SD-Karte:

```json
{
  "actuator": "stepper1",
  "steps": 20000,
  "speed": 200,
  "reverse": false
}
```

Dieses Profil dreht den Trickler 100 Umdrehungen, weil 200 Schritte einer Umdrehung entsprechen.

**Die Waage muss für den automatischen Kalibrierlauf auf Grain gestellt sein.** Die Firmware übernimmt den gemessenen Wert direkt als Grain und zeigt die Bestätigung als `gn` an.

**Vorgehen:**

1. Wähle im Profil-Tab `calibrate`.
2. Drücke Start.
3. Lasse den Kalibrierlauf bei frisch gefülltem Trickler am besten 3-mal laufen, damit das Rohr gleichmäßig gefüllt ist. Klicke dabei bei `Create profile from calibration?` auf `No`, damit noch kein neues Profil erstellt wird.
4. Bestätige am Display `Create profile from calibration?` mit `Yes`.
5. Nach dem Kalibrierlauf liest die Firmware das stabile Gewicht von der Waage.

Die Firmware erstellt dann automatisch ein neues Profil in `/profiles` mit dem Namen `powder_000.txt`, `powder_001.txt` usw., wählt dieses Profil aus und speichert es in `config.txt`.

Der automatisch erzeugte Profilaufbau basiert auf der gemessenen Pulvermenge pro 100 Umdrehungen. `unitsPerThrow` wird aus `Kalibriergewicht / 100` berechnet. Die Firmware legt fünf Feinschritte an (`1.929`, `0.965`, `0.482`, `0.241`, `0.000` gn) und verwendet dabei einen Sicherheitsfaktor von 65 % für die berechneten Schrittzahlen.

## Profil Generator

Alternativ kann ein Profil mit `profileGenerator.html` erzeugt werden. Die Datei liegt auf der SD-Karte unter:

```text
/system/profileGenerator.html
```

Wenn WLAN aktiv ist, ist der Generator auch im Webbrowser erreichbar.

Video Anleitung:

[![youtube video](https://img.youtube.com/vi/Y3HOAB9iIfA/0.jpg)](https://www.youtube.com/watch?v=Y3HOAB9iIfA)

1. Öffne den Profile Generator.
2. Trage bei `Weight of calibration run` die Pulvermenge des Kalibrierlaufs ein.
3. Wähle die Einheit `Gram` oder `Grain`.
4. Stelle bei Bedarf `Weight gap`, `Stepper speed rpm`, `Calculation tolerance`, `Threshold for alarm`, `Tolerance` und `Reverse` ein.
5. Klicke auf `Generate Profile`.
6. Trage einen Profilnamen ein, z.B. `n140`.
7. Klicke auf `Download` oder, wenn du über den Webserver arbeitest, auf `Save`.
8. Speichere das Profil als `.txt` in `/profiles`.
9. Starte den Trickler neu, damit die neue Profilliste geladen wird.

Der Generator erzeugt aktuell `general.measurements` mit `20` und die fünf Feinschritte mit `2`, `2`, `5`, `10` und `15` Messungen.

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/67fcc33f-f18f-44a9-9f25-5ea08cc997fe)

## Profile Editor

Der Webserver enthält zusätzlich `profileEditor.html`. Damit können Profile im neuen Format bearbeitet und alte Profile in das neue Format übernommen werden. Alte Felder wie `weight` und `stepsPerUnit` werden vom Editor in `diffWeight` und `unitsPerThrow` umgerechnet.

## Gramm / Grain

Das Profil muss zur Einheit der Waage passen. Wenn die Waage in Grain ausgibt, müssen `targetWeight`, `diffWeight`, `tolerance`, `alarmThreshold`, `weightGap` und `unitsPerThrow` ebenfalls in Grain angegeben werden. Wenn die Waage in Gramm ausgibt, müssen diese Werte in Gramm angegeben werden.

Um Gramm in Grain umzurechnen:

```text
Grain = Gramm * 15.4323583529
```

Empfehlung: Lege für jedes Pulver getrennte Profile für Gramm und Grain an, z.B. `n140_g.txt` und `n140_gn.txt`.

## Aufbau eines Profils

Beispiel für das neue Profilformat:

```json
{
  "general": {
    "targetWeight": 40.000,
    "tolerance": 0.000,
    "alarmThreshold": 1.000,
    "weightGap": 1.000,
    "measurements": 5
  },
  "actuator": {
    "stepper1": {
      "enabled": true,
      "unitsPerThrow": 0.375,
      "unitsPerThrowSpeed": 200
    },
    "stepper2": {
      "enabled": false,
      "unitsPerThrow": 10.000,
      "unitsPerThrowSpeed": 200
    }
  },
  "rs232TrickleMap": [
    {
      "diffWeight": 1.929,
      "actuator": "stepper1",
      "steps": 669,
      "speed": 200,
      "reverse": false,
      "measurements": 2
    },
    {
      "diffWeight": 0.965,
      "actuator": "stepper1",
      "steps": 335,
      "speed": 200,
      "reverse": false,
      "measurements": 2
    },
    {
      "diffWeight": 0.482,
      "actuator": "stepper1",
      "steps": 167,
      "speed": 200,
      "reverse": false,
      "measurements": 5
    },
    {
      "diffWeight": 0.241,
      "actuator": "stepper1",
      "steps": 84,
      "speed": 200,
      "reverse": false,
      "measurements": 10
    },
    {
      "diffWeight": 0.000,
      "actuator": "stepper1",
      "steps": 5,
      "speed": 200,
      "reverse": false,
      "measurements": 15
    }
  ]
}
```

### `general`

* `targetWeight`: Wird beim Laden des Profils übernommen. Änderungen am Display werden beim Starten des Trickelns wieder in dieses Profil geschrieben; Änderungen über `/setTarget` werden sofort geschrieben.
* `tolerance`: erlaubte Abweichung zum Zielgewicht. Bei `0.000` muss der Zielwert ohne Toleranz erreicht werden.
* `alarmThreshold`: Überwurf-Grenze. Wenn `targetWeight + alarmThreshold` erreicht oder überschritten wird, stoppt die Firmware, piept mehrfach und zeigt eine Warnung an. Bei `0` ist der Alarm deaktiviert.
* `weightGap`: Abstand zum Zielgewicht, bei dem der automatische erste Grobwurf enden soll.
* `measurements`: Anzahl stabiler Messwerte, die vor dem ersten Wurf und nach dem automatischen Grobwurf abgewartet werden.

### `actuator`

* `stepper1` und `stepper2`: Einstellungen für Trickler 1 und Trickler 2.
* `enabled`: aktiviert den jeweiligen Stepper für den automatischen ersten Grobwurf.
* `unitsPerThrow`: Pulvermenge pro Umdrehung bei `unitsPerThrowSpeed`. Die Einheit muss zur Waage passen.
* `unitsPerThrowSpeed`: Geschwindigkeit für den automatischen ersten Grobwurf.

Der automatische Grobwurf läuft nur beim ersten Wurf. Die Firmware berechnet aus Zielgewicht, aktuellem Gewicht, `weightGap` und `unitsPerThrow` die benötigten Schritte. Dabei wird erst `stepper2`, danach `stepper1` verwendet, wenn beide aktiviert sind.

### `rs232TrickleMap`

`rs232TrickleMap` ist die eigentliche Trickel-Tabelle. Es sind maximal 16 Einträge möglich.

* `diffWeight`: Abstand zum Zielgewicht, ab dem dieser Schritt verwendet wird.
* `actuator`: `stepper1` oder `stepper2`.
* `steps`: Motorschritte für diesen Wurf. 200 Schritte entsprechen einer Umdrehung.
* `speed`: Motorgeschwindigkeit in U/min. Sinnvolle Werte liegen meist zwischen 10 und 200.
* `reverse`: `true` kehrt die Drehrichtung um, Standard ist `false`.
* `measurements`: Anzahl stabiler Messwerte, die nach diesem Wurf abgewartet werden.

Die Firmware wählt den ersten Eintrag, dessen `diffWeight` noch zum Abstand zwischen aktuellem Gewicht und Zielgewicht passt. Je näher das Zielgewicht kommt, desto kleinere `diffWeight`-Einträge werden verwendet.

Hinweise:

* Bei zu kleinen `steps` kann es sein, dass sich der Trickler nicht bewegt.
* Niedrigere Geschwindigkeiten fördern je nach Pulver oft mehr Pulver pro Umdrehung.
* Zu viele `measurements` machen das Trickeln langsam. Am Anfang reichen meist 2 Messungen, am Ende sind 10 bis 15 sinnvoll.

## Mehrere Trickler

Für mehrere Trickler wird `actuator` verwendet:

```json
{
  "general": {
    "targetWeight": 40.000,
    "tolerance": 0.000,
    "alarmThreshold": 1.000,
    "weightGap": 1.000,
    "measurements": 5
  },
  "actuator": {
    "stepper1": {
      "enabled": true,
      "unitsPerThrow": 0.200,
      "unitsPerThrowSpeed": 200
    },
    "stepper2": {
      "enabled": true,
      "unitsPerThrow": 2.000,
      "unitsPerThrowSpeed": 200
    }
  },
  "rs232TrickleMap": [
    {
      "diffWeight": 2.000,
      "actuator": "stepper2",
      "steps": 200,
      "speed": 200,
      "reverse": false,
      "measurements": 2
    },
    {
      "diffWeight": 0.250,
      "actuator": "stepper1",
      "steps": 80,
      "speed": 200,
      "reverse": false,
      "measurements": 5
    },
    {
      "diffWeight": 0.000,
      "actuator": "stepper1",
      "steps": 5,
      "speed": 200,
      "reverse": false,
      "measurements": 15
    }
  ]
}
```

# SD-Karte

Falls die SD-Karte defekt ist oder beim Bearbeiten Fehler aufgetreten sind, können die SD-Karten-Daten [hier](https://github.com/ripper121/RoboTrickler/releases) neu heruntergeladen werden. Die SD-Karte muss mit FAT32 formatiert sein.

Unter Windows kannst du auch `Create_SD_Card.bat` nutzen. Gib den Laufwerksbuchstaben ein und die SD-Karte wird automatisch erstellt.

Video Anleitung:

[![youtube video](https://img.youtube.com/vi/QKmBHEd7rD4/0.jpg)](https://www.youtube.com/watch?v=QKmBHEd7rD4)

1. Stecke die Micro-SD-Karte in den PC.
2. Lade Etcher herunter: https://etcher.balena.io/#download-etcher
3. Lade das neueste `SD-Files.img` herunter: https://robo-trickler.de
4. Starte Etcher.
5. Wähle `SD-Files.img` aus.
6. Flash.
7. Fertig.

## Konfiguration

Die Konfiguration liegt als `/config.txt` im Hauptverzeichnis der SD-Karte.

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
  "profile": "avg",
  "beeper": "done",
  "fw_check": true
}
```

* `wifi.ssid`: WLAN-Name. Nur 2.4 GHz WLAN wird unterstützt.
* `wifi.psk`: WLAN-Passwort. Bei offenem WLAN leer lassen.
* `wifi.IPStatic`: optionale statische IP-Adresse.
* `wifi.IPGateway`: Gateway-IP, nötig bei statischer IP.
* `wifi.IPSubnet`: Subnetzmaske, nötig bei statischer IP.
* `wifi.IPDNS`: optionaler DNS-Server. Wenn leer, nutzt die Firmware `8.8.8.8`.
* `scale.protocol`: unterstützte Werte sind `GG`, `SBI`, `KERN`, `KERN-ABT`, `AD`, `CUSTOM` und leer für kein aktives Anfragekommando.
* `scale.customCode`: nur bei `CUSTOM`; Kommando, mit dem Messwerte von der Waage angefordert werden.
* `scale.baud`: Baudrate der Waage, meistens `9600`.
* `profile`: Profilname ohne `.txt`. Das Zielgewicht kommt aus `general.targetWeight` im gewählten Profil.
* `beeper`: `done`, `button`, `both` oder `off`.
* `fw_check`: `true` prüft beim WLAN-Start, ob eine neue Firmware verfügbar ist.

Wenn `config.txt` fehlt oder nicht gelesen werden kann, erzeugt die Firmware eine Standard-Konfiguration, zeigt eine Fehlermeldung an und startet neu.

Auf der SD-Karte befindet sich `system/configGenerator.html`, welcher das Erstellen einer Konfiguration erleichtert. Der Generator wird auch über den Webserver bereitgestellt.

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/4f03a9cb-fd73-49ec-b0ca-dec5daa9c527)

## Deserialization failed

Prüfe `config.txt` und das verwendete Pulverprofil mit [dieser Seite](https://jsonformatter.curiousconcept.com). Die Seite zeigt Formatierungsfehler in den Dateien an und korrigiert sie gegebenenfalls.

* Pulverprofile müssen gültiges JSON sein.

## Firmware Update

Es gibt zwei Möglichkeiten, ein Firmware-Update durchzuführen:

1. Kopiere `update.bin` in das Hauptverzeichnis der SD-Karte und starte den Trickler neu. Nach erfolgreichem Update löscht die Firmware die Datei und startet erneut.
2. Führe das Update über den Webbrowser mit `FW Update` oder `http://robo-trickler.local/fwupdate` durch.

Die neueste Firmware findest du [hier](https://github.com/ripper121/RoboTrickler/releases).

# WLAN und Webserver

Um den WLAN-Modus zu aktivieren, trage `ssid` und `psk` in `config.txt` ein.

**Nur 2.4 GHz WLAN wird unterstützt.**

Beim Start zeigt der Trickler `Connect to Wifi...` an. Bei erfolgreicher Verbindung steht im Info-Tab die IP-Adresse.

Je nach Router erreichst du den Trickler über:

```text
http://robo-trickler.local
http://robo-trickler
http://<IP-Adresse>
```

Beispiel:

```text
http://192.168.178.22
```

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/6d1b3d82-a14a-4d59-b1f0-4ec83f4624f1)

## Weboberfläche

Die Startseite lädt `/system/index.html` von der SD-Karte. Von dort erreichst du:

* Trickler-Steuerung
* File Browser
* Profile Generator
* Profile Editor
* Configuration Generator
* Firmware Update
* Reboot

## File Browser

Mit dem File Browser kannst du Dateien auf der SD-Karte über den Webbrowser bearbeiten. Änderungen an `config.txt` oder Pulverprofilen werden erst nach einem Neustart übernommen.

**Jede Änderung an einer Konfigurationsdatei oder einem Pulverprofil wird erst nach einem Neustart des Tricklers übernommen.**

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/e3c420b0-bd87-42ac-ae9b-8e6ad72f0107)

## Web-API

Diese Endpunkte können im Browser oder aus einer eigenen Steuerung aufgerufen werden:

* `GET /getWeight`: aktuelles Gewicht lesen.
* `GET /getTarget`: Zielgewicht lesen.
* `GET /setTarget?targetWeight=WERT`: Zielgewicht setzen und im aktuellen Profil speichern. Erlaubt sind Werte größer `0` und kleiner `999`. Beispiel: `/setTarget?targetWeight=40`.
* `GET /getProfile`: aktuelles Profil lesen.
* `GET /getProfileList`: Liste der erkannten Profile als JSON lesen.
* `GET /setProfile?profileNumber=NUMMER`: Profil über die Nummer aus der Profilliste wählen.
* `GET /system/start`: Trickeln starten.
* `GET /system/stop`: Trickeln stoppen.
* `GET /reboot`: Trickler neu starten.
* `GET /fwupdate`: Firmware-Update-Seite öffnen.
* `POST /update`: Firmware-Datei hochladen.
* `GET /list?dir=/PFAD`: Dateien im SD-Verzeichnis auflisten.
* `PUT /system/resources/edit?path`: Datei oder Ordner anlegen.
* `POST /system/resources/edit`: Datei hochladen.
* `DELETE /system/resources/edit?path`: Datei oder Ordner löschen.

# Waagen

## Unterstützte Protokolle

Die Firmware fragt die Waage je nach `scale.protocol` so ab:

* `GG`: G&G Kommando `ESC p CR LF`.
* `AD`: A&D Kommando `Q CR LF`.
* `KERN`: Kern Kommando `w`.
* `KERN-ABT`: Kern ABT Kommando `D05 CR LF`.
* `SBI`: Sartorius Balance Interface Kommando `P CR LF`.
* `CUSTOM`: sendet `scale.customCode`.
* leer oder unbekannt: wartet nur auf eingehende Daten.

Die Firmware liest die Antwort bis `LF`, extrahiert die Zahl und erkennt `g`, `gn` oder `gr` aus dem Text der Waagenantwort.

## G&G

Es sollten alle G&G Waagen mit RS-232 kompatibel sein.

Eine Empfehlung für Wiederlader direkt von G&G: https://gandg.de/download/anleitungen/Wiederlader%20Infobrosch%C3%BCre.pdf

### Einstellungen

Man sollte alle Filter ausschalten und die Sensibilität auf maximal stellen. Falls der Gewichtswert zu stark schwankt, spiele etwas mit C1 und C2.

Anleitung für G&G Waagen: https://gandg.de/index.php/downloads

Beispiel Einstellung für die PLC100:

```text
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

Jumper: RXD links, TXD rechts

![image](https://github.com/user-attachments/assets/44a68c1d-5b82-4271-a65c-3e39c4b497fd)

Lötpunkte Unterseite PCB (X geschlossen, leer offen):

![image](https://github.com/user-attachments/assets/5b458bc9-3f59-49bc-8881-647bff3cbd19)

### PLC Serie

Empfehlung: G&G PLC100BC, max. 100 g und 0,001 g Messbereich.

* https://waage-shop.com/PLC-Feinwaagen-Tischwaage-Praezisionswaagen_10
* https://www.amazon.de/PLC-Baureihe-Pr%C3%A4zisionswaage-Industriewaage-Tischwaage-Batteriebetrieb/dp/B00ZCRLPY6

### JJ-B Serie

Empfehlung: G&G JJ100B, max. 100 g und 0,001 g Messbereich. Die G&G JJ200B wurde ebenfalls getestet.

* https://waage-shop.com/JJ-B-Praezisionwaage-Laborwaage
* https://www.amazon.de/JJ100B-Pr%C3%A4zisionswaage-Laborwaage-Industriewaage-Tischwaage/dp/B004S5V09I

### JJ-BC

Wer es ganz genau haben will, kann diese Serie benutzen. 0,1 mg Messbereich.

* https://waage-shop.com/JJ-BC-Industrie-Analysenwaage-mit-externer-Justierung-120g-01mg-JJ124BC
* https://www.amazon.de/Industrie-Analysenwaage-Pr%C3%A4zisionwaage-Feinwaage-Laborwaage/dp/B00AQZEPZK

## Sartorius

Es sollten alle Sartorius Waagen mit RS-232 kompatibel sein. Implementiert ist das Sartorius Balance Interface (`SBI`).

### RS232 Converter

Female Adapter

Jumper: RXD links, TXD rechts

![image](https://github.com/user-attachments/assets/b5c6bbb2-9b60-4f15-b5c3-37ae580ae52b)

Lötpunkte Unterseite PCB (X geschlossen, leer offen):

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

Adapter für Waagen mit 25-Pin-Port:

https://www.amazon.de/Nedis-Serielles-Kabel-9-Pin-Buchse-Vernickelt/dp/B0CW1SLW3G

Einstellungen für neue Waagen:

https://github.com/ripper121/RoboTrickler/blob/main/Doc/Sartorius_Trickler_Parameter.pdf

## Kern

Kern 440-21a, Kern PCB 100-3, Kern ABT-120 4NM und Kern EG-220 3NM wurden erfolgreich getestet.

### RS232 Converter

Male Adapter

Jumper: RXD rechts, TXD links

![image](https://github.com/user-attachments/assets/44a68c1d-5b82-4271-a65c-3e39c4b497fd)

5-Pol Stecker:

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/5ded86e4-43c8-4a66-bfd4-8bf8fca7fd6b)

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/9330e2ba-fd3d-4b44-8cd4-ee5ada302e3a)

<img width="721" height="313" alt="image" src="https://github.com/user-attachments/assets/60f05ad0-8968-4775-8157-455d723be5fe" />

```text
1 rot   RXD
2 weiß  GND
3 gelb
4 grün  TXD
5 blau
```

https://www.amazon.de/dp/B000LB4Q7G

Lötpunkte Unterseite PCB (X geschlossen, leer offen):

![image](https://github.com/user-attachments/assets/48750e42-f598-4837-9e98-9678695f4e9f)

### Einstellungen

[Betriebsanleitung Kern 440](https://dok.kern-sohn.com/manuals/files/German/440-BA-d-1643.pdf)

[Betriebsanleitung Kern PCB 100-3](https://www.kern-sohn.com/manuals/files/French/PCB-BA-def-1617.pdf)

Einstellungen für Kern PCB 100-3, die Kern 440-21a hat leider keine Filter-Einstellung:

![KernSettings](https://github.com/ripper121/RoboTrickler/assets/11836272/0d9a26fe-68dd-4759-9979-cf476ec7c7a0)

## A&D

### RS232 Converter

Female Adapter

Jumper: RXD links, TXD rechts

![image](https://github.com/user-attachments/assets/b5c6bbb2-9b60-4f15-b5c3-37ae580ae52b)

Lötpunkte Unterseite PCB (X geschlossen, leer offen):

![image](https://github.com/user-attachments/assets/9f027fbe-3737-4327-8e98-811adefcde9d)

### Einstellungen

https://www.aandd.jp/products/manual/balances/gxa_com_de.pdf

* Stream-Modus
* Baudrate 9600
* Datenbits 8 Bits
* Parität keine
* Stoppbits 1 Bit
* Code ASCII

https://www.aandd.jp/products/manual/balances/gp_de.PDF

Kapitel 9.3:

* `dougt -> Prt -> 3`
* `dougt -> int -> 0`
* `dougt -> PUSE -> 0`
* `5iF -> bPS -> 4`
* `5iF -> btPr -> 2`
* `5iF -> t-UP -> 0`

Optional:

* `bASFnc -> Cond 0`
* `bASFnc -> 5Pd -> 1`
* `bASFnc -> Pnt -> 0`

## Steinberg

SBS-LW-200A

* 2COM
* 9600 brt

Da Firmware 2.10 kein eigenes `STE` Protokoll mehr auswertet, nutze für streamende Steinberg-Waagen ein leeres `scale.protocol` oder `CUSTOM`, falls deine Waage ein Anfragekommando benötigt.

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/ad7d7a08-e925-4a65-bd4b-26a1941fcecf)

### RS232 Converter

Male Adapter

Jumper: RXD links, TXD rechts

![image](https://github.com/user-attachments/assets/44a68c1d-5b82-4271-a65c-3e39c4b497fd)

Lötpunkte Unterseite PCB (X geschlossen, leer offen):

![image](https://github.com/user-attachments/assets/f6042e03-561b-42ac-8850-440e07cd8451)

# Flash via USB

Lade `usb-flash.zip` herunter: https://github.com/ripper121/RoboTrickler/releases/latest

Schließe die Steuerung mit dem USB-Kabel an den PC an und verbinde die Steuerung mit dem Netzteil.

Für Windows:

Entpacke `usb-flash.zip` und öffne `flash.bat`, dann drücke Enter.

Für Mac und Linux:

Entpacke `usb-flash.zip` und benutze `esptool.py`.

Download: [esptool.py](https://github.com/espressif/esptool)

```text
esptool.py --chip esp32 erase_flash

esptool.py --chip esp32 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dout --flash_freq 80m --flash_size 4MB 0x1000 ./RoboTricklerUI.ino.bootloader.bin 0x8000 ./RoboTricklerUI.ino.partitions.bin 0xe000 ./boot_app0.bin 0x10000 ./RoboTricklerUI.ino.bin
```

# Hardware Aufbau

## Trickler

Fülle den Standfuß mit etwas Schwerem, z.B. Bleischrot oder Gips.

Prüfe vor der Montage der Lager, ob das Alurohr in die Lager passt. Sollte es nicht passen, klemme das Alurohr in einen Akkuschrauber oder eine Bohrmaschine und schleife mit Sandpapier den Durchmesser des Rohres herunter. Prüfe regelmäßig, bis alles passt.

Gib vor der Montage der Lager einen Tropfen Sekundenkleber an die Stelle, an der die Lager sitzen werden. Das gleiche gilt für die Stelle der Motor-Kupplung, in die das Alurohr kommt. Trickler-Körper und Standfuß werden ebenfalls miteinander verklebt.

![image](https://github.com/user-attachments/assets/04f270dc-bacb-49ec-a030-07b381d57200)

3D-Ansicht: https://a360.co/3uPOsYw

## Anschluss an die Waage

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/402d324c-5ebe-4109-9119-ce106cf7c60f)

## RS232 Konverter

**Verpolung beschädigt den RS232 Stecker.**

Korrekte Verkabelung:

```text
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

**Verpolung beschädigt den Motor-Treiber.**

Hier sieht man, wie der Motor-Treiber richtig herum gesteckt ist:

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/cd5a5732-4a88-4232-9ace-a2b0bba3e675)

## Motor Treiber Einstellungen

Standard-Treiber ist der A4988 mit Full Step. Die Firmware 2.10 initialisiert beide Stepper mit Full Step.

![image](https://github.com/user-attachments/assets/1453375b-e0a7-45d3-9df5-898fa958f221)

**Achtung: Manche als A4988 gekennzeichnete Treiber sind in Wirklichkeit TMC2208. Man merkt es daran, dass der Motor sehr langsam dreht.**

Einstellung für andere Treiber:

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/1617481f-c859-44ee-93a6-0f5c7c211055)

## Gehäuse Aufbau

Kann verschraubt werden, ich verklebe es aber einfach mit Sekundenkleber.

![Case_2](https://github.com/user-attachments/assets/c7cf622f-5486-4696-9e57-ce4c6e4f7e5f)
![Case_1](https://github.com/user-attachments/assets/d891660f-0360-420c-9ee1-87ce05dc7131)

3D-gedruckte Version:

![IMG_20260322_115602](https://github.com/user-attachments/assets/7cf09f91-00d7-4b26-9afe-46f5fa91de24)
![IMG_20260322_120513](https://github.com/user-attachments/assets/94311673-e704-42bd-b006-51ae1d500c21)
![IMG_20260322_120442](https://github.com/user-attachments/assets/e71a2e28-6594-45c5-934f-b6418307989c)
![IMG_20260322_120419](https://github.com/user-attachments/assets/0d701aa7-aaa2-47bb-86e0-ff581bd2e924)

## Alurohr Passung

Falls das Alurohr nicht in die Lager passt, muss man es etwas nachbearbeiten.

Dazu das Rohr in eine Bohrmaschine einspannen und mit Schleifpapier bearbeiten.

![20240710_212914](https://github.com/user-attachments/assets/5b6ddeb4-6d4e-43b2-a49b-fde387b6b977)

![20240710_212923](https://github.com/user-attachments/assets/2413a764-6fdf-4191-a5a3-37876a219bc0)
