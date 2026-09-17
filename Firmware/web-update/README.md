# Robo-Trickler web updater

A small Web Serial installer for complete RoboTrickler release images. The build
requires `firmware.bin`, `littlefs.bin`, `USB-Flash.zip`, and `SD-Files.zip`,
verifies that the files inside the USB package match the standalone release
binaries and merged 8 MB image, and synchronizes the five required images into
the static site. There are no local firmware file inputs or firmware download
links in the interface.

## Run locally

Use Google Chrome or Microsoft Edge, then run:

```sh
npm install
npm run dev
```

The start command first synchronizes release files, then prints a localhost URL
to open. Web Serial requires a secure context; GitHub Pages uses HTTPS and
browsers treat `http://localhost` as secure.

## Build and test

```sh
npm test
npm run build
```

The production site is written to `dist/`. The repository workflow refreshes
the release list and deploys this directory to GitHub Pages when app changes are
pushed, a release is published, or its daily schedule runs. In the repository
settings, set **Pages → Build and deployment → Source** to **GitHub Actions**
once before the first deployment.

The production build keeps esptool's chip modules in one JavaScript bundle so a
browser-cached page cannot reference a chunk removed by a newer Pages deployment.

## Flash layout

The installer erases the whole chip and then restores:

- Bootloader at `0x1000`
- Partition table at `0x8000`
- OTA boot data at `0xe000`
- Application at `0x10000`
- LittleFS at `0x670000`

This is a complete factory-style installation for the RoboTrickler 8 MB layout.
All settings and files currently stored in flash are erased. After flashing, the
page resets the ESP32 into the installed firmware and closes the serial port.

After installation, a localized overlay links directly to the selected
release's `SD-Files.zip` and tells the user to format the card as FAT32 with the
default allocation-unit size, then extract the archive into the SD-card root.

Release binaries are not linked in the UI, but a browser must download bytes in
order to flash them. They therefore cannot be cryptographically hidden from a
user who inspects network traffic or the public GitHub release.
