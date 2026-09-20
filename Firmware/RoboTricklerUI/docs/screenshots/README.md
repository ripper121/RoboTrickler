# Manual screenshots

All generated manual screenshots are stored together in this folder. PNG files
are ready to copy/upload; `touch_*.png` are 480 × 320 LVGL captures and
`web_*.png` are browser captures of the SD-hosted pages.

## Copying to the GitHub wiki

Copy the PNG files into `docs/screenshots/` in your cloned wiki repository and
copy `manual.md` to the desired wiki page. Keep that directory structure so the
manual's relative image links continue to point to these files. Commit and push
the wiki changes when ready. This task does not publish anything to the wiki.

For images hosted elsewhere on GitHub, replace the corresponding
`docs/screenshots/<filename>.png` links in the wiki text with their image URLs.
The filenames identify the matching screen. See GitHub's
[wiki image instructions](https://docs.github.com/en/communities/documenting-your-project-with-wikis/editing-wiki-content#adding-images).

## Regenerating

From the firmware folder:

```powershell
python tools/ui_verify_layout.py --manual
python tools/manual_web_screenshots.py
```

The native renderer requires the installed LVGL library, Visual Studio 2022
CMake/MSVC and Pillow. It compiles the actual `ui.c`, `ui_Screen1.c`, `ui_touch.c`
and dialog factories extracted from `ui_dialogs.ino`/`display_helpers.ino`,
using the firmware fonts, configuration and German translations. The manual
scenarios are in `tools/manual_screenshots.inc`; hardware callbacks are inert.

The web capture requires Selenium and Microsoft Edge. A temporary server on
loopback serves the repository's `SD-Files` pages and compressed resources,
with synthetic GET responses for configuration, profiles, weight and Wi-Fi.
It accepts no write requests and never contacts a Robo-Trickler. Edge runs
headlessly, and both browser and server close when capture ends.

Profile values, weights, logs and network details are illustrative, not device
measurements. The web home footer still says 2.14 in the current HTML source;
the screenshot preserves that text. Firmware itself reports 2.15.

To run the existing native layout checks without overwriting manual images:

```powershell
python tools/ui_verify_layout.py
```
