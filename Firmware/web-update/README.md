# Robo-Trickler web updater

A small Web Serial installer for the `firmware.bin` and `littlefs.bin` assets in
RoboTrickler GitHub releases. The build synchronizes compatible releases into
the static site because GitHub's release-asset host does not allow direct
cross-origin browser downloads. There are no local firmware file inputs or
download links in the interface.

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

## Flash layout

The updater intentionally does not erase the whole chip. It updates:

- Application (`firmware.bin`) at `0x10000`
- LittleFS (`littlefs.bin`, matched case-insensitively) at `0x670000`

This preserves the existing bootloader and 8 MB partition table. It is an
updater for an already provisioned Robo-Trickler, not a blank-device factory
installer. Writing LittleFS replaces files and settings stored in that
partition.

Release binaries are not linked in the UI, but a browser must download bytes in
order to flash them. They therefore cannot be cryptographically hidden from a
user who inspects network traffic or the public GitHub release.
