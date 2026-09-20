# Changelog

## 2.15 (pre-release)

Changes since the 2.14 release, commit `645a78a67d75b8f2af6d5e8414a474fec889adfe`.

### Highlights

- Larger, consistently spaced touchscreen controls, release-to-activate behavior, and cancellation when sliding off a button.
- One shared profile-tuning dialog for weight per revolution, map-limit factor, measurement counts, and step counts, including a motor test before saving.
- Compact profiles with defaults for optional fields, plus a configurable `general.trickleMapLimitFactor`.
- Complete USB installation from the browser using the 8 MB firmware/LittleFS layout.
- More robust file replacement, update validation, motor-error handling, and coordination between touchscreen, web, and trickler tasks.
- Updated German manual with current API methods and screenshots in `docs/screenshots/`.

### Added

- On-device tuning of `general.trickleMapLimitFactor` from `0.010` to `1.000`, with a default of `0.650`. Changing the factor or weight per revolution recalculates Stepper 1 map entries; step counts explicitly edited in the same tuning session take precedence.
- Factor support in generated profiles, the web profile editor, translations, and profile-creation tools; bundled compact `min` profile.
- Step-tuning motor test using the selected entry's motor, speed and direction with the unsaved step count. The test runs outside the LVGL callback, disables repeat activation while active, and is cancelled when the dialog closes.
- `trickle` in `GET /getTricklerState`: `0` for inactive operation, `1` for running/waiting, and `2` for finished while waiting for the charge to be removed. `running` remains true in states `1` and `2`.
- Browser USB installer at [ripper121.github.io/RoboTrickler](https://ripper121.github.io/RoboTrickler/) with release selection and English/German SD-card instructions. Its build verifies consistency of release images; a GitHub Pages workflow refreshes the available releases.
- `tools/tools.py` as a common launcher for firmware, release, filesystem, manual, and profile tools.
- Native LVGL layout verification and reproducible manual screenshots, including a local web preview with illustrative device responses.

### Changed

- Trickler and tuning controls use 44-pixel touch targets and 8-pixel spacing. Profile navigation buttons are 88 pixels high, with a larger central profile/action row.
- The tuning-mode selector is at the top, Save/Close stay at the bottom, and Step tuning places the `1`/`10`/`100` increment selector beside the blue motor-test triangle directly below the step count. The map-entry selector is on the next row.
- Save uses a green disk icon and Cancel a red cross. Confirmation dialogs use action symbols for OK, create, copy and delete; long text scrolls above fixed buttons. Compact Flash/SD icons open confirmations that spell out the copy direction.
- Touchscreen target-weight controls are visibly disabled during operation. The web interface disables profile selection while running; target/profile API mutations are rejected during a run.
- Normal profiles require `general.targetWeight`, Stepper 1 `weightPerRev`, and each map entry's `diffWeight`, `measurements`, and `stepper.steps`. Supported omitted fields receive defaults; unknown fields remain invalid. The web profile editor accepts this compact schema too.
- Automatically created profile filenames range from `powder_1.txt` to `powder_32.txt`; the total limit is still 32 valid profiles, including `calibrate`.
- **API migration:** `/setTarget`, `/setProfile`, `/system/start`, `/system/stop`, and `/reboot` now require **POST**. Read endpoints remain GET. Invalid target/profile arguments return `400`, and conflicting operations return `409`. Browser mutations validate the request origin; non-browser clients may omit `Origin`.
- File-editor paths and upload sizes are checked. Uploads and filesystem synchronization stage replacements through temporary files and backups. Firmware and LittleFS updates check image sizes against their partitions; firmware version checks bound the response size and reject malformed versions.
- Translation parsing retains only the required JSON section. Shared dialogs, directly rendered button symbols and fewer style allocations reduce memory use within the unchanged 24 KiB LVGL pool.
- Release tooling now builds only the 8 MB layout with LittleFS; the legacy 4 MB build option and legacy SD package are removed. Production builds clean stale build/staging output before rebuilding.
- Updated trickler CAD models and RS232 board design files.

### Fixed

- Invalid `config.txt` is preserved as `config.cor.txt` before defaults are written; an older recovery copy is replaced. Corrupt profile recovery likewise replaces the matching `.cor.txt` consistently on SD and LittleFS.
- Interrupted configuration/profile replacements can recover their backups when filesystems are mounted. Failed file writes no longer report a successful upload.
- Synchronized runtime state, weight snapshots and dialog flags reduce conflicting operations and duplicate completion handling across tasks.
- Failed or partial I2S writes latch a motor-output fault and reject further moves until restart. Initialization now checks task, mutex and I2S setup failures; motor timing validation rejects unsupported step-rate combinations.
- Step-tuning tests interrupt the idle scale-poll wait so a missing scale response does not delay the requested test.

### Firmware-Update

**Bevorzugte Variante: Web-Update**

Öffne den [Web-Updater](https://ripper121.github.io/RoboTrickler/) in Chrome oder Edge, verbinde die Steuerung über USB, wähle eine Version aus und folge den Anweisungen auf dem Bildschirm.

**Alternativen:**

1. **USB-Paket:** Lade `USB-Flash.zip` von der [Release-Seite](https://github.com/ripper121/RoboTrickler/releases/latest) herunter und entpacke es. Starte unter Windows `flash.bat`. Die Anleitung für macOS/Linux findest du im [Wiki](https://github.com/ripper121/RoboTrickler/wiki/Anleitung-Firmware-2.15#flash-via-usb).
2. **SD-Karte:** Entpacke die passende `SD-Files.zip` auf eine FAT32-SD-Karte. Beim Start installiert die Steuerung `firmware.bin` und `littlefs.bin` aus dem Hauptverzeichnis.
3. **Geräte-Weboberfläche über WLAN:** Öffne auf der Webseite des Tricklers `Firmware-Update` und lade `firmware.bin` hoch. `littlefs.bin` kannst du auf derselben Seite separat hochladen. Ein reines Firmware-Update aktualisiert weder LittleFS noch die Webdateien auf SD.

Die vollständige USB-Installation benötigt 8 MB Flash und löscht den internen Flash einschließlich Konfiguration und Profilen. Auch ein LittleFS-Update ersetzt die internen Dateien; sichere sie vorher.

Für den SD-Betrieb nach der gewählten Methode: Sichere die Karte, formatiere sie als FAT32, entpacke die zur Firmware passende `SD-Files.zip` und kopiere ihren Inhalt direkt ins Hauptverzeichnis. Verwende die passenden Webdateien, da ältere Seiten noch GET für Schreibaktionen verwenden können.

Bei Fehlern oder Fragen: [Kontakt](https://shop.strenuous.dev/contact).

### Firmware update

**Preferred method: browser updater**

Open the [browser updater](https://ripper121.github.io/RoboTrickler/) in Chrome or Edge, connect the controller over USB, select a release, and follow the on-screen instructions.

**Alternatives:**

1. **USB package:** Download and extract `USB-Flash.zip` from the [releases page](https://github.com/ripper121/RoboTrickler/releases/latest). Run `flash.bat` on Windows; see the [wiki](https://github.com/ripper121/RoboTrickler/wiki/Anleitung-Firmware-2.15#flash-via-usb) for macOS/Linux.
2. **SD card:** Extract the matching `SD-Files.zip` onto a FAT32 SD card. At startup, the controller installs `firmware.bin` and `littlefs.bin` from the card's root.
3. **Device web interface over Wi-Fi:** Open `Firmware Update` on the device's website and upload `firmware.bin`. You can upload `littlefs.bin` separately on the same page. An application-only update does not update LittleFS or SD web files.

The full USB installation requires 8 MB flash and erases internal settings and profiles. A LittleFS update also replaces internal files; back them up first.

For SD operation after your chosen method, back up the card, format it as FAT32, then extract the matching `SD-Files.zip` and copy its contents to the card's root. Use the matching web files: older pages may still use GET for write actions.

For errors or questions, [contact us](https://shop.strenuous.dev/contact).
