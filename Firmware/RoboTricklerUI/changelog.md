# Changelog

## 2.15 (pre-release)

### Highlights
- Added compact normal-profile support: optional settings now use documented defaults while unknown fields are still rejected.
- Added a per-profile `general.trickleMapLimitFactor` setting for controlling Stepper 1 trickle-map calculations.
- Expanded the German user manual with setup, update, storage, Wi-Fi/AP, web interface, API, troubleshooting, scale, and assembly guidance.

### Added
- Added on-device tuning of the trickle-map limit factor from `0.01` to `1.00`; changing it recalculates Stepper 1 map entries unless their step counts were explicitly edited.
- Added trickle-map limit-factor support to generated profiles, bundled sample profiles, the profile editor, translations, and profile-creation tooling.
- Added a bundled `min` sample profile.
- Added a `trickle` field to `/getTricklerState`: `0` for idle, `1` for running, and `2` for finished while waiting for the charge to be removed.
- Added `tools/tools.py`, a unified command-line and interactive launcher for release, firmware, filesystem, LittleFS, manual, and profile tools.

### Changed
- Normal profiles now require only `general.targetWeight`, Stepper 1 `weightPerRev`, and each trickle-map entry's `diffWeight`, `measurements`, and `stepper.steps`; omitted supported fields receive defaults.
- The web profile editor now accepts the same compact profile schema as the firmware and fills omitted values with defaults.
- Automatically created profile names now use `powder_1` through `powder_32`, matching the device's 32-profile limit.
- UI and firmware-web translation loading now filters JSON during parsing so only the required language section is retained in heap.
- Production release builds now clean the selected build directory, generated filesystem trees, staging data, and stale USB-package binaries before rebuilding.
- Updated generated SD/LittleFS trees, release binaries, web assets, translations, profiles, and the PDF manual.