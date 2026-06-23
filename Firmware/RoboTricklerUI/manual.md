# Robo-Trickler Anleitung

Stand: Firmware 2.14

> **Keine Sorge:** Diese Anleitung ist bewusst sehr ausführlich und wirkt dadurch umfangreich. Die eigentliche Bedienung des Robo-Tricklers ist aber ganz einfach – für den Einstieg genügt das Kapitel [Erste Schritte](#erste-schritte). Der Rest dient zum Nachschlagen.

## Inhaltsverzeichnis

- [Erste Schritte](#erste-schritte)
- [Bedienung am Display](#bedienung-am-display)
  - [Tab `Trickler`](#tab-trickler)
  - [Tab `Profil`](#tab-profil)
  - [Tab `Info`](#tab-info)
  - [Zähler für fertige Trickles](#zähler-für-fertige-trickles)
  - [Überwurf-Alarm](#überwurf-alarm)
  - [Dialoge](#dialoge)
- [Pulverprofile](#pulverprofile)
  - [Speicherort](#speicherort)
  - [Automatisches Profil aus Kalibrierlauf erstellen](#automatisches-profil-aus-kalibrierlauf-erstellen)
  - [Profil-Tuning](#profil-tuning)
  - [Profil löschen](#profil-löschen)
  - [Pulverprofil-Editor](#pulverprofil-editor)
  - [Gramm / Grain](#gramm--grain)
  - [Aufbau eines Profils](#aufbau-eines-profils)
  - [Mehrere Trickler](#mehrere-trickler)
- [SD-Karte](#sd-karte)
  - [Konfiguration](#konfiguration)
  - [Firmware-Update](#firmware-update)
- [Internes Dateisystem (LittleFS)](#internes-dateisystem-littlefs)
  - [Konfiguration und Profile zwischen Flash und SD synchronisieren](#konfiguration-und-profile-zwischen-flash-und-sd-synchronisieren)
- [WLAN und Webserver](#wlan-und-webserver)
  - [WLAN am Display steuern](#wlan-am-display-steuern)
  - [Access-Point-Einrichtung](#access-point-einrichtung)
  - [Weboberfläche](#weboberfläche)
  - [Weboberfläche offline nutzen](#weboberfläche-offline-nutzen)
  - [Fernsteuerung über den Webbrowser](#fernsteuerung-über-den-webbrowser)
  - [Dateibrowser](#dateibrowser)
  - [Web-API](#web-api)
- [Waagen](#waagen)
  - [Unterstützte Protokolle](#unterstützte-protokolle)
  - [G&G](#gg)
  - [Sartorius](#sartorius)
  - [Kern](#kern)
  - [A&D](#ad)
  - [Steinberg](#steinberg)
- [Flash via USB](#flash-via-usb)
- [Hardware Aufbau](#hardware-aufbau)
  - [Trickler](#trickler)
  - [Anschluss an die Waage](#anschluss-an-die-waage)
  - [RS232 Konverter](#rs232-konverter)
  - [Motor Treiber Anschluss](#motor-treiber-anschluss)
  - [Motor Treiber Einstellungen](#motor-treiber-einstellungen)
  - [Gehäuse Aufbau](#gehäuse-aufbau)
  - [Alurohr Passung](#alurohr-passung)

# Erste Schritte

1. Verbinde die Steuereinheit mit der Waage über den RS-232-Stecker.
2. Verbinde die Steuereinheit mit dem Trickler.
3. Stecke die SD-Karte in die Steuereinheit. Ab Firmware 2.13 läuft der Trickler nach einem USB-Update auch ohne SD-Karte über das interne Dateisystem (siehe [Internes Dateisystem (LittleFS)](#internes-dateisystem-littlefs)) – eine SD-Karte wird aber weiterhin empfohlen.
4. Schalte die Waage an und nulle diese mit leerer Pulverpfanne (TARE).
5. Verbinde die Steuereinheit mit dem Netzteil.

Im Display der Steuerung sollte nun das gleiche Gewicht wie auf der Waage angezeigt werden.

Falls dies nicht der Fall ist, stelle im Tab `Info` über den Waagen-Button das passende Protokoll ein (`GG`, `SBI`, `KERN`, `KERN-ABT`, `KERN-ABS`, `AD`, `CUSTOM` oder `STREAM`). Das Protokoll wird sofort gespeichert, ein Neustart ist nicht nötig. Stimmt die Baudrate nicht (meist `9600`), passe sie in der [Konfiguration](#konfiguration) (`scale.baud`) an. Mehr Details unter [Einstellungen der Waage](#waagen).

> 📸 **Screenshot – Display:** Tab `Info` mit dem Waagen-Button (`Scale: GG`), an dem das Protokoll umgeschaltet wird.

Der Robo-Trickler startet mit dem zuletzt gewählten Profil. Auf der vollständigen SD-Karte ist standardmäßig `avg` vorhanden. Wenn `config.txt` fehlt oder nicht gelesen werden kann, erzeugt die Firmware eine Standard-Konfiguration mit dem Profil `calibrate`.

Wähle im Tab `Profil` ein passendes Pulver aus. Falls das gewünschte Pulver nicht vorhanden ist, kann zum Testen das `avg` Pulverprofil genommen werden. `avg` ist ein Durchschnittsprofil und funktioniert mit vielen Pulvern, ist aber nicht optimal. Für gutes und schnelles Trickeln sollte jedes Pulver ein eigenes Profil bekommen.

Stelle anschließend im Tab `Trickler` das Zielgewicht ein und drücke `Start`. Starte erst, wenn die Waage mit leerer Pulverpfanne auf `0.000` steht. Nach dem ersten Wurf läuft der Trickler automatisch weiter: Sobald du die gefüllte Pfanne abnimmst und die Waage wieder auf `0.000` zurückgeht, dosiert er die nächste Ladung – ein erneutes Drücken von `Start` ist nicht nötig. Zum Beenden drücke `Stop`. Eine Übersicht aller Bedienelemente findest du unter [Bedienung am Display](#bedienung-am-display).

Optional kannst du den Trickler auch über WLAN und den Webbrowser bedienen. Trage dazu deine WLAN-Daten ein oder nutze die [Access-Point-Einrichtung](#access-point-einrichtung) am Display.

**Für jedes Pulver muss ein eigenes Profil angelegt werden, um ein optimales Trickeln zu gewährleisten.**

**Profil erstellen:** Wie du für ein neues Pulver ein passendes Profil anlegst, ist unter [Automatisches Profil aus Kalibrierlauf erstellen](#automatisches-profil-aus-kalibrierlauf-erstellen) beschrieben.

**Jede Waage muss vor dem Gebrauch warm laufen, je nach Modell bis zu 1 Stunde. Sonst kann der angezeigte Wert driften und die Waage geht nach dem Leeren der Pulverpfanne nicht sauber auf 0 zurück.**


<img width="722" height="482" alt="Screen" src="https://github.com/user-attachments/assets/dddc2665-baae-4eac-bc0d-5eb91caa13f8" />

# Bedienung am Display

Die Oberfläche ist in drei Tabs unterteilt: `Trickler`, `Profil` und `Info`. Alle Aktionen lassen sich vollständig am Touchscreen ausführen, eine SD-Karte oder WLAN ist dafür nicht zwingend nötig.

## Tab `Trickler`

* Oben wird das Zielgewicht angezeigt, darunter das aktuell von der Waage gemessene Gewicht.
* Mit `+` und `-` wird das Zielgewicht verändert. Der mittlere Button schaltet die Schrittweite zwischen `0.001`, `0.010`, `0.100`, `1.000` und `10.000` um. Das Zielgewicht ist auf maximal `999` begrenzt.
* Der große Start/Stop-Button startet bzw. stoppt das Trickeln. Beim Starten wird der Button rot und zeigt `Stop`, im Ruhezustand ist er grün und zeigt `Start`.
* Am unteren Rand zeigt eine Statuszeile die Firmware-Version bzw. aktuelle Meldungen an.

> 📸 **Screenshot – Display:** Tab `Trickler` im Ruhezustand. Sichtbar: Zielgewicht oben, darunter `+` / Schrittweite-Button / `-`, gemessenes Gewicht, grüner `Start`-Button und Statuszeile am unteren Rand.

Das geänderte Zielgewicht wird beim Starten des Trickelns in das aktive Profil geschrieben.

### Automatischer Dauerbetrieb

Nach dem Drücken von `Start` läuft der Trickler im Dauerbetrieb:

1. Die Firmware wartet auf eine leere Pulverpfanne (`0.000`) und dosiert die erste Ladung.
2. Ist das Zielgewicht erreicht, ertönt der `done`-Beep und das Gewicht bleibt stehen.
3. Nimm die volle Pfanne ab. Sobald die Waage wieder auf `0.000` zurückgeht, startet automatisch die nächste Ladung – ein erneutes `Start` ist nicht nötig.
4. Zum Beenden drücke `Stop`.

### Farben des Gewichts

Das angezeigte Gewicht wechselt während des Trickelns die Farbe:

* **Weiß**: Trickelvorgang läuft.
* **Grün**: Zielgewicht erreicht, innerhalb der `tolerance`.
* **Gelb**: Zielgewicht erreicht, aber außerhalb der `tolerance`.
* **Rot**: Überwurf bzw. Alarm (`alarmThreshold` überschritten).

> 📸 **Screenshot – Display:** Tab `Trickler` mit grünem Gewicht (Zielgewicht exakt erreicht). Optional zusätzlich ein Bild mit gelbem Gewicht (außerhalb Toleranz).

## Tab `Profil`

* Mit den Pfeil-Buttons (oben/unten) wird durch die erkannten Profile geblättert; das gewählte Profil wird sofort geladen.
* Der orangefarbene Button (Zahnrad) öffnet das Profil-Tuning und passt `steppers.1.weightPerRev` an (siehe [Profil-Tuning](#profil-tuning)).
* Der rote Button (Papierkorb) löscht das ausgewählte Profil nach einer Bestätigung.
* Für das Profil `calibrate` werden Tuning- und Lösch-Button ausgeblendet.

**Während des Trickelns ist der Tab `Profil` gesperrt.** Um das Profil zu wechseln, muss der Trickler zuerst gestoppt werden.

> 📸 **Screenshot – Display:** Tab `Profil` mit einem geladenen Profil (z.B. `avg`). Sichtbar: Profilname in der Mitte, orangefarbener Zahnrad-Button (Tuning) links, roter Papierkorb-Button (Löschen) rechts und die Pfeil-Buttons oben/unten.

## Tab `Info`

* Zeigt den Log-Bereich mit Status- und Fehlermeldungen sowie die IP-Adresse bei aktivem WLAN.
* Beim Start wird hier außerdem angezeigt, ob der Trickler von der SD-Karte oder vom internen Flash gestartet ist.
* WLAN-Button: schaltet WLAN ein/aus (`wifi.enabled`), grün bei aktivem WLAN.
* Waagen-Button (`Scale: …`): schaltet das Abfrageprotokoll der Waage durch (`GG`, `SBI`, `KERN`, `KERN-ABT`, `KERN-ABS`, `AD`, `CUSTOM`, `STREAM`) und speichert es sofort in `scale.protocol`.
* Sind sowohl SD-Karte als auch internes LittleFS verfügbar, erscheinen zwei zusätzliche Buttons zum Synchronisieren von `config.txt` und `/profiles` (siehe [Konfiguration und Profile zwischen Flash und SD synchronisieren](#konfiguration-und-profile-zwischen-flash-und-sd-synchronisieren)).
* Im Access-Point-Modus wird hier der WLAN-QR-Code für die Einrichtung angezeigt.

> 📸 **Screenshot – Display:** Tab `Info` mit Log-Bereich (inkl. IP-Adresse), dem `Scale: …`-Button, dem grünen WLAN-Button und – falls SD und Flash vorhanden – den beiden Synchronisations-Buttons.

## Zähler für fertige Trickles

Der Trickler kann die Anzahl fertig dosierter Ladungen mitzählen. Es gibt zwei unabhängige Zähler:

* **Sitzungszähler** (Profilfeld `general.sessionCounter`): zählt die fertigen Trickles seit dem letzten `Start`. Bei jeder fertigen Ladung innerhalb der Toleranz wird der Stand in der Statuszeile angezeigt (`Fertig … Anzahl: N`). Beim `Stop` wird er auf `0` zurückgesetzt.
* **Gesamtzähler** (`totalCounter.enable`/`totalCounter.count` in `config.txt`): zählt dauerhaft über alle Sitzungen hinweg. Der Stand wird beim `Stop` in `config.txt` gespeichert.

> 📸 **Screenshot – Display:** Statuszeile mit dem Sitzungszähler nach einer fertigen Ladung (`Fertig … Anzahl: N`).

## Überwurf-Alarm

Ist im Profil ein `alarmThreshold` größer `0` gesetzt und wird `targetWeight + alarmThreshold` erreicht oder überschritten, löst die Firmware den Überwurf-Alarm aus: Das Gewicht wird rot, es ertönen drei `done`-Beeps, der Trickler stoppt und es erscheint eine rote Warnmeldung. So fällt eine zu große Ladung sofort auf.

> 📸 **Screenshot – Display:** Rote Warnmeldung des Überwurf-Alarms (Dialog mit roter Schrift), während das gemessene Gewicht über dem Zielgewicht liegt.

## Dialoge

Die Firmware blendet bei Bedarf Dialoge ein, z.B. die Abfrage `Profil aus Kalibrierung erstellen?` (Ja/Nein) nach einem Kalibrierlauf, Lösch- und Synchronisations-Bestätigungen sowie Fehler-, Warn- und Erfolgsmeldungen. Während ein Dialog offen ist, sind die übrigen Bedienelemente gesperrt.

> 📸 **Screenshot – Display:** Beispiele für Dialoge – z.B. eine Bestätigungsabfrage (Ja/Nein) sowie eine Erfolgs- oder Fehlermeldung mit `OK`-Button.

# Pulverprofile

## Speicherort

Pulverprofile liegen in diesem Ordner auf der SD-Karte:

* `/profiles`

Der Profilname in `config.txt` enthält nur den Dateinamen ohne `.txt`. Beispiel:

```json
{
  "activeProfile": "avg"
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
  "measurements": 10,
  "stepper": {
    "id": 1,
    "revolutions": 100,
    "rpm": 200
  }
}
```

Anders als normale Profile (die ihre Würfe als `stepper.steps` angeben) ist `calibrate` ein Sonderfall und gibt den Kalibrierlauf in Motor-**Umdrehungen** an. Dieses Profil nutzt `stepper.revolutions: 100`. Die Firmware rechnet das beim Laden in STEP-Pulse um: `steps = revolutions × stepper.stepsPerRev`. Bei den Standard-`stepper.stepsPerRev: 200` ergibt das einen Kalibrierlauf über 100 Umdrehungen (20000 STEP-Pulse). Der Vorteil: Der Kalibrierlauf bleibt unabhängig von der Mikroschritt-Einstellung immer gleich lang.

**Vorgehen:**

1. Wähle im Tab `Profil` `calibrate`.

<img width="480" height="320" alt="Calibrate" src="https://github.com/user-attachments/assets/36eb5c5b-7721-48a3-9a37-762c74fe1e67" />

2. Im Tab `Trickler` drücke `Start`, achte darauf, dass die Waage auf `0.000` steht.

<img width="480" height="320" alt="Trickle_Main" src="https://github.com/user-attachments/assets/c445d0f0-6b74-408d-820d-f0e4e8454064" />

3. Lasse den Kalibrierlauf bei frisch gefülltem Trickler am besten 3-mal laufen, damit das Rohr gleichmäßig gefüllt ist. Klicke dabei bei `Profil aus Kalibrierung erstellen?` auf `Nein`, damit noch kein neues Profil erstellt wird.

<img width="480" height="320" alt="Calibration_Run" src="https://github.com/user-attachments/assets/a46efe2d-41fb-4140-8a00-f821d6fe14d7" />

4. Nach dem Kalibrierlauf bestätige am Display `Profil aus Kalibrierung erstellen?` mit `Ja`.

<img width="480" height="320" alt="Calibration_Run" src="https://github.com/user-attachments/assets/ce8b4c01-0785-48e5-bdb4-79179fb826fc" />

5. Nach dem Kalibrierlauf liest die Firmware das stabile Gewicht von der Waage und erstellt ein neues Profil.

<img width="480" height="320" alt="Calibration_Save" src="https://github.com/user-attachments/assets/80de2c87-3bf6-448a-a28c-2e319a4ac9ed" />

6. Jetzt kannst du das neue Profil verwenden: einfach ein Zielgewicht einstellen und auf `Start` drücken.

<img width="480" height="320" alt="Trickle_Main" src="https://github.com/user-attachments/assets/34cb2d95-5e9f-447c-9ac6-939140b4d0b9" />


**Infos:**

Die Firmware erstellt automatisch ein neues Profil in `/profiles` mit dem Namen `powder_000.txt`, `powder_001.txt` usw., wählt dieses Profil aus und speichert es in `config.txt`.

Die automatische Erstellung verwendet die Namen `powder_000.txt` bis `powder_999.txt`. Sind alle diese Namen belegt, muss zuerst ein nicht mehr benötigtes Profil gelöscht werden. Die Profilliste der Firmware kann insgesamt bis zu 32 gültige Profile anzeigen.

Ist dieses Limit von 32 Profilen bereits erreicht, prüft die Firmware das schon **vor** dem Kalibrierlauf: Beim Druck auf `Start` (mit gewähltem `calibrate`-Profil) erscheint dann die Meldung `Profillimit erreicht! Kalibrierung nicht möglich.`, und der Kalibrierlauf wird gar nicht erst gestartet. Lösche in diesem Fall zuerst ein nicht mehr benötigtes Profil.

Der automatisch erzeugte Profilaufbau basiert auf der gemessenen Pulvermenge pro 100 Umdrehungen. `weightPerRev` wird aus `Kalibriergewicht / 100` berechnet. Die Firmware legt acht Feinwurf-Einträge an (`1.929`, `0.965`, `0.482`, `0.241`, `0.121`, `0.060`, `0.030`, `0.000` gn mit `2`, `2`, `5`, `5`, `10`, `10`, `15`, `20` Messungen) und verwendet dabei einen Sicherheitsfaktor von 65 % für die berechneten STEP-Pulse. Jeder Eintrag erhält mindestens `5` STEP-Pulse.

## Profil-Tuning

Das Profil kann direkt über die Steuerung angepasst werden.

### 1. Profil auswählen

Wechsle in den Tab `Profil` und wähle das Profil aus, das angepasst werden soll.

![Profil auswählen](https://github.com/user-attachments/assets/b4ba4f5b-84c8-492e-a2bc-4d74446d9ba5)

### 2. Units / Rev anpassen

Klicke auf das **Zahnrad-Symbol**. Nun kannst du den Wert **Units / Rev** anpassen (entspricht `steppers.1.weightPerRev`). Die Schrittweite des Eingabefelds wechselt zwischen `0.001`, `0.010`, `0.100`, `1.000` und `10.000`; zulässig sind Werte von `0.001` bis `99.999`.

![Units / Rev anpassen](https://github.com/user-attachments/assets/208370be-a8fe-4f64-bb73-0af4d2eab1e8)

#### Hinweise

- Je niedriger der Wert, desto mehr Pulver wird pro Wurf dosiert.
- Übertrickelt der Trickler regelmäßig, erhöhe den Wert.
- Verringere den Wert schrittweise, bis der Trickler gerade nicht mehr übertrickelt.
- So erreichst du einen guten Kompromiss zwischen Geschwindigkeit und Genauigkeit.
- Für eine optimale Feinabstimmung empfiehlt es sich, das Profil anschließend manuell anzupassen (siehe [Aufbau eines Profils](#aufbau-eines-profils); bearbeiten lässt es sich über den [Pulverprofil-Editor](#pulverprofil-editor) oder den [Dateibrowser](#dateibrowser)).

### 3. Einstellungen speichern

Klicke auf **Speichern**, um die Änderungen zu übernehmen. Beim Speichern berechnet die Firmware die acht Einträge der `trickleMap` neu und schreibt das Profil direkt auf die SD-Karte. Der Trickler muss dafür gestoppt sein.

![Einstellungen speichern](https://github.com/user-attachments/assets/7a7d0735-f0f2-4740-8e7b-fcf7f4df550e)

### 4. Ergebnis testen

Wechsle zurück in den Tab `Trickler` und teste die neuen Einstellungen.

![Ergebnis testen](https://github.com/user-attachments/assets/b3d187a3-37ab-4deb-ba9e-6d8f17629922)

#### Diagnose-Anzeige beim Trickeln

Während des Trickelns zeigt die Statuszeile (im Tab `Info`) den gerade aktiven Tabelleneintrag an, z.B.:

```text
W0.482 ST167 RPM200 M5/3
```

Das bedeutet: `W` = `diffWeight` des aktiven Eintrags, `ST` = `steps`, `RPM` = `rpm`, `M` = benötigte / aktuelle Anzahl stabiler Messwerte. Diese Anzeige hilft beim Feinabstimmen der `trickleMap`.

> 📸 **Screenshot – Display:** Tab `Info` während eines laufenden Trickelvorgangs mit sichtbarer Diagnosezeile (z.B. `W0.482 ST167 RPM200 M5/3`).

## Profil löschen

### 1. Profil auswählen

Wechsle in den Tab `Profil` und wähle das Profil aus, das gelöscht werden soll.

![Profil auswählen](https://github.com/user-attachments/assets/c185afa4-03b1-41d3-b1b1-f0c7c5ff0df2)

### 2. Profil löschen

Klicke auf das **Löschen-Symbol**, um das ausgewählte Profil zu entfernen. Vor dem Löschen wechselt die Firmware auf `calibrate` und aktualisiert anschließend die Profilliste. Das Profil `calibrate` selbst kann am Display weder angepasst noch gelöscht werden.

> **Hinweis:** Das Löschen eines Profils kann nicht rückgängig gemacht werden.

![Profil löschen](https://github.com/user-attachments/assets/305e6653-df4b-47c8-8035-8d8d7b95fbcd)


## Pulverprofil-Editor

Der Webserver enthält `profileEditor.html` als `Pulverprofil-Editor`. Damit können alle von der Firmware unterstützten Profilfelder (`general`, `steppers.1`/`2` und die `trickleMap`-Schritte sowie die Kalibrier-Profilform) bearbeitet, heruntergeladen und über den Webserver direkt in `/profiles` gespeichert werden. Auch `general.startAtZero` und `general.sessionCounter` sind eigene Eingabefelder.

Wenn du über den Webserver arbeitest, listet das Auswahlfeld `Profil laden:` alle vorhandenen Profile aus `/profiles` auf. Beim Auswählen wird das Profil vom Gerät geladen und in den Editor übernommen; mit `Speichern` wird es unter dem `Profilname` wieder in `/profiles` zurückgeschrieben. Profile lassen sich weiterhin per Drag&Drop oder Einfügen in das Textfeld laden. Starte den Trickler nach dem Speichern neu, damit die neue Profilliste geladen wird.

Der Editor lässt sich auch offline direkt von der SD-Karte öffnen. Offline fehlen das Auswahlfeld `Profil laden:` und das Speichern auf das Gerät; das bearbeitete Profil wird stattdessen über `Herunterladen` als Datei gesichert (siehe [Weboberfläche offline nutzen](#weboberfläche-offline-nutzen)).

> 📸 **Screenshot – Webserver:** Der Pulverprofil-Editor (`profileEditor.html`) mit dem Auswahlfeld `Profil laden:`, einem geladenen Profil, sichtbaren `general`-Feldern, der `trickleMap`-Tabelle und den Buttons zum Herunterladen/Speichern.

## Gramm / Grain

Das Profil muss zur Einheit der Waage passen. Wenn die Waage in Grain ausgibt, müssen `targetWeight`, `diffWeight`, `tolerance`, `alarmThreshold`, `weightGap` und `weightPerRev` ebenfalls in Grain angegeben werden. Wenn die Waage in Gramm ausgibt, müssen diese Werte in Gramm angegeben werden.

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
    "bulkStepper": 1,
    "startAtZero": false,
    "sessionCounter": false,
    "measurements": 5
  },
  "steppers": {
    "1": {
      "enabled": true,
      "weightPerRev": 0.375,
      "rpm": 200
    },
    "2": {
      "enabled": false,
      "weightPerRev": 10.000,
      "rpm": 200
    }
  },
  "trickleMap": [
    {
      "diffWeight": 1.929,
      "measurements": 2,
      "stepper": {
        "id": 1,
        "steps": 669,
        "rpm": 200
      }
    },
    {
      "diffWeight": 0.965,
      "measurements": 2,
      "stepper": {
        "id": 1,
        "steps": 335,
        "rpm": 200
      }
    },
    {
      "diffWeight": 0.482,
      "measurements": 5,
      "stepper": {
        "id": 1,
        "steps": 167,
        "rpm": 200
      }
    },
    {
      "diffWeight": 0.241,
      "measurements": 10,
      "stepper": {
        "id": 1,
        "steps": 84,
        "rpm": 200
      }
    },
    {
      "diffWeight": 0.000,
      "measurements": 15,
      "stepper": {
        "id": 1,
        "steps": 5,
        "rpm": 200
      }
    }
  ]
}
```

### `general`

* `targetWeight`: Wird beim Laden des Profils übernommen. Änderungen am Display werden beim Starten des Trickelns wieder in dieses Profil geschrieben.
* `tolerance`: erlaubte Abweichung zum Zielgewicht.
* `alarmThreshold`: Überwurf-Grenze. Wenn `targetWeight + alarmThreshold` erreicht oder überschritten wird, stoppt die Firmware, piept mehrfach und zeigt eine Warnung an. Bei `0` ist der Alarm deaktiviert.
* `weightGap`: Abstand zum Zielgewicht, bei dem der automatische erste Grobwurf enden soll.
* `bulkStepper`: optionaler Grob-Stepper für den automatischen ersten Grobwurf. Erlaubt sind `1` und `2`. Wenn das Feld fehlt, leer oder ungültig ist, verwendet die Firmware `1`.
* `startAtZero`: Wenn `true`, wartet die Firmware vor dem ersten Wurf auf exakt `0.000`. Wenn `false`, beginnt der erste Wurf bereits, sobald das Gewicht bei oder über `0.000` liegt – die Waage muss also nicht exakt genullt sein.
* `sessionCounter`: Wenn `true`, zeigt die Anzahl der fertigen Trickles seit dem letzten Stop an. Standard ist `false`.
* `measurements`: Anzahl stabiler Messwerte bevor der nächste Tricklevorgang gestartet wird (bei neu aufsetzen der Pulverpfanne).

Wenn `general` fehlt, bleiben die Standardwerte aktiv: `tolerance = 0.000`, `alarmThreshold = 0.000`, `weightGap = 1.000`, `bulkStepper = 1`, `startAtZero = false`, `sessionCounter = false` und `measurements = 20`.

### `steppers`

* `1` und `2`: Einstellungen für Trickler 1 und Trickler 2 (die Objekt-Schlüssel sind die Stepper-Nummern).
* `enabled`: aktiviert den jeweiligen Stepper für den automatischen ersten Grobwurf.
* `weightPerRev`: Pulvermenge pro Umdrehung bei `rpm`.
* `rpm`: Motordrehzahl in U/min für den automatischen ersten Grobwurf.

Der automatische Grobwurf läuft nur beim ersten Wurf. Die Firmware berechnet aus Zielgewicht, aktuellem Gewicht, `weightGap` und `weightPerRev` die benötigten STEP-Pulse. Es wird genau der in `general.bulkStepper` eingetragene Grob-Stepper verwendet; ohne gültigen Eintrag ist das `1`. Wenn der gewählte Stepper nicht aktiviert ist oder `weightPerRev` fehlt bzw. `0` ist, wird der automatische Grobwurf übersprungen.

### `trickleMap`

`trickleMap` ist die eigentliche Trickel-Tabelle. Es sind maximal 16 Einträge möglich.

* `diffWeight`: Abstand zum Zielgewicht, ab dem dieser Eintrag verwendet wird.
* `measurements`: Anzahl stabiler Messwerte, die abgewartet werden, bis dieser Wurf ausgeführt wird.
* `stepper`: gruppiert die Stepper-Bewegung dieses Eintrags:
  * `stepper.id`: `1` oder `2`.
  * `stepper.steps`: Anzahl direkter STEP-Pulse für diesen Wurf. Die Firmware gibt diesen Wert unverändert an den Stepper aus.
  * `stepper.rpm`: Motordrehzahl in U/min. Sinnvolle Werte liegen meist zwischen 5 und 300.

Die Firmware wählt den ersten Eintrag, dessen `diffWeight` noch zum Abstand zwischen aktuellem Gewicht und Zielgewicht passt. Je näher das Zielgewicht kommt, desto kleinere `diffWeight`-Einträge werden verwendet.

Hinweise:

* Bei zu wenigen STEP-Pulsen kann es sein, dass sich der Trickler nicht bewegt.
* Niedrigere Geschwindigkeiten fördern je nach Pulver oft mehr Pulver pro Umdrehung.
* Zu viele `measurements` machen das Trickeln langsam. Am Anfang reichen meist 2 Messungen, am Ende sind 10 bis 15 sinnvoll.

## Mehrere Trickler

Die Firmware kann zwei Trickler (Stepper) unabhängig ansteuern. Das ist nützlich, um z.B. einen schnellen Grob-Trickler für die große Menge weit vom Zielgewicht und einen feinen Trickler für die letzten Körner zu kombinieren.

**Hardware:**

* Stepper 1 (`steppers.1`) wird über den **X-Motoranschluss** des Treiberboards angesteuert.
* Stepper 2 (`steppers.2`) wird über den **Y-Motoranschluss** des Treiberboards angesteuert.

Beide Treiber teilen sich die gemeinsame Enable-Leitung; es ist immer nur ein Stepper gleichzeitig in Bewegung.

**So werden die beiden Trickler verteilt:**

1. **Beide Stepper aktivieren.** Setze im `steppers`-Block bei beiden, die du nutzen willst, `enabled: true` und trage jeweils `weightPerRev` (Pulvermenge pro Umdrehung) und `rpm` (Drehzahl in U/min) passend zum jeweiligen Trickler ein.
2. **Grobwurf-Stepper wählen.** `general.bulkStepper` bestimmt, welcher Stepper den automatischen ersten Grobwurf ausführt (z.B. `2` als großer/schneller Trickler). Dieser Stepper muss `enabled: true` sein und ein gültiges `weightPerRev` größer `0` haben, sonst wird der Grobwurf übersprungen.
3. **Feinwurf pro Tabelleneintrag zuweisen.** Jeder Eintrag in `trickleMap` hat ein eigenes `stepper.id`-Feld (`1` oder `2`). So kannst du je nach Abstand zum Zielgewicht (`diffWeight`) einen anderen Trickler verwenden – typischerweise der große Trickler für die größeren `diffWeight`-Bereiche und der feine Trickler für die letzten Einträge nahe `0`.

Damit übernimmt im folgenden Beispiel Stepper `2` den Grobwurf und den ersten Feinwurf-Bereich (großer/schneller Trickler), während Stepper `1` ab `diffWeight 0.250` die feine Annäherung an das Zielgewicht erledigt:

```json
{
  "general": {
    "targetWeight": 40.000,
    "tolerance": 0.000,
    "alarmThreshold": 1.000,
    "weightGap": 1.000,
    "bulkStepper": 2,
    "startAtZero": false,
    "sessionCounter": false,
    "measurements": 5
  },
  "steppers": {
    "1": {
      "enabled": true,
      "weightPerRev": 0.200,
      "rpm": 200
    },
    "2": {
      "enabled": true,
      "weightPerRev": 2.000,
      "rpm": 200
    }
  },
  "trickleMap": [
    {
      "diffWeight": 2.000,
      "measurements": 2,
      "stepper": {
        "id": 2,
        "steps": 200,
        "rpm": 200
      }
    },
    {
      "diffWeight": 0.250,
      "measurements": 5,
      "stepper": {
        "id": 1,
        "steps": 80,
        "rpm": 200
      }
    },
    {
      "diffWeight": 0.000,
      "measurements": 15,
      "stepper": {
        "id": 1,
        "steps": 5,
        "rpm": 200
      }
    }
  ]
}
```

# SD-Karte

Falls die SD-Karte defekt ist oder beim Bearbeiten Fehler aufgetreten sind, können die SD-Karten-Daten [hier](https://github.com/ripper121/RoboTrickler/releases) neu heruntergeladen werden.

1. SD-Karte mit FAT32 Formatieren
2. Kopiere alle Dateien aus der SD-Files.zip in das Hauptverzeichnis der SD-Karte und starte den Trickler neu.

SD-Karten mit mehr als 32 GB:

Ist die SD-Karte zu groß, gibt es hier eine Anleitung, wie du sie trotzdem mit FAT32 formatieren kannst:
https://www.simon42.com/grosse-sd-karte-formatieren-fat32/

## Konfiguration

Die Konfiguration liegt als `/config.txt` im Hauptverzeichnis der SD-Karte.

```json
{
  "wifi": {
    "enabled": true,
    "ssid": "RoboTrickler-WiFi",
    "psk": "change-me-1234",
    "ipStatic": "192.168.178.50",
    "ipGateway": "192.168.178.1",
    "ipSubnet": "255.255.255.0",
    "ipDns": "8.8.8.8"
  },
  "scale": {
    "protocol": "CUSTOM",
    "customCode": "0x1B 0x70 0x0D 0x0A",
    "baud": 9600
  },
  "stepper": {
    "stepsPerRev": 200
  },
  "activeProfile": "avg",
  "language": "de",
  "beeper": "both",
  "totalCounter": {
    "enable": true,
    "count": 128
  },
  "firmwareUpdate": {
    "check": true
  }
}
```

Die Werte im Beispiel oben dienen nur zur Veranschaulichung. In Klammern steht jeweils der Standardwert, den die Firmware verwendet, wenn `config.txt` fehlt oder das Feld nicht gesetzt ist.

* `wifi.enabled`: aktiviert WLAN, Webserver und alle Netzwerkdienste. Bei `false` startet der Trickler ohne WLAN. Lässt sich auch direkt am Display im Tab `Info` ein- und ausschalten. (Standard: `true`)
* `wifi.ssid`: WLAN-Name. Nur 2.4 GHz WLAN wird unterstützt. (Standard: leer)
* `wifi.psk`: WLAN-Passwort. Bei offenem WLAN leer lassen. (Standard: leer)
* `wifi.ipStatic`: optionale statische IP-Adresse. (Standard: leer = DHCP)
* `wifi.ipGateway`: Gateway-IP, nötig bei statischer IP. (Standard: leer)
* `wifi.ipSubnet`: Subnetzmaske, nötig bei statischer IP. (Standard: leer)
* `wifi.ipDns`: optionaler DNS-Server. Wenn leer, nutzt die Firmware `8.8.8.8`. (Standard: leer)
* Falls DHCP verwendet werden soll, lasse `wifi.ipStatic`, `wifi.ipGateway`, `wifi.ipSubnet` und `wifi.ipDns` leer.
* `scale.protocol`: unterstützte Werte sind `GG`, `SBI`, `KERN`, `KERN-ABT`, `KERN-ABS`, `AD`, `CUSTOM` und leer für kein aktives Anfragekommando. (Standard: `GG`)
* `scale.customCode`: nur bei `CUSTOM`; Hex-Bytefolge wie `0x51 0x0D 0x0A`, mit der Messwerte von der Waage angefordert werden. (Standard: leer)
* `scale.baud`: Baudrate der Waage, meistens `9600`. (Standard: `9600`)
* `stepper.stepsPerRev`: Schritte pro voller Umdrehung der Schrittmotoren (gilt für Stepper1 und Stepper2). Bei einem 1,8°-Schrittmotor sind das `200`; bei anderen Motoren entsprechend anpassen. Beeinflusst die Schrittberechnung beim Trickeln und die Kalibrierung. (Standard: `200`)
* `profile`: Profilname ohne `.txt`. Das Zielgewicht kommt aus `general.targetWeight` im gewählten Profil. (Standard: `calibrate`)
* `language`: Sprache der Oberfläche. Die Firmware normalisiert Werte wie `de-DE` zu `de`. Die Display-Texte werden aus `/lang/<sprache>.json` geladen und fallen auf `/lang/en.json` sowie danach auf eingebaute englische Texte zurück. Die Weboberfläche verwendet getrennte Dateien unter `/system/lang`. (Standard: `en`)
* `beeper`: `done` Beep wenn Trickle fertig, `button` Beep bei Touch betätigung, `both` beides aktiv oder `off` Beeper aus. (Standard: `done`)
* `totalCounter.enable`: aktiviert den dauerhaften Gesamtzähler für fertige Trickles. (Standard: `false`)
* `totalCounter.count`: gespeicherter Stand des dauerhaften Gesamtzählers. (Standard: `0`)
* `firmwareUpdate.check`: aktiviert die automatische Prüfung auf neue Firmware. (Standard: `true`)

Wenn `config.txt` fehlt oder nicht gelesen werden kann, erzeugt die Firmware eine Standard-Konfiguration, zeigt eine Fehlermeldung an und startet neu.

Auf der SD-Karte befindet sich `system/settings.html` (Menüpunkt `Einstellungen`), womit sich alle Konfigurationsfelder inklusive `totalCounter.enable`, `totalCounter.count` und `firmwareUpdate.check` erstellen lassen. Die Seite wird auch über den Webserver bereitgestellt und lässt sich zudem offline direkt von der SD-Karte öffnen (siehe [Weboberfläche offline nutzen](#weboberfläche-offline-nutzen)).

<img width="372" height="1220" alt="image" src="https://github.com/user-attachments/assets/bfb98107-4ebd-4d78-a6bd-ee829973a59f" />


## Firmware-Update

Die neueste Firmware findest du [hier](https://github.com/ripper121/RoboTrickler/releases).

> 📸 **Screenshot – Webserver:** Die Firmware-Update-Seite (`/fwupdate`) mit der angezeigten Firmware-Version, dem Upload-Feld für die `.bin`-Firmware und – bei aktivem LittleFS – dem zusätzlichen Upload-Feld für das `littlefs.bin`-Image.

Es gibt drei Update-Möglichkeiten:

1. Öffne bei aktivem WLAN in der Weboberfläche `Firmware-Update`, wähle die Firmware-Datei `.bin` und lade sie hoch. Nach erfolgreichem Schreiben startet der Trickler neu. Auf dieser Seite kann zusätzlich ein LittleFS-Image (`littlefs.bin`) hochgeladen werden, um das interne Dateisystem zu aktualisieren.
2. Kopiere die Firmware-Datei als `/firmware.bin` in das Hauptverzeichnis der SD-Karte und starte den Trickler. Zusätzlich kann ein LittleFS-Image als `/littlefs.bin` ins Hauptverzeichnis gelegt werden, um das interne Dateisystem zu aktualisieren. Die Firmware prüft schon früh beim Start – noch vor dem Laden von Konfiguration und Profilen – auf diese Dateien. Dadurch funktioniert das Update auch von einer ansonsten leeren SD-Karte, die nur `/firmware.bin` (und optional `/littlefs.bin`) enthält. Der Fortschritt wird dabei am Display angezeigt (in englischer Sprache, da die Konfiguration zu diesem Zeitpunkt noch nicht geladen ist). Nach einem erfolgreichen Update löscht die Firmware die jeweilige Datei und startet neu.

> 📸 **Screenshot – Display:** Statuszeile beim Start während eines SD-Updates (z.B. „Checking SD card updates..." bzw. „SD card update complete. Rebooting..."), bevor die Konfiguration geladen wird.
3. Verwende für eine vollständige Neuinstallation die Anleitung unter [Flash via USB](#flash-via-usb).

`firmwareUpdate.check` steuert nur die automatische Versionsprüfung bei bestehender Netzwerkverbindung. Wird eine neuere Firmware gefunden, zeigt der Trickler einen Hinweis mit der neuen Versionsnummer und der Download-Adresse an. Das eigentliche Update wird nicht automatisch heruntergeladen oder installiert.

> 📸 **Screenshot – Display:** Der Hinweis-Dialog „Neue Firmware verfügbar" mit der neuen Versionsnummer und der Download-Adresse.

# Internes Dateisystem (LittleFS)

Ab Firmware 2.13 (nur nach einem USB-Update) kann der Trickler auch ohne SD-Karte laufen. Dazu liegt im internen Flash ein LittleFS-Image mit der Standard-Konfiguration, den Profilen, den Sprachdateien und der Weboberfläche.

* Beim Start wählt die Firmware automatisch das Dateisystem: Ist eine SD-Karte vorhanden, wird sie bevorzugt, sonst greift sie auf das interne LittleFS zurück.
* Das LittleFS-Image kann über die Weboberfläche unter `Firmware-Update` aktualisiert werden.

## Konfiguration und Profile zwischen Flash und SD synchronisieren

Sind sowohl SD-Karte als auch LittleFS verfügbar, zeigt das Display zwei Synchronisations-Funktionen an:

* **Flash → SD**: kopiert `config.txt` und den Ordner `/profiles` vom internen Flash auf die SD-Karte.
* **SD → Flash**: kopiert `config.txt` und den Ordner `/profiles` von der SD-Karte in den internen Flash.

Vor dem Kopieren erscheint eine Bestätigungsabfrage. Nach Abschluss zeigt die Firmware die Anzahl der kopierten Dateien an; beim Kopieren auf die SD-Karte startet der Trickler anschließend neu. Ist eines der beiden Dateisysteme nicht verfügbar, werden die Funktionen ausgeblendet.

> 📸 **Screenshot – Display:** Tab `Info` mit den beiden sichtbaren Synchronisations-Buttons, dazu optional die Bestätigungsabfrage (`Flash → SD` bzw. `SD → Flash`) als zweites Bild.

# WLAN und Webserver

Um den WLAN-Modus zu aktivieren, trage `ssid` und `psk` in `config.txt` ein und stelle sicher, dass `wifi.enabled` auf `true` steht.

**Nur 2.4 GHz WLAN wird unterstützt.**

Beim Start zeigt der Trickler `Mit WLAN verbinden:` an. Bei erfolgreicher Verbindung steht im Tab `Info` die IP-Adresse.

## WLAN am Display steuern

Im Tab `Info` lässt sich WLAN über den WLAN-Button direkt am Touchscreen ein- und ausschalten. Der Button wird grün, wenn WLAN aktiv ist. Die Einstellung wird in `wifi.enabled` gespeichert und sofort angewendet.

> 📸 **Screenshot – Display:** WLAN-Button im Tab `Info` – am besten zwei Bilder: grün (WLAN ein) und grau (WLAN aus).

## Access-Point-Einrichtung

Kann sich der Trickler nicht mit dem hinterlegten WLAN verbinden oder sind noch keine Zugangsdaten gespeichert, öffnet er einen eigenen Access Point für die Einrichtung:

* WLAN-Name: `Robo-Trickler-AP`
* Passwort: wird automatisch erzeugt und im Tab `Info` am Display angezeigt.
* Adresse der Einrichtungsseite: `http://192.168.4.1`

Im Tab `Info` wird zusätzlich ein QR-Code angezeigt, mit dem ein Smartphone direkt dem Access Point beitreten kann (er enthält WLAN-Name und Passwort). Ein Tippen auf den QR-Code blendet ihn aus, ein Tippen auf den Log-Bereich blendet ihn wieder ein.

> 📸 **Screenshot – Display:** Tab `Info` im Access-Point-Modus mit den angezeigten Zugangsdaten (SSID `Robo-Trickler-AP`, Passwort, `http://192.168.4.1`) und dem WLAN-QR-Code.

Verbinde dich mit dem Access Point und öffne im Browser `http://192.168.4.1`, um die Einrichtungsseite (`/system/ap`) zu erreichen. Dort kannst du nach Netzwerken suchen, die Zugangsdaten eintragen und speichern. Nach dem Speichern startet der Trickler neu und verbindet sich mit dem gewählten WLAN.

> 📸 **Screenshot – Webserver:** Die Einrichtungsseite `/system/ap` im Browser mit der Liste gefundener WLAN-Netzwerke und den Eingabefeldern für SSID und Passwort.

Die meisten Smartphones öffnen die Einrichtungsseite nach dem Verbinden automatisch (Captive Portal). Geschieht das nicht, rufe `http://192.168.4.1` von Hand auf – im Access-Point-Modus werden alle Aufrufe auf die Einrichtungsseite umgeleitet.

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
* Pulverprofil-Editor
* Einstellungen
* WLAN verbinden (öffnet die [Access-Point-Einrichtung](#access-point-einrichtung) unter `/system/ap`)
* Firmware-Update
* Neustart

Darunter führen zwei Links zu den [Firmware-Versionen](https://github.com/ripper121/RoboTrickler/releases/latest) und zum Handbuch im GitHub-Wiki.

Die Weboberfläche ist mehrsprachig. Die Texte werden aus `/system/lang/<sprache>.json` geladen und folgen der in `config.txt` eingestellten `language` (mit Rückfall auf Englisch). Wird eine Seite offline direkt von der SD-Karte geöffnet, richtet sich die Sprache stattdessen nach der Browser-Spracheinstellung (siehe [Weboberfläche offline nutzen](#weboberfläche-offline-nutzen)).

<img width="556" height="675" alt="image" src="https://github.com/user-attachments/assets/a1242108-1b66-4a10-99d4-2867f0f85b6c" />


## Weboberfläche offline nutzen

Die beiden Generator-Seiten lassen sich auch ohne Gerät und ohne WLAN nutzen: Öffne dazu `system/index.html` von der SD-Karte direkt im Browser (z.B. per Doppelklick). In diesem lokalen Modus zeigt die Startseite nur die beiden eigenständigen Werkzeuge an:

* **Einstellungen** (`settings.html`): erzeugt eine `config.txt` zum Herunterladen.
* **Pulverprofil-Editor** (`profileEditor.html`): bearbeitet ein Profil und lädt es als Datei herunter. Das Auswahlfeld `Profil laden:` und das Speichern direkt auf das Gerät stehen offline nicht zur Verfügung – lade ein Profil per Drag&Drop oder Einfügen in das Textfeld und sichere das Ergebnis über `Herunterladen`.

Die gerätegebundenen Funktionen (Trickler-Fernsteuerung, Dateibrowser, `WLAN verbinden`, Firmware-Update, Neustart) werden offline ausgeblendet. Die beiden Werkzeuge können auch direkt geöffnet werden (`system/settings.html` bzw. `system/profileEditor.html`).

Die Sprache der offline geöffneten Seiten richtet sich nach der Spracheinstellung des Browsers (mit Rückfall auf Englisch); eine `config.txt` wird dafür nicht benötigt.


## Fernsteuerung über den Webbrowser

Über den Menüpunkt `Trickler` lässt sich der Trickler komplett aus dem Webbrowser bedienen – praktisch vom Smartphone oder PC aus:

* Pulverprofil aus einer Auswahlliste wählen.
* Zielgewicht mit `+` und `-` einstellen.
* Trickeln mit `Start`/`Stop` steuern.
* Das aktuell gemessene Gewicht wird laufend angezeigt.

Das Setzen von Zielgewicht und Profil wirkt sofort über dieselbe Firmware-Logik wie am Display (`/setTarget` bzw. `/setProfile`); ein Neustart ist dafür nicht nötig.

> 📸 **Screenshot – Webserver:** Die Trickler-Fernsteuerung (`trickler.html`) mit Profil-Auswahlliste, Zielgewicht-Eingabe mit `+`/`-`, dem `Start`/`Stop`-Button und der Live-Gewichtsanzeige.

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
* `GET /list?dir=/PFAD`: Dateien im aktiven Dateisystem auflisten.
* `PUT /system/resources/edit?path`: Datei oder Ordner anlegen.
* `POST /system/resources/edit`: Datei hochladen.
* `DELETE /system/resources/edit?path`: Datei oder Ordner löschen.
* `GET /system/ap`: Access-Point-Einrichtungsseite öffnen.
* `GET /api/wifi/scan`: verfügbare WLAN-Netzwerke als JSON lesen.
* `POST /api/wifi/save`: WLAN-Zugangsdaten speichern (von der Einrichtungsseite verwendet).

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
* leer oder unbekannt (`STREAM`): wartet nur auf eingehende Daten.

Das Protokoll kann auch direkt am Display umgeschaltet werden. Über den Protokoll-Button im Tab `Info` werden die Protokolle nacheinander durchgeschaltet (`GG`, `SBI`, `KERN`, `KERN-ABT`, `KERN-ABS`, `AD`, `CUSTOM`, `STREAM`) und in `scale.protocol` gespeichert.

## G&G

Es sollten alle G&G Waagen mit RS-232 kompatibel sein.

Eine Empfehlung für Wiederlader direkt von G&G: https://gandg.de/download/anleitungen/Wiederlader%20Infobrosch%C3%BCre.pdf

### Einstellungen

Du solltest alle Filter ausschalten und die Sensibilität auf Maximum stellen. Falls der Gewichtswert zu stark schwankt, spiele etwas mit C1 und C2.

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

Kern 440-21a, Kern PCB 100-3, Kern ABT-120 4NM, Kern ABS 80-4 und Kern EG-220 3NM wurden erfolgreich getestet.

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

## A&D

### RS232 Converter

#### Female Adapter

Jumper:
- RXD Links
- TXD Rechts

![image](https://github.com/user-attachments/assets/b5c6bbb2-9b60-4f15-b5c3-37ae580ae52b)

Lötpunkte Unterseite PCB:
- X = Geschlossen
- Leer = Offen

![image](https://github.com/user-attachments/assets/9f027fbe-3737-4327-8e98-811adefcde9d)

---

### FX-i / FZ-i – Werksreset und Konfiguration

**Bedienungsanleitung (PDF):**  
https://weighing.andprecision.com/wp-content/uploads/2017/04/FX-iFZ-i_Bedienungsanleitung_DE.pdf

#### 1. Werksreset durchführen

1. Waage ausschalten.
2. Taste **SAMPLE** gedrückt halten und die Waage einschalten.
3. Zum Menü **init** navigieren.
4. **ALL** auswählen.
5. Mit **PRINT** bestätigen.
6. Die Initialisierung abwarten.
7. Waage neu starten.

---

#### 2. Einheit dauerhaft auf GN (Grain) einstellen

Die Waage startet mit der ersten in der Unit-Liste gespeicherten Einheit. Wird ausschließlich **GN** gespeichert, startet die Waage künftig immer im Modus **GN (Grain)**.

##### GN als einzige Einheit speichern

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

#### 3. Gewünschte Einstellungen konfigurieren

##### Druckeinstellungen (dout)

| Menü | Parameter | Wert | Bedeutung |
|-------|-----------|------|-----------|
| dout | Prt | 0 | Ausgabe nur bei PRINT-Taste bzw. Kommunikationsbefehl |
| dout | int | 0 | Intervallmodus aus / keine automatische Intervallausgabe |
| dout | PUSE | 0 | Keine Pause zwischen Datenausgaben |

##### Schnittstelle (5iF)

| Menü | Parameter | Wert | Bedeutung |
|-------|-----------|------|-----------|
| 5iF | bPS | 4 | 9600 bps |
| 5iF | btPr | 2 | 8 Datenbits, keine Parität, 1 Stoppbit (8N1) |
| 5iF | t-UP | 0 | Keine Übertragungsbegrenzung |

##### Basisfunktionen (bASFnc)

| Menü | Parameter | Wert | Bedeutung |
|-------|-----------|------|-----------|
| bASFnc | Cond | 0 | FAST (schnellste Stabilitätserkennung) |
| bASFnc | 5Pd | 2 | 20 Messungen pro Sekunde |
| bASFnc | Pnt | 0 | Dezimaltrennzeichen Punkt (.) |

---

#### 4. Einstellungen speichern

1. Mit **CAL** das Menü verlassen.
2. Die Einstellungen werden automatisch gespeichert.
3. Waage aus- und wieder einschalten.
4. Funktion prüfen.

---

#### Kurzanweisung

##### Werksreset

```text
SAMPLE → init → ALL → PRINT
```

##### GN als Starteinheit

```text
SAMPLE (halten)
→ Unit
→ PRINT

→ GN mit RE-ZERO auswählen
→ PRINT
→ CAL
```

##### RS232

```text
dout   → Prt  → 0
dout   → int  → 0
dout   → PUSE → 0

5iF    → bPS  → 4
5iF    → btPr → 2
5iF    → t-UP → 0
```

##### Messung

```text
bASFnc → Cond → 0
bASFnc → 5Pd  → 2
bASFnc → Pnt  → 0
```

##### Zielkonfiguration

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

Da die Firmware kein eigenes `STE` Protokoll auswertet, nutze für streamende Steinberg-Waagen den Modus `STREAM` (leeres `scale.protocol`) oder `CUSTOM`, falls deine Waage ein Anfragekommando benötigt.

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

esptool.py --chip esp32 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 8MB 0x1000 ./RoboTricklerUI.ino.bootloader.bin 0x8000 ./RoboTricklerUI.ino.partitions.bin 0xe000 ./boot_app0.bin 0x10000 ./RoboTricklerUI.ino.bin 0x670000 ./littlefs.bin
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

Hier siehst du, wie der Motor-Treiber richtig herum gesteckt ist:

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/cd5a5732-4a88-4232-9ace-a2b0bba3e675)

## Motor Treiber Einstellungen

Standard-Treiber ist der A4988. Die Firmware steuert nur Enable, Richtung und direkte STEP-Pulse.

![image](https://github.com/user-attachments/assets/1453375b-e0a7-45d3-9df5-898fa958f221)

**Achtung: Manche als A4988 gekennzeichnete Treiber sind in Wirklichkeit TMC2208. Du merkst es daran, dass der Motor sehr langsam dreht.**

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

Falls das Alurohr nicht in die Lager passt, musst du es etwas nachbearbeiten.

Dazu das Rohr in eine Bohrmaschine einspannen und mit Schleifpapier bearbeiten.

![20240710_212914](https://github.com/user-attachments/assets/5b6ddeb4-6d4e-43b2-a49b-fde387b6b977)

![20240710_212923](https://github.com/user-attachments/assets/2413a764-6fdf-4191-a5a3-37876a219bc0)
