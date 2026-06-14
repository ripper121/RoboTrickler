# Robo-Trickler Anleitung

Stand: Firmware 2.12

## Erste Schritte

1. Verbinde die Steuereinheit mit der Waage über den RS-232-Stecker.
2. Verbinde die Steuereinheit mit dem Trickler.
3. Stelle sicher, dass sich die SD-Karte in der Steuereinheit befindet.
4. Schalte die Waage an und nulle diese mit leerer Pulverpfanne (TARE).
5. Verbinde die Steuereinheit mit dem Netzteil.

Im Display der Steuerung sollte nun das gleiche Gewicht wie auf der Waage angezeigt werden.

Falls dies nicht der Fall ist, überprüfe die [Einstellungen der Waage](#waagen) und die [Konfiguration](#konfiguration), besonders `scale.protocol` und `scale.baud`.

Der Robo-Trickler startet mit dem in `config.txt` eingetragenen Profil. Auf der vollständigen SD-Karte ist standardmäßig `avg` vorhanden. Wenn `config.txt` fehlt oder nicht gelesen werden kann, erzeugt die Firmware eine Standard-Konfiguration mit dem Profil `calibrate`. Stelle das Zielgewicht im Display ein und drücke `Start`. Starte erst, wenn die Waage mit leerer Pulverpfanne genullt ist.

Wähle im Tab `Profil` ein passendes Pulver aus. Falls das gewünschte Pulver nicht vorhanden ist, kann zum Testen das `avg` Pulverprofil genommen werden. `avg` ist ein Durchschnittsprofil und funktioniert mit vielen Pulvern, ist aber nicht optimal. Für gutes und schnelles Trickeln sollte jedes Pulver ein eigenes Profil bekommen.

**Für jedes Pulver muss ein eigenes Profil angelegt werden, um ein optimales Trickeln zu gewährleisten.**

### [Profil erstellen](#automatisches-profil-aus-kalibrierlauf-erstellen)

**Jede Waage muss vor dem Gebrauch warm laufen, je nach Modell bis zu 1 Stunde. Sonst kann der angezeigte Wert driften und die Waage geht nach dem Leeren der Pulverpfanne nicht sauber auf 0 zurück.**


<img width="722" height="482" alt="Screen" src="https://github.com/user-attachments/assets/dddc2665-baae-4eac-bc0d-5eb91caa13f8" />

# Pulverprofile

## Speicherort

Pulverprofile liegen in diesem Ordner auf der SD-Karte:

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

Die Profilliste im Display und über die Web-API enthält nur gültige Profile. Es werden bis zu 32 gültige `.txt`-Profile angezeigt; Dateien mit `.cor.txt` im Namen werden ignoriert. Ungültige Profile werden beim Scannen ignoriert und im Display gemeldet. Wenn das aktuell ausgewählte Profil beim Start oder beim Starten des Trickelns nicht geladen werden kann, benennt die Firmware die Datei nach `.cor.txt` um, stellt auf `calibrate` um und startet neu.

## Automatisches Profil aus Kalibrierlauf erstellen

Das `calibrate` Profil liegt als `/profiles/calibrate.txt` auf der SD-Karte.

```json
{
  "actuator": "stepper1",
  "steps": 20000,
  "speed": 200
}
```

Dieses Profil nutzt `steps: 20000`. Das sind 20000 direkte STEP-Pulse; bei 200 STEP-Pulsen pro Umdrehung entspricht das dem Kalibrierlauf über 100 Umdrehungen.

**Vorgehen:**

1. Wähle im Tab `Profil` `calibrate`.

<img width="480" height="320" alt="Calibrate" src="https://github.com/user-attachments/assets/36eb5c5b-7721-48a3-9a37-762c74fe1e67" />

2. Im Tab `Trickler` drücke `Start`, achte darauf das die Waage auf 0.00 steht.

<img width="480" height="320" alt="Trickle_Main" src="https://github.com/user-attachments/assets/c445d0f0-6b74-408d-820d-f0e4e8454064" />

3. Lasse den Kalibrierlauf bei frisch gefülltem Trickler am besten 3-mal laufen, damit das Rohr gleichmäßig gefüllt ist. Klicke dabei bei `Profil aus Kalibrierung erstellen?` auf `Nein`, damit noch kein neues Profil erstellt wird.

<img width="480" height="320" alt="Calibration_Run" src="https://github.com/user-attachments/assets/a46efe2d-41fb-4140-8a00-f821d6fe14d7" />

4. Nach dem Kalibrierlauf  Bestätige am Display `Profil aus Kalibrierung erstellen?` mit `Ja`.

<img width="480" height="320" alt="Calibration_Run" src="https://github.com/user-attachments/assets/ce8b4c01-0785-48e5-bdb4-79179fb826fc" />

5. Nach dem Kalibrierlauf liest die Firmware das stabile Gewicht von der Waage und erstellt ein neues Profil

<img width="480" height="320" alt="Calibration_Save" src="https://github.com/user-attachments/assets/80de2c87-3bf6-448a-a28c-2e319a4ac9ed" />

6. Jetzt kann man das neue Profil verwenden, einfach ein Zielgewicht einstellen und auf Start drücken

<img width="480" height="320" alt="Trickle_Main" src="https://github.com/user-attachments/assets/34cb2d95-5e9f-447c-9ac6-939140b4d0b9" />


**Infos:**

Die Firmware erstellt automatisch ein neues Profil in `/profiles` mit dem Namen `powder_000.txt`, `powder_001.txt` usw., wählt dieses Profil aus und speichert es in `config.txt`.

Die automatische Erstellung verwendet die 30 Namen `powder_000.txt` bis `powder_029.txt`. Sind alle diese Namen belegt, muss zuerst ein nicht mehr benötigtes Profil gelöscht werden. Die Profilliste der Firmware kann insgesamt bis zu 32 gültige Profile anzeigen.

Der automatisch erzeugte Profilaufbau basiert auf der gemessenen Pulvermenge pro 100 Umdrehungen. `unitsPerThrow` wird aus `Kalibriergewicht / 100` berechnet. Die Firmware legt fünf Feinwurf-Einträge an (`1.929`, `0.965`, `0.482`, `0.241`, `0.000` gn) und verwendet dabei einen Sicherheitsfaktor von 65 % für die berechneten STEP-Pulse.

## Profil-Tuning

Das Profil kann direkt über die Steuerung angepasst werden.

### 1. Profil auswählen

Wechsle in den **Profil-Tab** und wähle das Profil aus, das angepasst werden soll.

![Profil auswählen](https://github.com/user-attachments/assets/b4ba4f5b-84c8-492e-a2bc-4d74446d9ba5)

### 2. Units / Throw anpassen

Klicke auf das **Zahnrad-Symbol**. Nun kannst du den Wert **Units / Throw** anpassen.

![Units / Throw anpassen](https://github.com/user-attachments/assets/208370be-a8fe-4f64-bb73-0af4d2eab1e8)

#### Hinweise

- Je niedriger der Wert, desto mehr Pulver wird pro Wurf dosiert.
- Übertrickelt der Trickler regelmäßig, erhöhe den Wert.
- Verringere den Wert schrittweise, bis der Trickler gerade nicht mehr übertrickelt.
- So erreichst du einen guten Kompromiss zwischen Geschwindigkeit und Genauigkeit.
- Für eine optimale Feinabstimmung empfiehlt es sich, das Profil anschließend manuell anzupassen.

### 3. Einstellungen speichern

Klicke auf **Speichern**, um die Änderungen zu übernehmen.

![Einstellungen speichern](https://github.com/user-attachments/assets/7a7d0735-f0f2-4740-8e7b-fcf7f4df550e)

### 4. Ergebnis testen

Wechsle zurück in den **Trickle-Tab** und teste die neuen Einstellungen.

![Ergebnis testen](https://github.com/user-attachments/assets/b3d187a3-37ab-4deb-ba9e-6d8f17629922)

## Profil löschen

### 1. Profil auswählen

Wechsle in den **Profil-Tab** und wähle das Profil aus, das gelöscht werden soll.

![Profil auswählen](https://github.com/user-attachments/assets/c185afa4-03b1-41d3-b1b1-f0c7c5ff0df2)

### 2. Profil löschen

Klicke auf das **Löschen-Symbol**, um das ausgewählte Profil zu entfernen.

> **Hinweis:** Das Löschen eines Profils kann nicht rückgängig gemacht werden.

![Profil löschen](https://github.com/user-attachments/assets/305e6653-df4b-47c8-8035-8d8d7b95fbcd)


## Profilgenerator

Alternativ kann ein Profil mit `profileGenerator.html` erzeugt werden. Die Datei liegt auf der SD-Karte unter:

```text
/system/profileGenerator.html
```

Wenn WLAN aktiv ist, ist der Generator auch im Webbrowser erreichbar.

Video Anleitung:

[![youtube video](https://img.youtube.com/vi/Y3HOAB9iIfA/0.jpg)](https://www.youtube.com/watch?v=Y3HOAB9iIfA)

1. Öffne den `Pulverprofil-Generator`.
2. Trage bei `Gewicht des Kalibrierlaufs:` die Pulvermenge des Kalibrierlaufs ein.
3. Wähle unter `Einheit` `Gramm:` oder `Grain:`.
4. Stelle bei Bedarf `Gewichtsabstand:`, `Stepper-Drehzahl rpm:`, `Allgemeine Messungen (mit PLC 100-BC 5 nutzen):`, `Bulk actuator:`, `Berechnungstoleranz in %:`, `Alarmgrenze:`, `Toleranz:` und `Nur bei 0.000 starten:` ein.
5. Trage bei `Profilname:` einen Profilnamen ein, z.B. `n140`.
6. Klicke auf `Download` oder, wenn du über den Webserver arbeitest, auf `Speichern`.
7. Speichere das Profil als `.txt` in `/profiles`.
8. Starte den Trickler neu, damit die neue Profilliste geladen wird.

Der Generator übernimmt `Allgemeine Messungen` als `general.measurements`. Für die fünf Feinwurf-Einträge verwendet er mindestens diesen Wert und sonst die Stufen `2`, `2`, `5`, `10` und `15` Messungen. `general.actuator` bestimmt den Stepper für den automatischen ersten Grobwurf. Die Feinwurf-Einträge in `rs232TrickleMap` nutzen weiterhin `stepper1`.

<img width="447" height="1201" alt="image" src="https://github.com/user-attachments/assets/cd842237-bff4-4a58-b9f4-565258d935ef" />

## Pulverprofil-Editor

Der Webserver enthält zusätzlich `profileEditor.html` als `Pulverprofil-Editor`. Damit können die im Editor sichtbaren Profilfelder bearbeitet, heruntergeladen und über den Webserver direkt in `/profiles` gespeichert werden. `general.trickleCounter` wird von der Firmware unterstützt, ist im aktuellen Editor aber kein eigenes Eingabefeld.

## Profile am Display anpassen und löschen

Im Tab `Profil` werden bei allen Profilen außer `calibrate` zwei Wartungsfunktionen angezeigt:

* Mit dem orangefarbenen Einstellungs-Button kann `actuator.stepper1.unitsPerThrow` angepasst werden. Die Schrittweite wechselt zwischen `0.001`, `0.010`, `0.100`, `1.000` und `10.000`; zulässig sind Werte von `0.001` bis `99.999`.
* Beim Speichern berechnet die Firmware die fünf Einträge in `rs232TrickleMap` neu und schreibt das Profil direkt auf die SD-Karte. Der Trickler muss dafür gestoppt sein.
* Mit dem roten Papierkorb-Button kann das ausgewählte Profil nach einer Bestätigung gelöscht werden. Vor dem Löschen wechselt die Firmware auf `calibrate` und aktualisiert anschließend die Profilliste.
* Das Profil `calibrate` kann über das Display weder angepasst noch gelöscht werden.

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
    "actuator": "stepper1",
    "startAtZero": false,
    "trickleCounter": false,
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
      "measurements": 2
    },
    {
      "diffWeight": 0.965,
      "actuator": "stepper1",
      "steps": 335,
      "speed": 200,
      "measurements": 2
    },
    {
      "diffWeight": 0.482,
      "actuator": "stepper1",
      "steps": 167,
      "speed": 200,
      "measurements": 5
    },
    {
      "diffWeight": 0.241,
      "actuator": "stepper1",
      "steps": 84,
      "speed": 200,
      "measurements": 10
    },
    {
      "diffWeight": 0.000,
      "actuator": "stepper1",
      "steps": 5,
      "speed": 200,
      "measurements": 15
    }
  ]
}
```

### `general`

* `targetWeight`: Wird beim Laden des Profils übernommen. Änderungen am Display werden beim Starten des Trickelns wieder in dieses Profil geschrieben.
* `tolerance`: erlaubte Abweichung zum Zielgewicht.
* `alarmThreshold`: Überwurf-Grenze. Wenn `targetWeight + alarmThreshold` erreicht oder überschritten wird, stoppt die Firmware, piept mehrfach und zeigt eine Warnung an. Bei `0` ist der Alarm deaktiviert.
* `weightGap`: Abstand zum Zielgewicht, bei dem der automatische erste Grobwurf enden soll.
* `actuator`: optionaler Bulk-Actuator für den automatischen ersten Grobwurf. Erlaubt sind `stepper1` und `stepper2`. Wenn das Feld fehlt, leer oder ungültig ist, verwendet die Firmware `stepper1`.
* `startAtZero`: Wenn `true`, wartet die Firmware vor dem ersten Wurf auf exakt `0.000`. Wenn `false`, startet der erste Wurf bei jedem Gewicht ab `0.000`.
* `trickleCounter`: Wenn `true`, zeigt die Anzahl der fertigen Trickles seit dem letzten Stop an. Standard ist `false`.
* `measurements`: Anzahl stabiler Messwerte bevor der nächste Tricklevorgang gestartet wird (bei neu aufsetzen der Pulverpfanne).

Wenn `general` fehlt, bleiben die Standardwerte aktiv: `tolerance = 0.000`, `alarmThreshold = 0.000`, `weightGap = 1.000`, `actuator = stepper1`, `startAtZero = false`, `trickleCounter = false` und `measurements = 20`.

### `actuator`

* `stepper1` und `stepper2`: Einstellungen für Trickler 1 und Trickler 2.
* `enabled`: aktiviert den jeweiligen Stepper für den automatischen ersten Grobwurf.
* `unitsPerThrow`: Pulvermenge pro Umdrehung bei `unitsPerThrowSpeed`.
* `unitsPerThrowSpeed`: Geschwindigkeit für den automatischen ersten Grobwurf.

Der automatische Grobwurf läuft nur beim ersten Wurf. Die Firmware berechnet aus Zielgewicht, aktuellem Gewicht, `weightGap` und `unitsPerThrow` die benötigten STEP-Pulse. Es wird genau der in `general.actuator` eingetragene Bulk-Actuator verwendet; ohne gültigen Eintrag ist das `stepper1`. Wenn der gewählte Stepper nicht aktiviert ist oder `unitsPerThrow` fehlt bzw. `0` ist, wird der automatische Grobwurf übersprungen.

### `rs232TrickleMap`

`rs232TrickleMap` ist die eigentliche Trickel-Tabelle. Es sind maximal 16 Einträge möglich.

* `diffWeight`: Abstand zum Zielgewicht, ab dem dieser Eintrag verwendet wird.
* `actuator`: `stepper1` oder `stepper2`.
* `steps`: Anzahl direkter STEP-Pulse für diesen Wurf. Die Firmware gibt diesen Wert unverändert an den Stepper aus.
* `speed`: Motorgeschwindigkeit in U/min. Sinnvolle Werte liegen meist zwischen 5 und 300.
* `measurements`: Anzahl stabiler Messwerte die abgewartet bis dieser Wurf ausgeführt wird.

Die Firmware wählt den ersten Eintrag, dessen `diffWeight` noch zum Abstand zwischen aktuellem Gewicht und Zielgewicht passt. Je näher das Zielgewicht kommt, desto kleinere `diffWeight`-Einträge werden verwendet.

Hinweise:

* Bei zu wenigen STEP-Pulsen kann es sein, dass sich der Trickler nicht bewegt.
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
    "actuator": "stepper2",
    "startAtZero": false,
    "trickleCounter": false,
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
      "measurements": 2
    },
    {
      "diffWeight": 0.250,
      "actuator": "stepper1",
      "steps": 80,
      "speed": 200,
      "measurements": 5
    },
    {
      "diffWeight": 0.000,
      "actuator": "stepper1",
      "steps": 5,
      "speed": 200,
      "measurements": 15
    }
  ]
}
```

# SD-Karte

Falls die SD-Karte defekt ist oder beim Bearbeiten Fehler aufgetreten sind, können die SD-Karten-Daten [hier](https://github.com/ripper121/RoboTrickler/releases) neu heruntergeladen werden.

1. SD-Karte mit FAT32 Formatieren
2. Kopiere alle Dateien aus der SD-Files.zip in das Hauptverzeichnis der SD-Karte und starte den Trickler neu.

SD-Karten mit mehr als 32 GB:

Ist die SD-Karte zu groß, gibt es hier eine Anleitung wie man sie Trotzdem mit FAT32 formatieren kann:
https://www.simon42.com/grosse-sd-karte-formatieren-fat32/

## Konfiguration

Die Konfiguration liegt als `/config.txt` im Hauptverzeichnis der SD-Karte.

```json
{
  "wifi": {
    "ssid": "RoboTrickler-WiFi",
    "psk": "change-me-1234",
    "IPStatic": "192.168.178.50",
    "IPGateway": "192.168.178.1",
    "IPSubnet": "255.255.255.0",
    "IPDNS": "8.8.8.8"
  },
  "scale": {
    "protocol": "CUSTOM",
    "customCode": "0x1B 0x70 0x0D 0x0A",
    "baud": 9600
  },
  "profile": "avg",
  "language": "de",
  "beeper": "both",
  "trickleCounter": true,
  "trickleCount": 128,
  "fw_update": {
    "check": true
  }
}
```

* `wifi.ssid`: WLAN-Name. Nur 2.4 GHz WLAN wird unterstützt.
* `wifi.psk`: WLAN-Passwort. Bei offenem WLAN leer lassen.
* `wifi.IPStatic`: optionale statische IP-Adresse.
* `wifi.IPGateway`: Gateway-IP, nötig bei statischer IP.
* `wifi.IPSubnet`: Subnetzmaske, nötig bei statischer IP.
* `wifi.IPDNS`: optionaler DNS-Server. Wenn leer, nutzt die Firmware `8.8.8.8`.
* Falls DHCP verwendet werden soll, lasse `wifi.IPStatic`, `wifi.IPGateway`, `wifi.IPSubnet` und `wifi.IPDNS` leer.
* `scale.protocol`: unterstützte Werte sind `GG`, `SBI`, `KERN`, `KERN-ABT`, `KERN-ABS`, `AD`, `CUSTOM` und leer für kein aktives Anfragekommando.
* `scale.customCode`: nur bei `CUSTOM`; Hex-Bytefolge wie `0x51 0x0D 0x0A`, mit der Messwerte von der Waage angefordert werden.
* `scale.baud`: Baudrate der Waage, meistens `9600`.
* `profile`: Profilname ohne `.txt`. Das Zielgewicht kommt aus `general.targetWeight` im gewählten Profil.
* `language`: Sprache der Oberfläche. Die Firmware normalisiert Werte wie `de-DE` zu `de`. Die Display-Texte werden aus `/lang/<sprache>.json` geladen und fallen auf `/lang/en.json` sowie danach auf eingebaute englische Texte zurück. Die Weboberfläche verwendet getrennte Dateien unter `/system/lang`.
* `beeper`: `done` Beep wenn Trickle fertig, `button` Beep bei Touch betätigung, `both` beides aktiv oder `off` Beeper aus.
* `trickleCounter`: aktiviert den dauerhaften Gesamtzaehler fuer fertige Trickles.
* `trickleCount`: gespeicherter Stand des dauerhaften Gesamtzaehlers.
* `fw_update.check`: aktiviert die automatische Prüfung auf neue Firmware.

Wenn `config.txt` fehlt oder nicht gelesen werden kann, erzeugt die Firmware eine Standard-Konfiguration, zeigt eine Fehlermeldung an und startet neu.

Auf der SD-Karte befindet sich `system/configGenerator.html`, welcher das Erstellen der wichtigsten Konfigurationsfelder und `fw_update.check` erleichtert. Der Generator wird auch über den Webserver bereitgestellt. `trickleCounter` und `trickleCount` werden von der Firmware unterstützt, werden vom aktuellen Konfigurationsgenerator aber nicht erzeugt.

<img width="372" height="1220" alt="image" src="https://github.com/user-attachments/assets/bfb98107-4ebd-4d78-a6bd-ee829973a59f" />


## Firmware-Update

Die neueste Firmware findest du [hier](https://github.com/ripper121/RoboTrickler/releases).

Es gibt drei Update-Möglichkeiten:

1. Öffne bei aktivem WLAN in der Weboberfläche `Firmware-Update`, wähle die Firmware-Datei `.bin` und lade sie hoch. Nach erfolgreichem Schreiben startet der Trickler neu.
2. Kopiere die Firmware-Datei als `/update.bin` in das Hauptverzeichnis der SD-Karte und starte den Trickler. Nach einem erfolgreichen Update löscht die Firmware die Datei und startet neu.
3. Verwende für eine vollständige Neuinstallation die Anleitung unter [Flash via USB](#flash-via-usb).

`fw_update.check` steuert nur die automatische Versionsprüfung bei bestehender Netzwerkverbindung. Das eigentliche Update wird nicht automatisch heruntergeladen oder installiert.

# WLAN und Webserver

Um den WLAN-Modus zu aktivieren, trage `ssid` und `psk` in `config.txt` ein.

**Nur 2.4 GHz WLAN wird unterstützt.**

Beim Start zeigt der Trickler `Mit WLAN verbinden:` an. Bei erfolgreicher Verbindung steht im Tab `Info` die IP-Adresse.

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

<img width="564" height="803" alt="image" src="https://github.com/user-attachments/assets/1020a029-fa60-4a1b-92d6-dadfe88fd2e2" />


## Weboberfläche

Die Startseite lädt `/system/index.html` von der SD-Karte. Von dort erreichst du:

* Trickler
* Dateibrowser
* Pulverprofil-Generator
* Pulverprofil-Editor
* Konfigurationsgenerator
* Firmware-Update
* Neustart

<img width="556" height="675" alt="image" src="https://github.com/user-attachments/assets/a1242108-1b66-4a10-99d4-2867f0f85b6c" />


## Dateibrowser

Mit dem `Dateibrowser` kannst du Dateien auf der SD-Karte über den Webbrowser bearbeiten. Änderungen an `config.txt` oder Pulverprofilen werden erst nach einem Neustart übernommen.

**Jede Änderung an einer Konfigurationsdatei oder einem Pulverprofil wird erst nach einem Neustart des Tricklers übernommen.**

Ausnahmen sind die Web-API-Funktionen `/setTarget` und `/setProfile`: Sie speichern das Zielgewicht bzw. Profil sofort über die Firmware-Logik.

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/e3c420b0-bd87-42ac-ae9b-8e6ad72f0107)

## Web-API

Diese Endpunkte können im Browser oder aus einer eigenen Steuerung aufgerufen werden:

* `GET /getTricklerState`: aktuelles Gewicht und Laufstatus als JSON lesen, z. B. `{"weight":40.000,"running":true}`.
* `GET /getTarget`: Zielgewicht lesen.
* `GET /setTarget?targetWeight=WERT`: Zielgewicht setzen und im aktuellen Profil speichern. Erlaubt sind Werte größer `0` und kleiner `999`. Beispiel: `/setTarget?targetWeight=40`.
* `GET /getProfile`: aktuelles Profil lesen.
* `GET /getLanguage`: aktuell geladene Sprache lesen.
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
* `AD`: A&D Kommando `SI CR LF`.
* `KERN`: Kern Kommando `w`.
* `KERN-ABT`: Kern ABT Kommando `D05 CR LF`.
* `KERN-ABS`: Kern ABS Kommando `D01 CR LF`.
* `SBI`: Sartorius Balance Interface Kommando `P CR LF`.
* `CUSTOM`: sendet `scale.customCode` als Hex-Bytefolge, z. B. `0x51 0x0D 0x0A`.
* leer oder unbekannt: wartet nur auf eingehende Daten.

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

Kern 440-21a, Kern PCB 100-3, Kern ABT-120 4NM, Kern ABS 80-4  und Kern EG-220 3NM wurden erfolgreich getestet.

| Befehl | Verhalten                               |
|---------|-----------------------------------------|
| D01     | Stream aller Messwerte                  |
| D05     | Einmal aktuelle Messung senden          |
| D06     | Nur stabile Wägungen automatisch senden |
| D08     | Einmal senden, aber erst wenn stabil    |
| D09     | D01/D06 stoppen                         |

Kern ABT-120 4NM -> D05:

```json
{
  "protocol": "KERN-ABT",
  "baud": 9600
}
```

Kern ABS 80-4 -> D01:

```json
{
  "protocol": "KERN-ABS",
  "baud": 9600
}
```

```text
intFACE
 └─ iF:USEr
     ├─ io.b:9600
     ├─ io.P:P-no    (8 Bit, keine Parität)
     ├─ io.S:S-S1    (1 Stopbit)
     ├─ io.H:H-oFF   (kein Handshake)
     └─ io.d:d-CrLF  (CR/LF empfohlen)

FUnC.SEL
 ├─ trC:on      Auto-Zero ON
 ├─ b-1         Sehr ruhige Umgebung (0,1 mg)
 ├─ AP-oF       Auto Print OFF
 ├─ Ad-on       Analog-/Kapazitätsanzeige ON
 └─ Stnd        Standard-Wägemodus
```


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

# A&D

## RS232 Converter

### Female Adapter

Jumper:
- RXD Links
- TXD Rechts

![image](https://github.com/user-attachments/assets/b5c6bbb2-9b60-4f15-b5c3-37ae580ae52b)

Lötpunkte Unterseite PCB:
- X = Geschlossen
- Leer = Offen

![image](https://github.com/user-attachments/assets/9f027fbe-3737-4327-8e98-811adefcde9d)

---

## FX-i / FZ-i – Werksreset und Konfiguration

**Bedienungsanleitung (PDF):**  
https://weighing.andprecision.com/wp-content/uploads/2017/04/FX-iFZ-i_Bedienungsanleitung_DE.pdf

### 1. Werksreset durchführen

1. Waage ausschalten.
2. Taste **SAMPLE** gedrückt halten und die Waage einschalten.
3. Zum Menü **init** navigieren.
4. **ALL** auswählen.
5. Mit **PRINT** bestätigen.
6. Die Initialisierung abwarten.
7. Waage neu starten.

---

### 2. Einheit dauerhaft auf GN (Grain) einstellen

Die Waage startet mit der ersten in der Unit-Liste gespeicherten Einheit. Wird ausschließlich **GN** gespeichert, startet die Waage künftig immer im Modus **GN (Grain)**.

#### GN als einzige Einheit speichern

```text
SAMPLE (gedrückt halten)
→ Unit
→ PRINT

Mit SAMPLE bis GN wechseln.
Mit RE-ZERO GN auswählen.
(Stabilitätssymbol erscheint.)

Keine weiteren Einheiten auswählen.

PRINT
CAL
```

---

### 3. Gewünschte Einstellungen konfigurieren

#### Druckeinstellungen (dout)

| Menü | Parameter | Wert | Bedeutung |
|-------|-----------|------|-----------|
| dout | Prt | 0 | Ausgabe nur bei PRINT-Taste bzw. Kommunikationsbefehl |
| dout | int | 0 | Intervallmodus aus / keine automatische Intervallausgabe |
| dout | PUSE | 0 | Keine Pause zwischen Datenausgaben |

#### Schnittstelle (5iF)

| Menü | Parameter | Wert | Bedeutung |
|-------|-----------|------|-----------|
| 5iF | bPS | 4 | 9600 bps |
| 5iF | btPr | 2 | 8 Datenbits, keine Parität, 1 Stoppbit (8N1) |
| 5iF | t-UP | 0 | Keine Übertragungsbegrenzung |

#### Basisfunktionen (bASFnc)

| Menü | Parameter | Wert | Bedeutung |
|-------|-----------|------|-----------|
| bASFnc | Cond | 0 | FAST (schnellste Stabilitätserkennung) |
| bASFnc | 5Pd | 2 | 20 Messungen pro Sekunde |
| bASFnc | Pnt | 0 | Dezimaltrennzeichen Punkt (.) |

---

### 4. Einstellungen speichern

1. Mit **CAL** das Menü verlassen.
2. Die Einstellungen werden automatisch gespeichert.
3. Waage aus- und wieder einschalten.
4. Funktion prüfen.

---

### Kurzanweisung

#### Werksreset

```text
SAMPLE → init → ALL → PRINT
```

#### GN als Starteinheit

```text
SAMPLE (halten)
→ Unit
→ PRINT

→ GN mit RE-ZERO auswählen
→ PRINT
→ CAL
```

#### RS232

```text
dout   → Prt  → 0
dout   → int  → 0
dout   → PUSE → 0

5iF    → bPS  → 4
5iF    → btPr → 2
5iF    → t-UP → 0
```

#### Messung

```text
bASFnc → Cond → 0
bASFnc → 5Pd  → 2
bASFnc → Pnt  → 0
```

#### Zielkonfiguration

- Wägeeinheit: GN (Grain)
- RS-232: 9600 Baud, 8N1
- FAST-Modus
- 20 Messungen pro Sekunde
- Dezimalpunkt (.)
- Keine automatische Intervallausgabe
- Keine Pause zwischen Datenausgaben
- Keine Übertragungsbegrenzung

## Steinberg

SBS-LW-200A

* 2COM
* 9600 brt

Da Firmware 2.12 kein eigenes `STE` Protokoll auswertet, nutze für streamende Steinberg-Waagen ein leeres `scale.protocol` oder `CUSTOM`, falls deine Waage ein Anfragekommando benötigt.

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

![image](https://github.com/user-attachments/assets/4fef64f3-8915-4898-8454-f5276cb16af0)

![image](https://github.com/user-attachments/assets/2425d182-7688-447e-9712-952fe3b3e999)

![image](https://github.com/user-attachments/assets/07e61690-ed64-4d9b-a55a-3a6080bbe288)


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

Standard-Treiber ist der A4988. Die Firmware steuert nur Enable, Richtung und direkte STEP-Pulse.

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
