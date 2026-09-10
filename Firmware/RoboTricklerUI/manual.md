# Robo-Trickler Anleitung

Stand: Firmware 2.14

Dieses Handbuch beginnt mit einer kurzen Anleitung für die erste Inbetriebnahme. Die weiteren Abschnitte beschreiben die Bedienung und Einstellungen im Detail und dienen zum Nachschlagen.

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
  - [Wann werden Änderungen übernommen?](#wann-werden-änderungen-übernommen)
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
- [Fehlersuche](#fehlersuche)
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

Dieser Abschnitt gilt für eine fertig aufgebaute Steuerung mit installierter Firmware. WLAN und ein Computer sind für die Bedienung am Display nicht erforderlich.

## 1. Anschließen

1. Verbinde die ausgeschaltete Steuerung mit der Waage (RS-232) und dem Trickler.
2. Stecke die vorbereitete SD-Karte ein. Bei einer 8-MB-Steuerung mit eingerichtetem internem Dateisystem ist der Betrieb auch ohne SD-Karte möglich.
3. Schalte die Waage ein und lasse sie nach ihrer Anleitung warm laufen.
4. Setze die leere Pulverpfanne auf und drücke an der Waage `TARE`, sodass sie null anzeigt.
5. Verbinde die Steuerung mit dem Netzteil.

**Das Gewicht auf der Steuerung muss mit der Waage übereinstimmen.** Falls nicht, wähle im Tab `Info` über den Waagen-Button das passende Protokoll aus. Prüfe bei `NaN...` oder `Timeout!` zuerst die Verbindung und die [Waagen-Einstellungen](#waagen).

## 2. Profil auswählen

Öffne den Tab `Profil` und wähle mit den Pfeilen dein passendes Pulverprofil.

**Achtung:** Eine neue Installation startet mit `calibrate`. Das ist ein Sonderprofil für die Profilerstellung, kein normales Dosierprofil. Fehlt dein Profil, folge zuerst der Anleitung unter [Automatisches Profil aus Kalibrierlauf erstellen](#automatisches-profil-aus-kalibrierlauf-erstellen).

Die Einheit des Profils muss zur Waage passen: **Gramm und Grain werden nicht automatisch umgerechnet.**

## 3. Zielgewicht einstellen und starten

1. Wechsle in den Tab `Trickler`.
2. Stelle oben das gewünschte Zielgewicht mit `+` und `-` ein. Mit dem mittleren Button wechselst du die Schrittweite.
3. Prüfe Profil, Zielgewicht und Einheit. Die Waage soll mit leerer Pfanne null anzeigen.
4. Drücke den großen grünen `Start`-Button. Während des Betriebs ist er rot und dient als `Stop`-Button.

Die Profilwahl und ein am Display geändertes Zielgewicht werden beim Start gespeichert.

## 4. Fertigmeldung und nächste Ladung

Bei erreichter Zielmenge zeigt die Steuerung die Fertigmeldung an. Ein Signalton ertönt, sofern er eingeschaltet ist.

* **Grün:** Das Gewicht liegt innerhalb der eingestellten Toleranz.
* **Gelb oder rote Warnmeldung:** Prüfe das Gewicht.

**Der Betrieb läuft automatisch weiter:** Nimm die gefüllte Pfanne ab, leere sie und setze sie wieder auf. Sobald die Waage wieder null anzeigt, beginnt die nächste Ladung. Du musst nicht erneut `Start` drücken.

## 5. Beenden

Drücke `Stop`, bevor du das Profil oder Zielgewicht änderst, die Verbindung prüfst oder das Gerät ausschaltest. Während des Betriebs sind die Tabs `Profil` und `Info` gesperrt.

Drücke bei `NaN...`, `Timeout!` oder einer anderen Störung ausdrücklich `Stop`. Ein fehlender Messwert beendet den Betriebszustand nicht automatisch. Auch das Schließen einer Browserseite oder ein WLAN-Ausfall stoppt das Gerät nicht.

Weitere Hilfe findest du unter [Fehlersuche](#fehlersuche) und [Access-Point-Einrichtung](#access-point-einrichtung).

# Bedienung am Display

Die Oberfläche ist in drei Tabs unterteilt: `Trickler`, `Profil` und `Info`. Trickeln, Profilwahl, Profil-Tuning sowie die wichtigsten Netzwerk- und Waagenfunktionen lassen sich am Touchscreen bedienen. Für weitergehende Einstellungen und das vollständige Bearbeiten von Profilen dient die Weboberfläche oder der direkte Zugriff auf die Dateien. Eine SD-Karte ist bei der 8-MB-Firmware mit eingerichtetem LittleFS nicht zwingend nötig; WLAN wird für die Bedienung am Display nicht benötigt.

<img width="722" height="482" alt="Screen" src="https://github.com/user-attachments/assets/dddc2665-baae-4eac-bc0d-5eb91caa13f8" />

## Tab `Trickler`

* Oben siehst du das Zielgewicht, darunter das aktuell von der Waage gemessene Gewicht.
* Mit `+` und `-` veränderst du das Zielgewicht. Mit dem mittleren Button wechselst du die Schrittweite zwischen `0.001`, `0.010`, `0.100`, `1.000` und `10.000`. Das Zielgewicht ist auf maximal `500.000` begrenzt.
* Mit dem großen Start/Stop-Button startest oder stoppst du das Trickeln. Beim Starten wird der Button rot und zeigt `Stop`, im Ruhezustand ist er grün und zeigt `Start`. Mit `Stop` brichst du auch einen gerade laufenden Stepper-Wurf zeitnah ab; ein schnelles erneutes Starten setzt den alten Wurf nicht fort.
* Am unteren Rand siehst du in einer Statuszeile die Firmware-Version bzw. aktuelle Meldungen.

Während des laufenden Betriebs werden Änderungen des Zielgewichts auch am Display ignoriert. Drücke zuerst `Stop`. Die Tabs werden durch Antippen ihrer Überschriften gewechselt; Wischgesten zum Tabwechsel sind abgeschaltet.

Der Messwert wird am Display mit höchstens drei Nachkommastellen und, soweit erkannt, mit der Einheit der Waage angezeigt. `NaN...` bedeutet, dass gerade kein gültiger Messwert vorliegt; es ist kein Gewicht von null. Direkt nach `Stop` erscheint kurz `-.-`, bis die nächste Messung eintrifft. Im Ruhezustand wird etwa einmal pro Sekunde eine Messung angefordert; bei ausbleibenden Antworten kann es länger dauern.

Das geänderte Zielgewicht wird beim Starten des Trickelns in das aktive Profil geschrieben. Die Firmware merkt sich, ob das Zielgewicht seit dem Laden oder letzten Speichern geändert wurde; ohne Änderung wird die Profildatei beim Start nicht geöffnet oder neu geschrieben. Das Sonderprofil `calibrate` besitzt kein Zielgewicht; dort wird diese Änderung nicht in die Profildatei übernommen.

### Automatischer Dauerbetrieb

Nach dem Drücken von `Start` läuft der Trickler im Dauerbetrieb:

1. Vor der ersten Ladung wartet die Firmware nur dann auf eine leere Pulverpfanne (`0.000`), wenn `general.startAtZero` im Profil auf `true` steht. Bei `false` startet sie, sobald die Waage ein nicht-negatives Gewicht liefert.
2. Ist das Zielgewicht erreicht, ertönt der `done`-Beep und das Gewicht bleibt stehen.
3. Nimm die volle Pfanne ab. Sobald die Waage wieder auf `0.000` zurückgeht, startet automatisch die nächste Ladung – ein erneutes `Start` ist nicht nötig.
4. Zum Beenden drücke `Stop`.

### Farben des Gewichts

Das angezeigte Gewicht wechselt während des Trickelns die Farbe:

* **Weiß**: Trickelvorgang läuft.
* **Grün**: Zielgewicht erreicht, innerhalb der `tolerance`.
* **Gelb**: Zielgewicht erreicht, aber außerhalb der `tolerance`.
* **Rot**: Überwurf bzw. Alarm (`alarmThreshold` überschritten).

## Tab `Profil`

* Mit den Pfeil-Buttons (oben/unten) blätterst du durch die erkannten Profile; das gewählte Profil wird sofort geladen. Die Auswahl wird erst beim nächsten `Start` dauerhaft als `activeProfile` in `config.txt` gespeichert.
* Mit dem orangefarbenen Button (Zahnrad) öffnest du das Profil-Tuning. Dort kannst du `stepper.1.weightPerRev`, `general.trickleMapLimitFactor` sowie die `measurements`- und `stepper.steps`-Werte der `trickleMap` anpassen (siehe [Profil-Tuning](#profil-tuning)).
* Mit dem roten Button (Papierkorb) löschst du das ausgewählte Profil nach einer Bestätigung.
* Für das Profil `calibrate` werden Tuning- und Lösch-Button ausgeblendet.

**Während des Trickelns sind die Tabs `Profil` und `Info` gesperrt.** Die Firmware springt auf `Trickler` zurück, damit während eines laufenden Wurfs kein Profil gewechselt oder Dialog geöffnet wird. Um das Profil zu wechseln oder die Info-Seite zu nutzen, muss der Trickler zuerst gestoppt werden.

## Tab `Info`

* Im Log-Bereich siehst du Status- und Fehlermeldungen sowie die IP-Adresse bei aktivem WLAN.
* Beim Start wird hier außerdem angezeigt, ob der Trickler von der SD-Karte oder vom internen Flash gestartet ist.
* Mit dem WLAN-Button schaltest du WLAN ein oder aus (`wifi.enabled`). Bei eingeschaltetem WLAN ist der Button grün.
* Mit dem Waagen-Button (`Scale: …`) wechselst du zwischen den Abfrageprotokollen der Waage (`GG`, `SBI`, `KERN`, `KERN-ABT`, `KERN-ABS`, `AD`, `CUSTOM`, `STREAM`). Die Firmware speichert die Auswahl sofort in `scale.protocol`.
* Sind sowohl SD-Karte als auch internes LittleFS verfügbar, erscheinen zwei zusätzliche Buttons zum Synchronisieren von `config.txt` und `/profiles` (siehe [Konfiguration und Profile zwischen Flash und SD synchronisieren](#konfiguration-und-profile-zwischen-flash-und-sd-synchronisieren)).
* Im Access-Point-Modus wird hier der WLAN-QR-Code für die Einrichtung angezeigt.

Neben der Tabüberschrift `Info` zeigt ein SD-Karten- oder Flash-Symbol den aktiven Speicher an. Ein zusätzliches WLAN-Symbol erscheint bei bestehender Verbindung zum WLAN. Der grüne WLAN-Button allein zeigt nur, dass WLAN eingeschaltet ist, nicht ob die Verbindung bereits steht. Der Log-Bereich enthält die letzten acht Meldungen, lange Zeilen können abgeschnitten sein; er ist kein dauerhaft gespeichertes Protokoll.

<a id="zähler-für-fertige-trickles"></a>

## Zähler für fertige Trickles

Der Trickler kann die Anzahl fertig dosierter Ladungen mitzählen. Es gibt zwei unabhängige Zähler:

* Der **Sitzungszähler** (Profilfeld `general.sessionCounter`) zählt die fertigen Trickles seit dem letzten `Start`. Bei jeder fertigen Ladung innerhalb der Toleranz wird der Stand in der Statuszeile angezeigt (`Fertig … Anzahl: N`). Beim `Stop` wird er auf `0` zurückgesetzt.
* Der **Gesamtzähler** (`totalCounter.enable`/`totalCounter.count` in `config.txt`) zählt jede fertig dosierte Ladung dauerhaft über alle Sitzungen hinweg. Der Stand wird beim `Stop` in `config.txt` gespeichert. Wird die Stromversorgung vorher unterbrochen, gehen die seit dem letzten Speichern hinzugekommenen Zählungen verloren.

<a id="überwurf-alarm"></a>

## Überwurf-Alarm

Ist im Profil ein `alarmThreshold` größer `0` gesetzt und wird `targetWeight + alarmThreshold` erreicht oder überschritten, löst die Firmware den Überwurf-Alarm aus: Das Gewicht wird rot, es ertönen drei `done`-Beeps, der Trickler stoppt und es erscheint eine rote Warnmeldung. So fällt eine zu große Ladung sofort auf.

Auch die Alarmtöne folgen der Einstellung `beeper`: Nur `done` und `both` aktivieren diese Töne. Bei `off` oder `button` bleibt der Überwurf-Alarm akustisch stumm; die Warnmeldung erscheint weiterhin.

## Dialoge

Die Firmware blendet bei Bedarf Dialoge ein, z.B. die Abfrage `Profil aus Kalibrierung erstellen?` (Ja/Nein) nach einem Kalibrierlauf, Lösch- und Synchronisations-Bestätigungen sowie Fehler-, Warn- und Erfolgsmeldungen. Während ein Dialog offen ist, sind die übrigen Bedienelemente am Touchscreen gesperrt. Diese Dialoge werden nicht in die Browser-Fernsteuerung übertragen; bestätige sie am Gerät. Bei einer Meldung mit angekündigtem Neustart wird dieser durch `OK` ausgelöst.

# Pulverprofile

## Speicherort

Pulverprofile liegen im aktiven Dateisystem in diesem Ordner:

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

Die Profilliste im Display und über die Web-API enthält nur gültige Profile. Es werden bis zu 32 gültige `.txt`-Profile direkt aus `/profiles` angezeigt; Unterordner werden nicht durchsucht. Dateien mit `.cor` im Namen werden ignoriert. Verwende kurze, eindeutige Dateinamen mit höchstens 31 Zeichen vor `.txt`, vorzugsweise Kleinbuchstaben, Ziffern und Unterstriche. Die Firmware garantiert keine alphabetische Reihenfolge.

Ungültige Profile werden beim Scannen ignoriert und im Display gemeldet. Wenn das aktuell ausgewählte Profil beim Start oder beim Umschalten nicht geladen werden kann, benennt die Firmware eine vorhandene defekte Datei nach `.cor.txt` um, stellt auf `calibrate` um und lädt dieses Profil direkt. Ein dabei angeforderter Start wird abgebrochen; die Wiederherstellung startet keinen Lauf automatisch. Nur wenn auch das Wiederherstellen des Kalibrierprofils scheitert, fordert die Firmware einen Neustart an.

## Automatisches Profil aus Kalibrierlauf erstellen

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

6. Jetzt kannst du das neue Profil verwenden: Stelle ein Zielgewicht ein und drücke auf `Start`.

<img width="480" height="320" alt="Trickle_Main" src="https://github.com/user-attachments/assets/34cb2d95-5e9f-447c-9ac6-939140b4d0b9" />


**Infos:**

Die Firmware erstellt automatisch ein neues Profil in `/profiles` mit dem nächsten freien Namen von `powder_1.txt` bis `powder_32.txt`, wählt dieses Profil aus und speichert es in `config.txt`.

Die Firmware kann insgesamt bis zu 32 gültige Profile verwalten. Dabei zählt auch das Profil `calibrate` mit. Ist das Limit erreicht oder sind alle Namen von `powder_1.txt` bis `powder_32.txt` belegt, muss zuerst ein nicht mehr benötigtes Profil gelöscht oder umbenannt werden.

Ist dieses Limit von 32 Profilen bereits erreicht, prüft die Firmware das schon **vor** dem Kalibrierlauf: Beim Druck auf `Start` (mit gewähltem `calibrate`-Profil) erscheint dann die Meldung `Profillimit erreicht! Kalibrierung nicht möglich.`, und der Kalibrierlauf wird gar nicht erst gestartet. Lösche in diesem Fall zuerst ein nicht mehr benötigtes Profil.

Der automatisch erzeugte Profilaufbau basiert auf der gemessenen Pulvermenge pro Kalibrierlauf. `weightPerRev` wird aus `Kalibriergewicht / Kalibrierumdrehungen` berechnet. Mit dem mitgelieferten `calibrate`-Profil sind das `100` Umdrehungen, also `Kalibriergewicht / 100`. Die Firmware legt acht Feinwurf-Einträge an (`1.929`, `0.965`, `0.482`, `0.241`, `0.121`, `0.060`, `0.030`, `0.000` gn mit `2`, `2`, `5`, `5`, `10`, `10`, `15`, `20` Messungen) und verwendet dabei einen Sicherheitsfaktor von 65 % für die berechneten STEP-Pulse. Jeder Eintrag erhält mindestens `5` STEP-Pulse.

## Profil-Tuning

Das Profil kann direkt über die Steuerung angepasst werden.

### 1. Profil auswählen

Wechsle in den Tab `Profil` und wähle das Profil aus, das angepasst werden soll.

![Profil auswählen](https://github.com/user-attachments/assets/b4ba4f5b-84c8-492e-a2bc-4d74446d9ba5)

### 2. Tuning-Art wählen

Klicke auf das **Zahnrad-Symbol**. Der gemeinsame Tuning-Dialog startet mit **Gewicht / Umdr**. Mit den beiden Pfeilen neben dem Titel wechselst du vorwärts oder rückwärts durch die vier Tuning-Arten:

* **Gewicht / Umdr**: Passe `stepper.1.weightPerRev` an. Die Firmware berechnet dabei die nicht manuell geänderten Schrittzahlen für Stepper 1 neu. `stepper.2.weightPerRev` kannst du nur im Pulverprofil-Editor oder direkt in der Profildatei ändern.
* **Map-Limit-Faktor**: Passe `general.trickleMapLimitFactor` zwischen `0.010` und `1.000` an. Die Firmware berechnet dabei die nicht manuell geänderten Schrittzahlen der Stepper-1-Einträge anhand von `stepper.1.weightPerRev` neu.
* **Messwerte**: Passe die `measurements`-Werte der vorhandenen `trickleMap`-Einträge an, ohne die STEP-Pulse zu ändern.
* **Schritte**: Passe die STEP-Pulse (`stepper.steps`) der vorhandenen `trickleMap`-Einträge einzeln an.

Das Profil `calibrate` kann nicht getunt werden. Der Trickler muss für alle Tuning-Funktionen gestoppt sein.

### 3. Gewicht / Umdr anpassen

Wähle **Gewicht / Umdr**, um den Wert `stepper.1.weightPerRev` zu ändern. Die Schrittweite des Eingabefelds wechselt zwischen `0.001`, `0.010`, `0.100`, `1.000` und `10.000`; zulässig sind Werte von `0.001` bis `99.999`.

#### Hinweise

- Je niedriger der Wert, desto mehr Pulver wird pro Wurf dosiert.
- Übertrickelt der Trickler regelmäßig, erhöhe den Wert.
- Verringere den Wert schrittweise, bis der Trickler gerade nicht mehr übertrickelt.
- So erreichst du einen guten Kompromiss zwischen Geschwindigkeit und Genauigkeit.
- Beim Speichern bleiben alle vorhandenen `trickleMap`-Einträge erhalten. Wenn Gewicht/Umdr oder der Map-Limit-Faktor geändert wurde, berechnet die Firmware die Schrittzahlen für Stepper 1 anhand der vorhandenen `diffWeight`-Werte neu. Im selben Dialog manuell geänderte Schrittzahlen haben Vorrang; Messwertänderungen werden ebenfalls gespeichert.

![Gewicht / Umdr anpassen](https://github.com/user-attachments/assets/208370be-a8fe-4f64-bb73-0af4d2eab1e8)

### 4. Map-Limit-Faktor anpassen

Wähle **Map-Limit-Faktor**, um `general.trickleMapLimitFactor` zu ändern. Der mittlere Button wechselt die Schrittweite zwischen `0.001`, `0.010` und `0.100`; zulässig sind Werte von `0.010` bis `1.000`. Kleinere Werte erzeugen weniger STEP-Pulse pro Feinwurf, größere Werte mehr.

### 5. Messwerte anpassen

Wähle **Messwerte**, um die Stabilitätszählung pro `trickleMap`-Eintrag anzupassen. Der mittlere Button zeigt den `diffWeight`-Eintrag, der gerade bearbeitet wird; mit jedem Druck wechselst du zum nächsten Eintrag. Mit `+` und `-` wird die Anzahl der Messwerte für diesen Eintrag geändert. Zulässig sind Werte von `0` bis `99`.

Mehr Messwerte geben der Waage mehr Zeit zum Einschwingen und machen das Dosieren ruhiger, kosten aber Zeit. Weniger Messwerte beschleunigen den Ablauf, können bei unruhigen Waagen aber zu frühen Würfen führen.

Wenn du ausschließlich Messwerte änderst, werden nur diese Werte gespeichert; `diffWeight`, STEP-Pulse, Drehzahl und Stepper-Auswahl bleiben unverändert.

### 6. Schritte anpassen

Wähle **Schritte**, um die STEP-Pulse pro `trickleMap`-Eintrag anzupassen. Der mittlere Button zeigt wie beim Messwerte-Dialog den gerade bearbeiteten `diffWeight`-Eintrag und wechselt beim Drücken zum nächsten Eintrag. Mit `+` und `-` wird die Schrittzahl jeweils um eins geändert; der kleinste zulässige Wert ist `1`.

Wenn du ausschließlich Schritte änderst, werden nur die `stepper.steps`-Werte überschrieben. Änderst du im selben Dialog auch Gewicht/Umdr oder den Map-Limit-Faktor, haben deine manuell geänderten Schrittzahlen Vorrang vor der Neuberechnung. Änderungen an Messwerten werden ebenfalls übernommen. Eine spätere Änderung von Gewicht/Umdr oder Map-Limit-Faktor in einer neuen Tuning-Sitzung berechnet die Schrittzahlen erneut.

### 7. Einstellungen speichern

Beim Wechseln der Tuning-Art bleiben die Eingaben im geöffneten Dialog erhalten. **Speichern** übernimmt alle Änderungen aus allen vier Tuning-Arten gemeinsam und schließt den Dialog, unabhängig vom gerade angezeigten Modus. **Abbrechen** verwirft alle ungespeicherten Eingaben. Die Firmware schreibt das Profil einmalig in das aktive Dateisystem. Läuft der Trickler von SD, ist das die SD-Karte; läuft er ohne SD von LittleFS, ist es der interne Flash.

![Einstellungen speichern](https://github.com/user-attachments/assets/7a7d0735-f0f2-4740-8e7b-fcf7f4df550e)

### 8. Ergebnis testen

Wechsle zurück in den Tab `Trickler` und teste die neuen Einstellungen.

![Ergebnis testen](https://github.com/user-attachments/assets/b3d187a3-37ab-4deb-ba9e-6d8f17629922)

#### Diagnose-Anzeige beim Trickeln

Während des Trickelns zeigt die Statuszeile den gerade aktiven Tabelleneintrag an, z.B.:

```text
W0.482 ST167 RPM200 M5/3
```

Das bedeutet: `W` = `diffWeight` des aktiven Eintrags, `ST` = `steps`, `RPM` = `rpm`, `M` = benötigte / aktuelle Anzahl stabiler Messwerte. Diese Anzeige hilft beim Feinabstimmen der `trickleMap`.

<a id="profil-löschen"></a>

## Profil löschen

### 1. Profil auswählen

Wechsle in den Tab `Profil` und wähle das Profil aus, das gelöscht werden soll.

![Profil auswählen](https://github.com/user-attachments/assets/c185afa4-03b1-41d3-b1b1-f0c7c5ff0df2)

### 2. Profil löschen

Klicke auf das **Löschen-Symbol**, um das ausgewählte Profil zu entfernen. Vor dem Löschen wechselt die Firmware auf `calibrate` und aktualisiert anschließend die Profilliste. Das Profil `calibrate` selbst kann am Display weder angepasst noch gelöscht werden.

> **Hinweis:** Das Löschen eines Profils kann nicht rückgängig gemacht werden.

![Profil löschen](https://github.com/user-attachments/assets/305e6653-df4b-47c8-8035-8d8d7b95fbcd)


## Pulverprofil-Editor

Der Webserver enthält `profile_editor.html` als `Pulverprofil-Editor`. Damit können alle von der Firmware unterstützten Profilfelder (`general`, `stepper.1`/`2` und die `trickleMap`-Schritte sowie die Kalibrier-Profilform) bearbeitet, heruntergeladen und über den Webserver direkt in `/profiles` des aktiven Dateisystems gespeichert werden. Auch `general.startAtZero`, `general.sessionCounter` und `general.measurements` sind eigene Eingabefelder.

Wenn du über den Webserver arbeitest, listet das Auswahlfeld `Profil laden:` alle vorhandenen Profile aus `/profiles` auf. Beim Auswählen wird das Profil vom Gerät geladen und in den Editor übernommen; mit `Speichern` wird es unter dem `Profilname` wieder in `/profiles` zurückgeschrieben. Profile lassen sich weiterhin per Drag&Drop oder Einfügen in das Textfeld laden. Starte den Trickler nach dem Speichern neu, damit die neue Profilliste geladen wird.

Der Editor lässt sich auch offline direkt von der SD-Karte öffnen. Offline fehlen das Auswahlfeld `Profil laden:` und das Speichern auf das Gerät; das bearbeitete Profil wird stattdessen über `Herunterladen` als Datei gesichert (siehe [Weboberfläche offline nutzen](#weboberfläche-offline-nutzen)).

<a id="gramm--grain"></a>

## Gramm / Grain

Das Profil muss zur Einheit der Waage passen. Wenn die Waage in Grain ausgibt, müssen `targetWeight`, `diffWeight`, `tolerance`, `alarmThreshold`, `weightGap` und `weightPerRev` ebenfalls in Grain angegeben werden. Wenn die Waage in Gramm ausgibt, müssen diese Werte in Gramm angegeben werden.

Um Gramm in Grain umzurechnen:

```text
Grain = Gramm * 15.4323583529
```

Empfehlung: Lege für jedes Pulver getrennte Profile für Gramm und Grain an, z.B. `n140_g.txt` und `n140_gn.txt`.

## Aufbau eines Profils

Ein Trickelvorgang läuft in zwei Phasen:

1. **Grobwurf (automatisch, einmal pro Ladung):** Zu Beginn jeder neuen Ladung rechnet die Firmware aus dem `stepper`-Block (`weightPerRev`) aus, wie viele Schritte nötig sind, um bis auf `weightGap` an das Zielgewicht heranzukommen, und dosiert diese Menge in einem Zug. Nach dem Abnehmen der fertigen Pfanne und der Rückkehr auf `0.000` wird der Grobwurf für die nächste Ladung erneut berechnet.
2. **Feinwürfe (wiederholt):** Den Rest erledigt die `trickleMap`. Hier sind die Würfe nicht berechnet, sondern als feste `steps` hinterlegt. Je näher das Gewicht ans Ziel kommt, desto kleinere Einträge (kleineres `diffWeight`) werden gewählt.

Kurz: Der `stepper`-Block steuert nur den **berechneten** Grobwurf, die `trickleMap` die **fest eingestellten** Feinwürfe. Deshalb tauchen die Stepper an zwei Stellen auf.

Beispiel für das neue Profilformat:

```json
{
  "general": {
    "targetWeight": 40.000,
    "tolerance": 0.000,
    "alarmThreshold": 1.000,
    "weightGap": 1.000,
    "trickleMapLimitFactor": 0.650,
    "bulkStepper": 1,
    "startAtZero": false,
    "sessionCounter": false,
    "measurements": 5
  },
  "stepper": {
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
        "rpm": 200,
        "reverse": false
      }
    },
    {
      "diffWeight": 0.965,
      "measurements": 2,
      "stepper": {
        "id": 1,
        "steps": 335,
        "rpm": 200,
        "reverse": false
      }
    },
    {
      "diffWeight": 0.482,
      "measurements": 5,
      "stepper": {
        "id": 1,
        "steps": 167,
        "rpm": 200,
        "reverse": false
      }
    },
    {
      "diffWeight": 0.241,
      "measurements": 10,
      "stepper": {
        "id": 1,
        "steps": 84,
        "rpm": 200,
        "reverse": false
      }
    },
    {
      "diffWeight": 0.000,
      "measurements": 15,
      "stepper": {
        "id": 1,
        "steps": 5,
        "rpm": 200,
        "reverse": false
      }
    }
  ]
}
```

### `general`

* `targetWeight`: Wird beim Laden des Profils übernommen. Änderungen am Display werden beim Starten des Trickelns wieder in dieses Profil geschrieben.
* `tolerance`: erlaubte Abweichung zum Zielgewicht.
* `alarmThreshold`: Überwurf-Grenze. Wenn `targetWeight + alarmThreshold` erreicht oder überschritten wird, stoppt die Firmware, piept mehrfach und zeigt eine Warnung an. Bei `0` ist der Alarm deaktiviert.
* `weightGap`: Sicherheitsabstand zum Zielgewicht, den der berechnete Grobwurf frei lässt. Rechnerisch reicht er bis `Zielgewicht − weightGap`; die restliche Menge schließen danach die Feinwürfe. Dieser Rechenwert garantiert nicht, dass die tatsächlich geförderte Menge einen Überwurf ausschließt.
* `trickleMapLimitFactor`: Sicherheitsfaktor für die Berechnung der Stepper-1-Schritte in der `trickleMap` beim Erstellen und Tunen eines Profils. Der Standardwert `0.650` entspricht 65 %.
* `bulkStepper`: Stepper für den automatischen Grobwurf zu Beginn jeder Ladung. Erlaubt sind `1` und `2`.
* `startAtZero`: Wenn `true`, wartet die Firmware vor dem ersten Wurf auf exakt `0.000`. Wenn `false`, beginnt der erste Wurf bereits, sobald das Gewicht bei oder über `0.000` liegt – die Waage muss also nicht exakt genullt sein.
* `sessionCounter`: Wenn `true`, zeigt die Anzahl der fertigen Trickles seit dem letzten Stop an. Standard ist `false`.
* `measurements`: Wie viele aufeinanderfolgende Gewichtswerte die Firmware von der Waage abwartet, bevor sie den nächsten Wurf auslöst. Das gibt der Waage Zeit zum Einschwingen, damit nicht auf einen noch zappelnden Wert dosiert wird. Mehr Messungen = ruhiger/genauer, aber langsamer. Dieser Wert gilt für den Start eines Trickelvorgangs (bei neu aufgesetzter Pulverpfanne); die einzelnen `trickleMap`-Einträge haben ihren eigenen `measurements`-Wert.

Nur `targetWeight` ist in `general` erforderlich. Fehlende unterstützte Felder verwenden folgende Standardwerte: `tolerance: 0.000`, `alarmThreshold: 0.000`, `weightGap: 1.000`, `trickleMapLimitFactor: 0.650`, `bulkStepper: 1`, `startAtZero: false`, `sessionCounter: false` und `measurements: 20`. Unbekannte zusätzliche Felder machen das Profil weiterhin ungültig.

### `stepper`

* `1` und `2`: Einstellungen für Trickler 1 und Trickler 2 (die Objekt-Schlüssel sind die Stepper-Nummern).
* Mit `enabled` aktivierst du den jeweiligen Stepper für den automatischen Grobwurf.
* `weightPerRev`: Pulvermenge pro Umdrehung bei `rpm`.
* `rpm`: Motordrehzahl in U/min für den automatischen Grobwurf.

Nur Stepper `"1"` mit `weightPerRev` ist erforderlich. Bei Stepper 1 fehlen `enabled` und `rpm` standardmäßig als `true` und `200`. Stepper 2 darf vollständig fehlen und ist dann mit `enabled: false`, `weightPerRev: 10.000` und `rpm: 200` vorbelegt.

Der automatische Grobwurf läuft einmal zu Beginn jeder neuen Ladung. Die Firmware berechnet aus Zielgewicht, aktuellem Gewicht, `weightGap` und `weightPerRev` die benötigten STEP-Pulse. Es wird genau der in `general.bulkStepper` eingetragene Grob-Stepper verwendet. Wenn der gewählte Stepper nicht aktiviert ist oder `weightPerRev` den Wert `0` hat, wird der automatische Grobwurf übersprungen.

### `trickleMap`

`trickleMap` ist die eigentliche Trickel-Tabelle. Es sind maximal 16 Einträge möglich.

* `diffWeight`: Abstand zum Zielgewicht, ab dem dieser Eintrag verwendet wird.
* `measurements`: Anzahl abzuwartender Gewichtswerte vor diesem Wurf (gleiche Bedeutung wie `general.measurements`, aber pro Eintrag).
* Das Objekt `stepper` gruppiert die Stepper-Bewegung dieses Eintrags:
  * `stepper.id`: `1` oder `2`.
  * `stepper.steps`: Anzahl direkter STEP-Pulse für diesen Wurf. Die Firmware gibt diesen Wert unverändert an den Stepper aus.
  * `stepper.rpm`: Motordrehzahl in U/min. Sinnvolle Werte liegen meist zwischen 5 und 300.
  * `stepper.reverse`: Bei `true` läuft der Stepper nur für diesen Eintrag in die entgegengesetzte Richtung; bei `false` bleibt die normale Richtung erhalten.

Jeder Eintrag benötigt `diffWeight`, `measurements` und `stepper.steps`. Fehlende `stepper.id`, `stepper.rpm` und `stepper.reverse` verwenden `1`, `200` und `false`. Unbekannte zusätzliche Felder bleiben ungültig.

Damit ist beispielsweise auch dieses kompakte Profil gültig:

```json
{
  "general": {"targetWeight": 40.000},
  "stepper": {"1": {"weightPerRev": 0.527}},
  "trickleMap": [
    {
      "diffWeight": 0.000,
      "measurements": 20,
      "stepper": {"steps": 5}
    }
  ]
}
```

In normalen Pulverprofilen gilt `stepper.reverse` ausschließlich für den jeweiligen Feinwurf aus der `trickleMap`. Der automatisch berechnete Grobwurf läuft immer in normaler Richtung. Im Sonderprofil `calibrate` steuert das dortige `stepper.reverse` die Richtung des gesamten Kalibrierwurfs.

Die Firmware wählt den ersten Eintrag, dessen `diffWeight` noch zum Abstand zwischen aktuellem Gewicht und Zielgewicht passt. Je näher das Zielgewicht kommt, desto kleinere `diffWeight`-Einträge werden verwendet.

Beispiel (Zielgewicht `40.000`): Ein Eintrag kommt infrage, sobald der Restabstand `Zielgewicht − aktuelles Gewicht` mindestens so groß ist wie sein `diffWeight`. Verwendet wird der oberste passende Eintrag:

* Gewicht `37.000` (Rest `3.000`) → Eintrag `diffWeight 1.929` (großer Feinwurf)
* Gewicht `39.600` (Rest `0.400`) → Eintrag `diffWeight 0.241`
* Gewicht `39.970` (Rest `0.030`) → letzter Eintrag `diffWeight 0.000` (kleinster Wurf)

Deshalb gehören die Einträge **absteigend nach `diffWeight`** sortiert.

Hinweise:

* Bei zu wenigen STEP-Pulsen kann es sein, dass sich der Trickler nicht bewegt.
* Niedrigere Geschwindigkeiten fördern je nach Pulver oft mehr Pulver pro Umdrehung.
* Zu viele `measurements` machen das Trickeln langsam. Am Anfang reichen meist 2 Messungen, am Ende sind 10 bis 15 sinnvoll.

## Mehrere Trickler

Die Firmware kann zwei Trickler (Stepper) unabhängig ansteuern. Das ist nützlich, um z.B. einen schnellen Grob-Trickler für die große Menge weit vom Zielgewicht und einen feinen Trickler für die letzten Körner zu kombinieren.

**Hardware:**

* Stepper 1 (`stepper.1`) wird über den **X-Motoranschluss** des Treiberboards angesteuert.
* Stepper 2 (`stepper.2`) wird über den **Y-Motoranschluss** des Treiberboards angesteuert.

Beide Treiber teilen sich die gemeinsame Enable-Leitung; es ist immer nur ein Stepper gleichzeitig in Bewegung.

**So werden die beiden Trickler verteilt:**

1. **Beide Stepper aktivieren.** Setze im `stepper`-Block bei beiden, die du nutzen willst, `enabled: true` und trage jeweils `weightPerRev` (Pulvermenge pro Umdrehung) und `rpm` (Drehzahl in U/min) passend zum jeweiligen Trickler ein.
2. **Grobwurf-Stepper wählen.** `general.bulkStepper` bestimmt, welcher Stepper den automatischen Grobwurf am Anfang jeder Ladung ausführt (z.B. `2` als großer/schneller Trickler). Dieser Stepper muss `enabled: true` sein und ein gültiges `weightPerRev` größer `0` haben, sonst wird der Grobwurf übersprungen.
3. **Feinwurf pro Tabelleneintrag zuweisen.** Jeder Eintrag in `trickleMap` hat ein eigenes `stepper.id`-Feld (`1` oder `2`). So kannst du je nach Abstand zum Zielgewicht (`diffWeight`) einen anderen Trickler verwenden – typischerweise der große Trickler für die größeren `diffWeight`-Bereiche und der feine Trickler für die letzten Einträge nahe `0`.

Damit übernimmt im folgenden Beispiel Stepper `2` den Grobwurf und den ersten Feinwurf-Bereich (großer/schneller Trickler), während Stepper `1` ab `diffWeight 0.250` die feine Annäherung an das Zielgewicht erledigt:

```json
{
  "general": {
    "targetWeight": 40.000,
    "tolerance": 0.000,
    "alarmThreshold": 1.000,
    "weightGap": 1.000,
    "trickleMapLimitFactor": 0.650,
    "bulkStepper": 2,
    "startAtZero": false,
    "sessionCounter": false,
    "measurements": 5
  },
  "stepper": {
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
        "rpm": 200,
        "reverse": false
      }
    },
    {
      "diffWeight": 0.250,
      "measurements": 5,
      "stepper": {
        "id": 1,
        "steps": 80,
        "rpm": 200,
        "reverse": false
      }
    },
    {
      "diffWeight": 0.000,
      "measurements": 15,
      "stepper": {
        "id": 1,
        "steps": 5,
        "rpm": 200,
        "reverse": false
      }
    }
  ]
}
```

# SD-Karte

Falls die SD-Karte defekt ist oder beim Bearbeiten Fehler aufgetreten sind, kannst du die SD-Karten-Daten [hier](https://github.com/ripper121/RoboTrickler/releases) neu herunterladen.

1. Formatiere die SD-Karte mit FAT32.
2. Kopiere alle Dateien aus der SD-Files.zip in das Hauptverzeichnis der SD-Karte und starte den Trickler neu.

SD-Karten mit mehr als 32 GB:

Ist die SD-Karte zu groß, gibt es hier eine Anleitung, wie du sie trotzdem mit FAT32 formatieren kannst:
https://www.simon42.com/grosse-sd-karte-formatieren-fat32/

## Konfiguration

Die Konfiguration liegt als `/config.txt` im Hauptverzeichnis des aktiven Dateisystems. Eine beim Start erfolgreich eingebundene SD-Karte hat Vorrang; andernfalls nutzt die Firmware das interne LittleFS, sofern es eingebunden werden kann.

Die Datei enthält JSON, obwohl sie auf `.txt` endet. Verwende doppelte Anführungszeichen für Schlüssel und Texte, einen Punkt als Dezimaltrennzeichen und `true`/`false` ohne Anführungszeichen. Kommentare und ein zusätzliches Komma nach dem letzten Feld gehören nicht in die Datei. Sichere die vorhandene Datei vor dem Bearbeiten: Eine ungültige Konfiguration wird beim nächsten Start durch Standardwerte ersetzt, einschließlich der WLAN-Zugangsdaten.

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

Die Werte im Beispiel oben dienen nur zur Veranschaulichung. Alle gezeigten Objekte und Felder sind erforderlich; fehlende oder zusätzliche Felder führen dazu, dass die Firmware `config.txt` verwirft und vollständig durch ihre Standardkonfiguration ersetzt. Die Angaben in Klammern sind die Werte dieser neu erzeugten Standardkonfiguration.

* Mit `wifi.enabled` aktivierst du WLAN, Webserver und alle Netzwerkdienste. Bei `false` startet der Trickler ohne WLAN. Du kannst die Einstellung auch direkt am Display im Tab `Info` ändern. (Standard: `true`)
* `wifi.ssid`: WLAN-Name. Nur 2.4 GHz WLAN wird unterstützt. (Standard: leer)
* `wifi.psk`: WLAN-Passwort. Bei offenem WLAN leer lassen. (Standard: leer)
* `wifi.ipStatic`: optionale statische IP-Adresse. (Standard: leer = DHCP)
* `wifi.ipGateway`: Gateway-IP, nötig bei statischer IP. (Standard: leer)
* `wifi.ipSubnet`: Subnetzmaske, nötig bei statischer IP. (Standard: leer)
* `wifi.ipDns`: optionaler DNS-Server. Wenn leer, nutzt die Firmware `8.8.8.8`. (Standard: leer)
* Wenn du DHCP verwenden möchtest, lasse `wifi.ipStatic`, `wifi.ipGateway`, `wifi.ipSubnet` und `wifi.ipDns` leer.
* `scale.protocol`: unterstützte Werte sind `GG`, `SBI`, `KERN`, `KERN-ABT`, `KERN-ABS`, `AD`, `CUSTOM` und leer für kein aktives Anfragekommando (`STREAM`). (Standard: `GG`)
* `scale.customCode`: nur bei `CUSTOM`; Hex-Bytefolge wie `0x51 0x0D 0x0A`, mit der Messwerte von der Waage angefordert werden. (Standard: leer)
* `scale.baud`: Baudrate der Waage, meistens `9600`. (Standard: `9600`)
* `stepper.stepsPerRev`: Schritte pro voller Umdrehung der Schrittmotoren (gilt für Stepper1 und Stepper2). Trage bei einem 1,8°-Schrittmotor `200` ein und passe den Wert bei anderen Motoren entsprechend an. Verwende für Mikroschritt-Treiber den Gesamtwert: Vollschritte pro Umdrehung × Mikroschritt-Faktor. Beispiel: 1,8°-Motor (`200` Vollschritte) bei 1/8-Mikroschritt → `200 × 8 = 1600`. Der Wert beeinflusst die Schrittberechnung beim Trickeln und die Kalibrierung. (Standard: `200`)
* `activeProfile`: Profilname ohne `.txt`. Das Zielgewicht kommt aus `general.targetWeight` im gewählten Profil. (Standard: `calibrate`)
* `language`: Sprache der Oberfläche. Die Firmware normalisiert Werte wie `de-DE` zu `de`. Die Display-Texte werden aus `/lang/<sprache>.json` geladen und fallen auf `/lang/en.json` sowie danach auf eingebaute englische Texte zurück. Die Weboberfläche verwendet getrennte Dateien unter `/system/lang`. (Firmware-Standard: `en`; die mitgelieferte SD-/LittleFS-Konfiguration ist auf `de` gesetzt.)
* `beeper`: `done` Beep wenn Trickle fertig, `button` Beep bei Touch betätigung, `both` beides aktiv oder `off` Beeper aus. (Standard: `done`)
* Mit `totalCounter.enable` aktivierst du den dauerhaften Gesamtzähler für fertige Trickles. (Standard: `false`)
* `totalCounter.count`: gespeicherter Stand des dauerhaften Gesamtzählers. (Standard: `0`)
* Mit `firmwareUpdate.check` aktivierst du die automatische Prüfung auf neue Firmware. (Standard: `true`)

Wenn `config.txt` fehlt oder nicht gelesen werden kann, verwendet die Firmware eine Standard-Konfiguration. Falls nötig, wird außerdem das Profil `calibrate` aus der eingebauten Vorlage neu angelegt.

Auf der SD-Karte und im LittleFS-Image befindet sich `system/settings.html` (Menüpunkt `Einstellungen`). Damit kannst du die üblichen Einstellungen inklusive WLAN, Waage, `stepper.stepsPerRev`, Sprache, `totalCounter.enable`, `totalCounter.count` und `firmwareUpdate.check` erstellen. Du kannst die Seite über den Webserver oder offline direkt von der SD-Karte öffnen (siehe [Weboberfläche offline nutzen](#weboberfläche-offline-nutzen)).

Der STREAM-Modus entspricht einem leeren `scale.protocol` und kann in `settings.html` über die Protokoll-Auswahl **None** gesetzt werden.

Beim Öffnen von `Einstellungen` auf dem Gerät wird die vorhandene `/config.txt` geladen. `Speichern` ersetzt diese Datei; starte das Gerät anschließend über den Menüpunkt `Neustart` neu. Bis dahin arbeitet die Firmware mit den zuvor geladenen Einstellungen. Vermeide in dieser Zwischenzeit Änderungen am Display, die die Konfiguration erneut speichern und damit deine Dateibearbeitung überschreiben könnten. Offline erzeugt `Herunterladen` die Datei für den PC; kopiere sie anschließend als `config.txt` ins Hauptverzeichnis der SD-Karte.

<img width="372" height="1220" alt="image" src="https://github.com/user-attachments/assets/bfb98107-4ebd-4d78-a6bd-ee829973a59f" />


<a id="wann-werden-änderungen-übernommen"></a>

## Wann werden Änderungen übernommen?

| Änderung | Sofort wirksam? | Dauerhaft gespeichert / nächster Schritt |
| --- | --- | --- |
| WLAN-Schalter oder Waagen-Protokoll am Display | Ja | Sofort in `config.txt`. |
| Profil am Display oder in der Fernsteuerung wählen | Ja, das Profil wird geladen | Die Auswahl wird beim nächsten `Start` in `config.txt` gesichert. |
| Zielgewicht am Display ändern | Anzeige sofort | Beim nächsten `Start` im aktiven Profil; ein vorheriger Profilwechsel verwirft die ungespeicherte Eingabe. |
| Zielgewicht in der Browser-Fernsteuerung ändern | Ja | Sofort im aktiven Profil. |
| Tuning am Display speichern | Ja | Die Änderungen des gesamten Dialogs werden zusammen im Profil gespeichert. |
| `config.txt` über Einstellungen oder Dateibrowser speichern | Nein | Datei gespeichert; anschließend neu starten. |
| Vorhandene Profildatei im Webeditor ersetzen | Beim erneuten Laden | Das Profil neu wählen oder neu starten; auch `Start` lädt die Profildatei erneut. |
| Profildateien im Webeditor anlegen, umbenennen oder löschen | Profilliste zunächst unverändert | Neu starten, damit die Liste neu eingelesen wird. |
| Gesamtzähler | Zählt im Arbeitsspeicher | Beim `Stop` wird ein geänderter Stand in `config.txt` gespeichert. |

Alle Dateizugriffe betreffen den aktiven Speicher. Es gibt keinen automatischen Abgleich zwischen SD-Karte und internem Flash. Vor dem Bearbeiten von Dateien, Synchronisieren, Aktualisieren oder Neustarten zuerst `Stop` drücken. Speichere ausstehende Eingaben vor einem Wechsel des Profils oder der Speicherkarte.

Die Zielgewicht-Speicherung gilt für normale Profile; `calibrate` besitzt kein entsprechendes Feld. Kann ein geändertes Zielgewicht nicht gespeichert werden, wird `Start` abgebrochen und eine Fehlermeldung angezeigt.

## Firmware-Update

### Bevorzugte Variante: Update über die SD-Karte

1. Drücke `Stop` und sichere deine `config.txt`, den Ordner `/profiles` sowie weitere selbst geänderte Dateien von der SD-Karte.
2. Öffne den [neuesten RoboTrickler-Release](https://github.com/ripper121/RoboTrickler/releases/latest) und lade `SD-Files.zip` herunter.
3. Formatiere die SD-Karte mit FAT32. Dabei werden alle vorhandenen Dateien auf der Karte gelöscht.
4. Entpacke `SD-Files.zip` auf deinem Computer in einen neuen Ordner.
5. Kopiere den gesamten **Inhalt** des entpackten Archivs in das Hauptverzeichnis der SD-Karte. `firmware.bin`, `littlefs.bin`, `config.txt` und die Ordner `profiles` und `system` müssen direkt im Hauptverzeichnis liegen. Lege keinen zusätzlichen übergeordneten Ordner auf der SD-Karte an.
6. Übernimm bei Bedarf deine gesicherte Konfiguration und deine eigenen Profile. Prüfe sie vorher auf das aktuelle Format, da alte Dateiformate nicht automatisch migriert werden.
7. Setze die SD-Karte in den ausgeschalteten Trickler ein und schalte ihn ein. Die Firmware installiert zuerst `firmware.bin` und nach dem Neustart `littlefs.bin`. Lasse die Stromversorgung angeschlossen, bis beide Updates abgeschlossen sind und die normale Oberfläche wieder erscheint.

Der Fortschritt wird während des frühen Startvorgangs auf Englisch angezeigt, da die Konfiguration zu diesem Zeitpunkt noch nicht geladen ist. Nach jedem erfolgreichen Teil-Update löscht die Firmware die zugehörige Datei und startet neu. Deshalb kann der vollständige Vorgang mehrere Neustarts umfassen.

### Alternative Update-Varianten

* **Über die Weboberfläche:** Öffne bei aktivem WLAN `Firmware-Update`, wähle `firmware.bin` und lade die Datei hoch. Nach erfolgreichem Schreiben startet der Trickler neu. Auf derselben Seite kannst du zusätzlich `littlefs.bin` hochladen, um das interne Dateisystem zu aktualisieren.
* **Vollständige Neuinstallation über USB:** Folge der Anleitung unter [Flash via USB](#flash-via-usb).

`firmwareUpdate.check` steuert nur die automatische Versionsprüfung bei bestehender Netzwerkverbindung. Wird eine neuere Firmware gefunden, zeigt der Trickler einen Hinweis mit der neuen Versionsnummer und der Download-Adresse an. Das eigentliche Update wird nicht automatisch heruntergeladen oder installiert.

Die Versionsprüfung wird beim Starten der Webserver-Dienste in einem verbundenen WLAN ausgeführt. Dabei werden die MAC-Adresse des Geräts und seine Firmware-Version an den Update-Dienst übertragen. Mit `firmwareUpdate.check: false` lässt sich diese Abfrage abschalten; manuelle Updates bleiben möglich.

### Welche Dateien werden ersetzt?

| Update | Auswirkung auf gespeicherte Dateien |
| --- | --- |
| Firmware über Browser oder `/firmware.bin` auf SD | Aktualisiert die Anwendung. SD-Dateien und das interne LittleFS werden dadurch nicht aktualisiert. |
| `littlefs.bin` über Browser oder SD | Ersetzt das gesamte interne Dateisystem, einschließlich dort gespeicherter Konfiguration, Profile, Sprachen und Weboberfläche. Dateien auf SD bleiben erhalten. |
| Vollständige USB-Installation | Löscht den internen Flash und installiert Firmware, Partitionierung und LittleFS neu; siehe USB-Anleitung. |

Sichere vor einem LittleFS-Update die internen Dateien per Dateibrowser oder **Flash → SD**. Das gilt auch dann, wenn gerade von SD gestartet wurde: Das LittleFS-Update überschreibt trotzdem den internen Speicher. Die Synchronisation sichert nur Konfiguration und Profile; selbst bearbeitete Sprach- oder Webdateien separat herunterladen.

Wähle beim Firmware-Upload die Anwendungsdatei aus dem Release (`firmware.bin` bzw. `RoboTricklerUI.ino.bin`), nicht Bootloader, Partitionstabelle, `merged.bin` oder `littlefs.bin`. Ein Firmware-Update allein ändert das Partitionsschema nicht; für den Wechsel von einer älteren Installation ohne passende LittleFS-Partition ist die vollständige USB-Installation erforderlich. Aktualisiere Web- und Sprachdateien auf SD getrennt aus dem passenden SD-Paket. Sichere dabei deine vorhandene `config.txt` und deine Profile und prüfe sie auf das aktuelle Format; alte Dateiformate werden nicht automatisch migriert.

Beim SD-Update werden nur die Namen `/firmware.bin` und `/littlefs.bin` im Hauptverzeichnis geprüft, nicht `update.bin` und nicht Dateien im internen Flash. Sind beide vorhanden, kommt zuerst die Firmware. Schlägt dieses Firmware-Update fehl, wird das LittleFS-Update in diesem Startvorgang übersprungen. Fehlgeschlagene Update-Dateien bleiben auf der Karte; entferne oder ersetze sie vor einem erneuten Versuch. Kann eine erfolgreich verarbeitete Datei nicht gelöscht werden, erscheint eine Meldung und sie wird beim nächsten Start erneut angeboten.

# Internes Dateisystem (LittleFS)

Ab Firmware 2.13 (nur nach einem USB-Update) kann der Trickler auch ohne SD-Karte laufen. Dazu liegt im internen Flash ein LittleFS-Image mit der Standard-Konfiguration, den Profilen, den Sprachdateien und der Weboberfläche.

* Beim Start wählt die Firmware automatisch das Dateisystem: Lässt sich die SD-Karte einbinden, wird sie bevorzugt, sonst greift sie auf das interne LittleFS zurück.
* Das LittleFS-Image kann über die Weboberfläche unter `Firmware-Update` aktualisiert werden.

Die Auswahl gilt für **alle** Dateien. Fehlende Web-, Sprach- oder Konfigurationsdateien auf einer eingebundenen SD-Karte werden nicht einzeln aus LittleFS ergänzt. Eine leere, lesbare SD-Karte kann deshalb die interne Weboberfläche verdecken. Im Tab `Info` lässt sich der tatsächlich verwendete Speicher erkennen.

Wechsle oder entferne die SD-Karte bei ausgeschaltetem Gerät und starte danach neu. Ein Wechsel des aktiven Speichers im laufenden Betrieb ist nicht vorgesehen. Kann weder SD noch LittleFS eingebunden werden, erscheint `Filesystem mount failed`; ein leeres internes Dateisystem wird nicht automatisch formatiert oder mit sämtlichen Webdateien neu aufgebaut. Stelle in diesem Fall die SD-Dateien bzw. das passende LittleFS-Image wieder her. Bei älteren Geräten mit weniger als 8 MB Flash und ohne LittleFS-Unterstützung bleibt die SD-Karte erforderlich.

## Konfiguration und Profile zwischen Flash und SD synchronisieren

Sind sowohl SD-Karte als auch LittleFS verfügbar, zeigt das Display zwei Synchronisations-Funktionen an:

* Mit **Flash → SD** kopierst du `config.txt` und den Ordner `/profiles` vom internen Flash auf die SD-Karte.
* Mit **SD → Flash** kopierst du `config.txt` und den Ordner `/profiles` von der SD-Karte in den internen Flash.

Vor dem Kopieren erscheint eine Bestätigungsabfrage. Nach Abschluss zeigt die Firmware die Anzahl der kopierten Dateien an; bei **Flash → SD** startet der Trickler nach Bestätigung der Erfolgsmeldung mit `OK` neu und lädt die SD-Dateien. Bei **SD → Flash** bleibt SD aktiv, ein Neustart wird nicht angefordert. Ist eines der beiden Dateisysteme nicht verfügbar, werden die Funktionen ausgeblendet.

Vorhandene gleichnamige Dateien am Ziel werden ersetzt. Zusätzliche Profildateien, die nur am Ziel existieren, werden nicht gelöscht. Die Synchronisation kopiert alle Dateien direkt unter `/profiles`, auch Sicherungsdateien; Unterordner werden nicht rekursiv kopiert. Weboberfläche, Sprachdateien und sonstige Ordner werden nicht mitkopiert.

Es werden die bereits gespeicherten Dateien kopiert, keine noch offenen Eingaben. Jeder Kopiervorgang verwendet zunächst eine temporäre Datei, der gesamte Abgleich ist aber nicht atomar: Bei einem Fehler können bereits einzelne Zieldateien ersetzt worden sein. Prüfe in diesem Fall freien Speicher und Lesbarkeit beider Dateisysteme und wiederhole den Abgleich nach Beheben des Fehlers.

# WLAN und Webserver

Um den WLAN-Modus zu aktivieren, trage `ssid` und `psk` in `config.txt` ein und stelle sicher, dass `wifi.enabled` auf `true` steht.

**Nur 2.4 GHz WLAN wird unterstützt.**

Beim Start zeigt der Trickler `Mit WLAN verbinden:` an. Bei erfolgreicher Verbindung steht im Tab `Info` die IP-Adresse.

Die Weboberfläche verwendet HTTP auf Port 80 und hat keine eigene Anmeldung. Wenn du das Gerät im Netzwerk erreichen kannst, kannst du damit auch Dateien bearbeiten, das Gerät neu starten und Updates hochladen. WLAN-Zugangsdaten stehen in `config.txt`; behandle heruntergeladene Sicherungen entsprechend vertraulich.

## WLAN am Display steuern

Im Tab `Info` lässt sich WLAN über den WLAN-Button direkt am Touchscreen ein- und ausschalten. Der Button wird grün, wenn WLAN eingeschaltet ist. Die Einstellung wird in `wifi.enabled` gespeichert und sofort angewendet.

Beim Ausschalten werden auch Webserver und Einrichtungs-Access-Point beendet; die gespeicherten Zugangsdaten bleiben erhalten. Um WLAN wieder einzuschalten, benutze den Touchscreen. Die lokale Bedienung benötigt keine Internetverbindung.

Bei ungültigen statischen IP-Einstellungen meldet die Firmware einen Konfigurationsfehler und versucht DHCP. Prüfe dann die tatsächlich angezeigte IP-Adresse. Nach einer Verbindungsstörung versucht die Firmware im Ruhezustand erneut zu verbinden und kann in den Einrichtungsmodus wechseln. Während des laufenden Betriebs wird diese zusätzliche Wiederverbindungslogik ausgesetzt.

## Access-Point-Einrichtung

Kann sich der Trickler innerhalb von etwa 30 Sekunden nicht mit dem hinterlegten WLAN verbinden oder sind noch keine Zugangsdaten gespeichert, öffnet er einen eigenen Access Point für die Einrichtung:

* WLAN-Name: `Robo-Trickler-AP`
* Passwort: wird aus der Gerätekennung erzeugt und im Tab `Info` am Display angezeigt. Es bleibt bei demselben Gerät über Neustarts gleich.
* Direkte Adresse der Einrichtungsseite: `http://192.168.4.1/system/ap`

Im Tab `Info` wird zusätzlich ein QR-Code angezeigt, mit dem ein Smartphone direkt dem Access Point beitreten kann (er enthält WLAN-Name und Passwort). Ein Tippen auf den QR-Code blendet ihn aus, ein Tippen auf den Log-Bereich blendet ihn wieder ein.

### Robo-Trickler ohne vorhandenes WLAN bedienen

Du kannst den Robo-Trickler im Access-Point-Modus direkt bedienen. Eine Verbindung mit deinem Heimnetzwerk oder dem Internet ist dafür nicht erforderlich:

1. Verbinde dein Smartphone, Tablet oder deinen Computer mit `Robo-Trickler-AP`. Das Passwort wird im Tab `Info` angezeigt.
2. Öffne im Browser `http://192.168.4.1`.
3. Wähle den Menüpunkt `Trickler`. Dort kannst du ein Profil wählen, das Zielgewicht einstellen und den Trickelvorgang starten oder stoppen.

Bleibe während der Bedienung mit `Robo-Trickler-AP` verbunden. Die Meldung deines Geräts, dass dieses WLAN keinen Internetzugang bietet, ist in diesem Betriebsmodus normal. Wenn der Trickler bei jedem Start seinen Access Point öffnen soll, lasse `wifi.ssid` leer und setze `wifi.enabled` auf `true`.

### Robo-Trickler mit einem vorhandenen WLAN verbinden

Verbinde dich mit dem Access Point und öffne im Browser `http://192.168.4.1/system/ap`. Dort kannst du nach Netzwerken suchen, die Zugangsdaten eintragen und speichern. Nach dem Speichern startet der Trickler neu und verbindet sich mit dem gewählten WLAN.

Die meisten Smartphones öffnen die Einrichtungsseite nach dem Verbinden automatisch (Captive Portal). Geschieht das nicht, rufe `http://192.168.4.1/system/ap` von Hand auf. Nicht gefundene Seiten werden im Access-Point-Modus auf die Einrichtung umgeleitet; vorhandene Seiten und API-Endpunkte bleiben erreichbar. Meldet das Smartphone „Kein Internet“, bleibe für die Einrichtung mit diesem WLAN verbunden.

Die Einrichtungsseite speichert nur WLAN-Name und Passwort. Eine zuvor eingestellte statische IP, Gateway, Subnetzmaske und DNS bleiben unverändert. Um wieder DHCP zu verwenden, ändere diese Felder in `Einstellungen` oder `config.txt`. Der Menüpunkt `WLAN verbinden` öffnet dieselbe Seite auch bei bestehender WLAN-Verbindung; er schaltet nicht selbst den Access Point ein.

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


<a id="weboberfläche"></a>

## Weboberfläche

Die Startseite lädt `/system/index.html` aus dem aktiven Dateisystem. Mit eingelegter SD-Karte kommt die Weboberfläche von der SD-Karte, sonst aus dem internen LittleFS. Von dort erreichst du:

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


<a id="weboberfläche-offline-nutzen"></a>

## Weboberfläche offline nutzen

Die beiden Generator-Seiten lassen sich auch ohne Gerät und ohne WLAN nutzen: Öffne dazu `start.html` aus dem Hauptverzeichnis der SD-Karte direkt im Browser, z. B. per Doppelklick. Die Datei leitet dich automatisch zur lokalen Startseite unter `system/index.html` weiter. In diesem lokalen Modus zeigt die Startseite nur die beiden eigenständigen Werkzeuge an:

* Mit **Einstellungen** (`settings.html`) erzeugst du eine `config.txt` zum Herunterladen.
* Mit dem **Pulverprofil-Editor** (`profile_editor.html`) bearbeitest du ein Profil und lädst es als Datei herunter. Das Auswahlfeld `Profil laden:` und das Speichern direkt auf das Gerät stehen offline nicht zur Verfügung – lade ein Profil per Drag&Drop oder Einfügen in das Textfeld und sichere das Ergebnis über `Herunterladen`.

Die gerätegebundenen Funktionen (Trickler-Fernsteuerung, Dateibrowser, `WLAN verbinden`, Firmware-Update, Neustart) werden offline ausgeblendet. Die beiden Werkzeuge können auch direkt geöffnet werden (`system/settings.html` bzw. `system/profile_editor.html`).

Die Sprache der offline geöffneten Seiten richtet sich nach der Spracheinstellung des Browsers (mit Rückfall auf Englisch); eine `config.txt` wird dafür nicht benötigt.

Verwende für die Offline-Werkzeuge den vollständigen entpackten SD-Release-Ordner mit seinen Ressourcen. Lokale Übersetzungen benötigen die mitgelieferten Dateien `/system/lang/de.js` bzw. `en.js`; nur die HTML-Datei oder die komprimierten Dateien aus dem LittleFS-Paket reichen dafür nicht aus. Eine früher im Browser gespeicherte Sprachwahl kann Vorrang vor der Browser-Sprache haben.


<a id="fernsteuerung-über-den-webbrowser"></a>

## Fernsteuerung über den Webbrowser

Über den Menüpunkt `Trickler` kannst du die grundlegenden Bedienfunktionen vom Smartphone oder PC aus verwenden:

* Wähle ein Pulverprofil aus der Auswahlliste.
* Stelle das Zielgewicht mit `+` und `-` in `0.001`-Schritten ein oder schreibe es direkt in das Eingabefeld.
* Steuere das Trickeln mit `Start` und `Stop`.
* Lies das aktuell gemessene Gewicht ab.

Das Setzen von Zielgewicht und Profil wirkt sofort über dieselbe Firmware-Logik wie am Display (`/setTarget` bzw. `/setProfile`); ein Neustart ist dafür nicht nötig. Das Zielgewicht wird sofort in das aktive Profil geschrieben. Eine Profilwahl wird sofort geladen, aber erst beim Starten eines Wurfs in `config.txt` dauerhaft gespeichert. Während der Trickler läuft, weist die Firmware Änderungen an Zielgewicht und Profil mit HTTP-Status `409` zurück.

Die Seite fragt Gewicht und Laufstatus ungefähr einmal pro Sekunde ab. Sie zeigt keine vollständige Kopie des Touchscreens: Dialoge, Gewichtswarnfarben, Log und Zähler werden nicht mit übertragen. Ein ungültiger Messwert wird vom Gerät als `null` gemeldet; das Zahlenfeld im Browser kann dann leer erscheinen.

Das Schließen der Browserseite oder der Verlust der WLAN-Verbindung sendet keinen `Stop`-Befehl. Der Gerätezustand bleibt bestehen. Bei einer unterbrochenen Verbindung können zuletzt geladene Browserwerte stehen bleiben; prüfe den Zustand am Display und stoppe dort bei Bedarf. Lade die Browserseite nach einem Neustart des Geräts neu.

## Dateibrowser

Mit dem `Dateibrowser` kannst du Dateien und Ordner im aktiven Dateisystem über den Webbrowser ansehen, anlegen, hochladen, herunterladen, bearbeiten und löschen. Mit erfolgreich eingebundener SD-Karte bearbeitest du die SD-Karte, andernfalls das interne LittleFS. Für Änderungen an `config.txt` ist ein Neustart erforderlich; vorhandene Profile werden auch beim erneuten Auswählen oder bei `Start` geladen. Neue oder gelöschte Dateien erscheinen erst nach dem erneuten Einlesen der Profilliste, am einfachsten durch einen Neustart.

Ausnahmen sind die Web-API-Funktionen `/setTarget` und `/setProfile`: `/setTarget` schreibt das Zielgewicht sofort in das aktive Profil, `/setProfile` lädt das gewählte Profil sofort. Die dauerhafte `activeProfile`-Speicherung erfolgt wie am Display beim Starten eines Wurfs.

Ein Upload ersetzt eine vorhandene Datei desselben Namens. Das Löschen eines Ordners entfernt auch seinen Inhalt; es gibt keinen Papierkorb. Lade wichtige Dateien vor Änderungen herunter. Nach einem Upload die Datei erneut öffnen und Inhalt bzw. Größe prüfen: Die Upload-Bestätigung allein bestätigt nicht, dass JSON gültig ist oder alle Daten auf dem Speicher angekommen sind.

Falls nach einer Änderung weiterhin die alte Weboberfläche erscheint, prüfe, ob daneben eine gleichnamige `.gz`-Datei liegt, z.B. `index.html.gz`. Der Webserver bevorzugt diese komprimierte Fassung. Aktualisiere sie ebenfalls oder entferne die veraltete `.gz`-Fassung, wenn die neue unkomprimierte Datei vorhanden ist, und lade die Browserseite neu.

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/e3c420b0-bd87-42ac-ae9b-8e6ad72f0107)

## Web-API

Diese Endpunkte können im Browser oder aus einer eigenen Steuerung aufgerufen werden:

* `GET /getTricklerState`: aktuelles Gewicht und Laufstatus als JSON lesen, z. B. `{"weight":40.000,"running":true}`. Ohne gültigen Messwert ist `weight` gleich `null`. `running` beschreibt den aktiven Betrieb einschließlich Wartephasen, nicht die momentane Motorbewegung.
* `GET /getTarget`: Zielgewicht lesen.
* `GET /setTarget?targetWeight=WERT`: Zielgewicht setzen und im aktuellen Profil speichern. Erlaubt sind Werte größer `0` bis maximal `500.000`. Während eines laufenden Trickelvorgangs antwortet die Firmware mit `409`. Beispiel: `/setTarget?targetWeight=40`.
* `GET /getProfile`: aktuelles Profil lesen.
* `GET /getLanguage`: aktuell geladene Sprache lesen.
* `GET /getProfileList`: Liste der erkannten Profile als JSON-Array lesen, z.B. `["avg","calibrate"]`.
* `GET /setProfile?profileNumber=NUMMER`: Profil über die nullbasierte Nummer aus der Profilliste wählen und sofort laden. Während eines laufenden Trickelvorgangs antwortet die Firmware mit `409`; die Auswahl wird beim Starten eines Wurfs dauerhaft in `config.txt` gespeichert.
* `GET /system/start`: Trickeln starten. Wenn kein gültiges Profil geladen werden kann, antwortet die Firmware mit `409`.
* `GET /system/stop`: Trickeln stoppen.
* `GET /reboot`: Trickler neu starten.
* `GET /fwupdate`: Firmware-Update-Seite öffnen.
* `POST /update`: Firmware-Datei hochladen. Das Upload-Feld `firmware` schreibt die Firmware, das Upload-Feld `filesystem` schreibt bei aktivem LittleFS das interne Dateisystem-Image.
* `GET /list?dir=/PFAD`: Dateien im aktiven Dateisystem auflisten.
* `PUT /system/resources/edit?path=/PFAD`: Datei oder Ordner anlegen. Ein Punkt im Pfad führt zur Anlage einer Datei; ohne Punkt wird ein Ordner angelegt.
* `POST /system/resources/edit`: Datei hochladen. Bei Multipart-Uploads bestimmt der übergebene Dateiname den Zielpfad im aktiven Dateisystem, z.B. `filename=/profiles/avg.txt`.
* `DELETE /system/resources/edit?path=/PFAD`: Datei oder Ordner samt Inhalt löschen; das Hauptverzeichnis selbst ist geschützt.
* `GET /system/ap`: Access-Point-Einrichtungsseite öffnen.
* `GET /api/wifi/scan`: verfügbare WLAN-Netzwerke als JSON lesen. Während ein Scan noch läuft, antwortet die Firmware mit `202` und `{"scanning":true}`; danach kommt eine JSON-Liste mit SSID, RSSI, Kanal und Verschlüsselungsstatus.
* `POST /api/wifi/save`: Formularfelder `ssid` und `password` speichern und das Gerät neu starten (von der Einrichtungsseite verwendet).

Die Antwort `200` auf `/setTarget` oder `/setProfile` ist keine vollständige Eingabevalidierung: Nicht übernommene Werte können ebenfalls mit `200` beantwortet werden. Wenn du einen eigenen Client verwendest, lies den Wert mit `/getTarget` bzw. `/getProfile` zurück. Prüfe auch Datei-Uploads durch erneutes Herunterladen. Bei eigenen Multipart-Uploads muss der Dateiname den vollständigen Zielpfad enthalten; für `curl.exe` verhindert `-H "Expect:"` Probleme mit einer verzögerten Upload-Übertragung.

# Fehlersuche

| Anzeige / Problem | Bedeutung und nächster Schritt |
| --- | --- |
| `NaN...` statt Gewicht | Es liegt kein gültiger Messwert vor. Neben einer fehlenden Antwort kommen unlesbare Daten oder nicht ausreichend gleichbleibende Messwerte infrage. Prüfe Waage und Verbindung; interpretiere `NaN` nicht als Nullgewicht. |
| `Timeout!` / Hinweis auf RS232 | Innerhalb der Wartezeit kam keine Antwort. Vergleiche Stromversorgung der Waage, Kabel, Protokoll und Baudrate mit den vorhandenen Einstellungen. |
| Konfiguration beschädigt / Standardkonfiguration erzeugt | `config.txt` war nicht lesbar oder entsprach nicht dem aktuellen Format. Die Firmware hat versucht, Standardwerte zu speichern. Prüfe deine Sicherung und trage die Einstellungen einschließlich WLAN erneut ein. |
| Ungültige Profile / Profil fehlt in der Liste | Prüfe Dateiname, Speicherort direkt unter `/profiles`, `.txt`-Endung, JSON-Format und das Limit von 32 Profilen. Dateien mit `.cor` im Namen werden ausgeblendet. Starte das Gerät nach einer Korrektur neu. |
| Profil beschädigt / `calibrate` geladen | Das ausgewählte Profil konnte nicht geladen werden und wurde durch das Wiederherstellungsprofil ersetzt. Lies die Meldung und prüfe das gewünschte Profil, bevor du erneut startest. |
| Speichern oder Synchronisieren fehlgeschlagen | Prüfe den aktiven Speicher im Tab `Info`, den freien Platz und die Lesbarkeit. Sichere deine Dateien; bei einer fehlgeschlagenen Synchronisation können schon einzelne Dateien kopiert worden sein. |
| `Filesystem mount failed` | Weder SD noch internes LittleFS sind nutzbar. Stelle eine FAT32-SD mit vollständigen Dateien oder ein passendes LittleFS-Image wieder her. |
| `robo-trickler.local` nicht erreichbar | Öffne direkt die IP-Adresse aus `Info` mit `http://`. Verbinde dich im Einrichtungsmodus mit `Robo-Trickler-AP` und öffne `http://192.168.4.1/system/ap`. |
| `Home page not found`, `WiFi setup page not found` oder `Filesystem file not found` | Webdateien fehlen im aktiven Speicher. Verwende das vollständige passende SD-Paket bzw. LittleFS-Image; eine eingebundene SD-Karte hat auch bei fehlenden Dateien Vorrang vor Flash. |
| Browser zeigt veraltete Seite oder Werte | Prüfe die Verbindung und den tatsächlichen Zustand am Display und lade die Seite neu. Prüfe nach Änderungen an Webdateien auch vorhandene `.gz`-Kopien. |
| Web-Start antwortet mit `409` | Start wurde nicht übernommen. Die konkrete Ursache steht am Display, etwa ein nicht ladbares Profil, ein Speicherschreibfehler oder das Profillimit. |
| Firmware-Upload meldet `FAIL` | Der Web-Upload war nicht erfolgreich; es folgt kein automatischer Erfolgs-Neustart. Prüfe die Fehlermeldung am Display, den Dateityp und das passende Release. |

Die Log-Anzeige ist begrenzt und wird beim Neustart gelöscht. Notiere bei wiederkehrenden Fehlern den genauen Meldungstext, die Firmware-Version, den aktiven Speicher und den verwendeten Dateinamen vor dem Neustart.

Ein fehlender gültiger Messwert setzt den laufenden Betriebszustand nicht automatisch auf `Stop`. Die Firmware verarbeitet erst wieder einen neuen gültigen Messwert; danach kann der Ablauf weitergehen. Drücke bei einer Störung deshalb ausdrücklich `Stop`, bevor du die Verbindung oder das Gerät prüfst.

# Waagen

<a id="unterstützte-protokolle"></a>

## Unterstützte Protokolle

Die Firmware fragt die Waage je nach `scale.protocol` so ab:

* `GG`: G&G Kommando `ESC p CR LF`.
* `AD`: A&D Kommando `SI CR LF`.
* `KERN`: Kern Kommando `w`.
* `KERN-ABT`: Kern ABT Kommando `D05 CR LF`.
* `KERN-ABS`: Kern ABS Kommando `D01 CR LF`.
* `SBI`: Sartorius Balance Interface Kommando `P CR LF`.
* Bei `CUSTOM` sendet die Firmware `scale.customCode` als Hex-Bytefolge, z. B. `0x51 0x0D 0x0A`.
* Bei einem leeren oder unbekannten Wert (`STREAM`) wartet die Firmware nur auf eingehende Daten.

Das Protokoll kann auch direkt am Display umgeschaltet werden. Über den Protokoll-Button im Tab `Info` werden die Protokolle nacheinander durchgeschaltet (`GG`, `SBI`, `KERN`, `KERN-ABT`, `KERN-ABS`, `AD`, `CUSTOM`, `STREAM`) und in `scale.protocol` gespeichert.

Die serielle Schnittstelle arbeitet mit der in `scale.baud` eingestellten Baudrate und fest mit `8N1` (8 Datenbits, keine Parität, 1 Stoppbit). Eine Antwort sollte mit LF abgeschlossen sein und einen eindeutig erkennbaren Gewichtswert enthalten. Die Firmware akzeptiert auch negative Werte, bevorzugt bei mehreren Textbestandteilen einen einzelnen Dezimalwert und verwirft mehrdeutige Zeilen, statt Ziffern verschiedener Felder zusammenzusetzen. Erkennt sie `g`, `gn` oder `gr` in der Antwort, zeigt sie die entsprechende Einheit an; eine Umrechnung zwischen Gramm und Grain erfolgt nicht.

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

Wenn du höchste Genauigkeit möchtest, kannst du diese Serie mit einem Messbereich von 0,1 mg verwenden.

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

1. Schalte die Waage aus.
2. Halte die Taste **SAMPLE** gedrückt und schalte die Waage ein.
3. Navigiere zum Menü **init**.
4. Wähle **ALL** aus.
5. Bestätige mit **PRINT**.
6. Warte die Initialisierung ab.
7. Starte die Waage neu.

---

#### 2. Einheit dauerhaft auf GN (Grain) einstellen

Die Waage startet mit der ersten in der Unit-Liste gespeicherten Einheit. Speicherst du ausschließlich **GN**, startet die Waage künftig immer im Modus **GN (Grain)**.

##### GN als einzige Einheit speichern

```text
SAMPLE (gedrückt halten)
→ Unit
→ PRINT

Wechsle mit SAMPLE bis GN.
Wähle GN mit RE-ZERO aus.
(Stabilitätssymbol erscheint.)

Wähle keine weiteren Einheiten aus.

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

1. Verlasse das Menü mit **CAL**.
2. Die Waage speichert die Einstellungen automatisch.
3. Schalte die Waage aus und wieder ein.
4. Prüfe die Funktion.

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

> **Achtung:** Die vollständige USB-Installation löscht den gesamten internen Flash einschließlich der LittleFS-Konfiguration und der dort gespeicherten Profile. Sichere wichtige Dateien vorher über den Dateibrowser oder synchronisiere sie auf die SD-Karte. Dateien auf der SD-Karte werden durch den USB-Flashvorgang nicht gelöscht.

Schließe die Steuerung mit dem USB-Kabel an den PC an und verbinde die Steuerung mit dem Netzteil.

Für Windows:

Entpacke `usb-flash.zip` und öffne `flash.bat`, dann drücke Enter.

Für Mac und Linux:

Entpacke `usb-flash.zip`, installiere Espressifs `esptool` und führe die folgenden Befehle im entpackten Ordner aus. Die angegebenen Adressen gehören zum mitgelieferten 8-MB-Partitionsschema der Firmware 2.14; verwende sie nicht mit einem anderen Partitionsschema.

Download: [esptool](https://github.com/espressif/esptool)

```text
esptool --chip esp32 erase-flash

esptool --chip esp32 --baud 921600 --before default-reset --after hard-reset write-flash -z --flash-mode dio --flash-freq 80m --flash-size 8MB 0x1000 ./RoboTricklerUI.ino.bootloader.bin 0x8000 ./RoboTricklerUI.ino.partitions.bin 0xe000 ./boot_app0.bin 0x10000 ./RoboTricklerUI.ino.bin 0x670000 ./littlefs.bin
```

# Hardware Aufbau

## Trickler

Fülle den Standfuß mit etwas Schwerem, z.B. Bleischrot oder Gips.

Prüfe vor der Montage der Lager, ob das Alurohr in die Lager passt. Sollte es nicht passen, klemme das Alurohr in einen Akkuschrauber oder eine Bohrmaschine und schleife mit Sandpapier den Durchmesser des Rohres herunter. Prüfe regelmäßig, bis alles passt.

Gib vor der Montage der Lager einen Tropfen Sekundenkleber an die Stelle, an der die Lager sitzen werden. Gib auch einen Tropfen an die Stelle der Motor-Kupplung, in die das Alurohr kommt. Verklebe außerdem Trickler-Körper und Standfuß miteinander.

![image](https://github.com/user-attachments/assets/4fef64f3-8915-4898-8454-f5276cb16af0)

![image](https://github.com/user-attachments/assets/2425d182-7688-447e-9712-952fe3b3e999)

![image](https://github.com/user-attachments/assets/07e61690-ed64-4d9b-a55a-3a6080bbe288)


## Anschluss an die Waage

![image](https://github.com/ripper121/RoboTrickler/assets/11836272/402d324c-5ebe-4109-9119-ce106cf7c60f)

## RS232 Konverter

**Verpolung beschädigt den RS232 Stecker.**

Die Firmware startet die Waagen-Schnittstelle mit `RX = SCL` und `TX = SDA`. Bei TTL-RS232-Konvertern werden UART-Leitungen normalerweise gekreuzt: Der TX-Ausgang des Konverters geht an den RX-Eingang der Steuerung, der RX-Eingang des Konverters an den TX-Ausgang der Steuerung.

Korrekte Verkabelung zur TTL-Seite des RS232-Konverters:

```text
| Steuerung | Firmware-Rolle | RS232-Konverter |
|-----------|----------------|-----------------|
| 3V3       | Versorgung     | VCC             |
| GND       | Masse          | GND             |
| SCL       | RX             | TXD             |
| SDA       | TX             | RXD             |
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

<a id="gehäuse-aufbau"></a>

## Gehäuse Aufbau

Du kannst das Gehäuse verschrauben oder mit Sekundenkleber verkleben.

![Case_2](https://github.com/user-attachments/assets/c7cf622f-5486-4696-9e57-ce4c6e4f7e5f)
![Case_1](https://github.com/user-attachments/assets/d891660f-0360-420c-9ee1-87ce05dc7131)

3D-gedruckte Version:

![IMG_20260322_115602](https://github.com/user-attachments/assets/7cf09f91-00d7-4b26-9afe-46f5fa91de24)
![IMG_20260322_120513](https://github.com/user-attachments/assets/94311673-e704-42bd-b006-51ae1d500c21)
![IMG_20260322_120442](https://github.com/user-attachments/assets/e71a2e28-6594-45c5-934f-b6418307989c)
![IMG_20260322_120419](https://github.com/user-attachments/assets/0d701aa7-aaa2-47bb-86e0-ff581bd2e924)

## Alurohr Passung

Falls das Alurohr nicht in die Lager passt, musst du es etwas nachbearbeiten.

Spanne das Rohr dazu in eine Bohrmaschine ein und bearbeite es mit Schleifpapier.

![20240710_212914](https://github.com/user-attachments/assets/5b6ddeb4-6d4e-43b2-a49b-fde387b6b977)

![20240710_212923](https://github.com/user-attachments/assets/2413a764-6fdf-4191-a5a3-37876a219bc0)
