# Changelog

## 2.15 (pre-release)

### Highlights
- Added compact normal-profile support: optional settings now use documented defaults while unknown fields are still rejected.
- Added a per-profile `general.trickleMapLimitFactor` setting for controlling Stepper 1 trickle-map calculations.
- Expanded the German user manual with setup, update, storage, Wi-Fi/AP, web interface, API, troubleshooting, scale, and assembly guidance.
- Firmware updates can now be installed in the browser at [ripper121.github.io/RoboTrickler](https://ripper121.github.io/RoboTrickler/), with English and German guidance for preparing the SD card.

### Firmware-Update

Es gibt zwei Möglichkeiten, die Firmware über USB zu installieren:

1. **Webbrowser:** Öffne den [Web-Updater](https://ripper121.github.io/RoboTrickler/), wähle eine Version aus und folge den Anweisungen auf dem Bildschirm.
2. **USB-Paket:** Lade `USB-Flash.zip` von der [Release-Seite](https://github.com/ripper121/RoboTrickler/releases/latest) herunter und entpacke es. Starte unter Windows `flash.bat`. Unter macOS oder Linux findest du die Anleitung zum Flashen im [Wiki](https://github.com/ripper121/RoboTrickler/wiki).

Formatiere nach der gewählten Methode die SD-Karte als FAT32. Entpacke die zur Firmware passende `SD-Files.zip` und kopiere ihren Inhalt direkt in das Hauptverzeichnis der SD-Karte.

Bei Fehlern oder Fragen: [Kontakt](https://shop.strenuous.dev/contact).

---

### Firmware update

Choose one of two ways to install the firmware over USB:

1. **Web browser:** Open the [browser updater](https://ripper121.github.io/RoboTrickler/), select a release, and follow the on-screen instructions.
2. **USB package:** Download `USB-Flash.zip` from the [releases page](https://github.com/ripper121/RoboTrickler/releases/latest) and extract it. On Windows, run `flash.bat`; on macOS or Linux, follow the [wiki's USB flashing instructions](https://github.com/ripper121/RoboTrickler/wiki).

After either method, format the SD card as FAT32. Extract the matching `SD-Files.zip` and copy its contents to the root of the SD card.

For errors or questions, [contact us](https://shop.strenuous.dev/contact).

### Added
- Added on-device tuning of the trickle-map limit factor from `0.01` to `1.00`; changing it recalculates Stepper 1 map entries unless their step counts were explicitly edited.
- Added trickle-map limit-factor support to generated profiles, bundled sample profiles, the profile editor, translations, and profile-creation tooling.
- Added a bundled `min` sample profile.
- Added a `trickle` field to `/getTricklerState`: `0` for idle, `1` for running, and `2` for finished while waiting for the charge to be removed.
- Added `tools/tools.py`, a unified command-line and interactive launcher for release, firmware, filesystem, LittleFS, manual, and profile tools.
- Added a test button in the on-device profile step-tuning dialog to run the selected stepper entry before saving it.
- Added a GitHub Pages workflow that refreshes release images for the browser installer.

### Changed
- Normal profiles now require only `general.targetWeight`, Stepper 1 `weightPerRev`, and each trickle-map entry's `diffWeight`, `measurements`, and `stepper.steps`; omitted supported fields receive defaults.
- The web profile editor now accepts the same compact profile schema as the firmware and fills omitted values with defaults.
- Automatically created profile names now use `powder_1` through `powder_32`, matching the device's 32-profile limit.
- UI and firmware-web translation loading now filters JSON during parsing so only the required language section is retained in heap.
- Production release builds now clean the selected build directory, generated filesystem trees, staging data, and stale USB-package binaries before rebuilding.
- Firmware and release tools now build the 8 MB layout with a LittleFS image; the legacy 4 MB build option and legacy SD-file package were removed.
- The browser installer checks release image consistency, erases and flashes the complete 8 MB layout, then directs users to the matching `SD-Files.zip`.
- Web controls now use POST for state-changing actions and validate request arguments, upload paths, and image sizes. File uploads and filesystem sync use temporary files and recover interrupted replacements.
- Firmware version checks now bound the response size and reject malformed versions; firmware and LittleFS updates check image sizes against their partitions.
- Trickler state, weight, and dialog flags are synchronized across tasks to prevent duplicate completion handling and conflicting web or file operations.
- Updated trickler CAD models and the RS232 board design files.
- Updated generated SD/LittleFS trees, release binaries, web assets, translations, profiles, and the PDF manual.
