
1. **Check for an existing ESP-IDF solution before writing new code**  
   Review the example code of these paths first:
   - `C:\Users\ripper121\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.10\libraries` look yourself for the Espressif ESP32 core VERSION
   - `C:\Users\ripper121\Documents\Arduino\libraries`

      Reuse existing solutions when possible. If no suitable solution exists, implement new code in the style and structure of the examples.

## Standard workflow

Use this order for all relevant implementation work:

1. Check examples
2. Define the smallest working scope
3. Keep board mapping and hardware assumptions centralized
4. Try always to save as much Heap as possible, because the Webserver, Wifi and LVGL are heap hungry.
5. Try to reuse UI Code to save Heap.
6. Dont turn on WIDGETS in lv_config and dont increase LV_MEM_SIZE

### Re-running the audit

This is the project-specific instance of the reusable runbook in `NAMING_AUDIT.md`
(a portable C/Arduino template — copy that file into other projects).

**Conventions enforced here:** functions / variables / `Config` fields are `camelCase`;
types (`Config`, `TricklerState`, `FilesystemSyncDirection`) are `PascalCase`; macros,
enum constants, and compile-time constants are `UPPER_SNAKE`; mutable globals stay
`camelCase` (`webServerActive`, not `WEB_SERVER_ACTIVE`); source files are `snake_case`
with a domain prefix (`ui_*`, `wifi_*`, `web_*`, `filesystem_*`, `trickler_*`). LVGL
event callbacks use `verbNoun_event_cb` (e.g. `toggleWifi_event_cb`).

**Do NOT rename** (externally required): `setup()` / `loop()` and library APIs; LVGL /
TFT callbacks (`my_disp_flush`, `my_print`, `my_touchpad_read`, `disp_task_init`,
`lvgl_disp_task`); SquareLine-generated `ui*.c/.h` symbols and `*_event_cb` names wired
in `ui_Screen1.c`; `lv_conf.h` / `build_opt.h`; the sketch `.ino` (must match the folder);
and contract strings — HTTP routes in `web_server.ino`, the `config.txt` / profile JSON
keys in `sd_storage.ino`, and the `web.firmware.*` lang keys.

**Deny-list re-check** (run from this folder after any rename; should return nothing but
deliberate hits — the audit doc itself and intentional legacy vocabulary):

```bash
# replace OLD1|OLD2|... with the old side of the current rename map
rg -n --glob '!build/**' --glob '!data/**' --glob '!SD-Files-Gz/**' --glob '!SD-Files-LittleFS/**' 'OLD1|OLD2'
# mutable globals wrongly UPPER_SNAKE:
rg -n '^(bool|int|long|float|double|char|byte|uint\w+) +[A-Z][A-Z0-9_]+ *(=|;|\[)' *.ino
# snake_case Config fields (should be camelCase):
rg -n 'config\.[a-z]+_[a-z]'
# snake_case custom event callbacks (should be verbNoun_event_cb):
rg -n '\b[a-z]+_[a-z_]+_event_cb\b' *.ino
```

Then validate contracts/data and rebuild: confirm SD JSON parses
(`python -c "import json,glob; [json.load(open(f,encoding='utf-8')) for f in glob.glob('SD-Files/**/*.json',recursive=True)]"`)
and compile with `python tools/compile_upload.py --cli --error --compile-only`.

## SD file rules

- `firmwareUpdate.url` is for internal firmware use only. Keep the update URL in firmware code (`DEFAULT_FW_UPDATE_URL`) and do not add it to SD files, generated `config.txt`, the config generator UI, or SD-facing docs. `firmwareUpdate.check` may remain user-configurable.

## SD file testing upload

When the Robo-Trickler is reachable on the network, SD-hosted files can be updated for testing through the web editor API instead of mounting the SD card.

Upload one changed file from this folder with `curl.exe -F`. The multipart filename must be the target SD path. Add `-H "Expect:"`, especially for binary files, so the ESP32 web editor receives the multipart body reliably instead of creating a zero-byte file after a stalled `100-continue` handshake:

```powershell
curl.exe -sS -H "Expect:" -F "file=@SD-Files/system/resources/edit/index.html;filename=/system/resources/edit/index.html" "http://robo-trickler.local/system/resources/edit"
```

Verify by fetching the file back and comparing hashes:

```powershell
curl.exe -sS "http://robo-trickler.local/system/resources/edit/index.html" -o "$env:TEMP\rtui-editor-index-upload-check.html"
$local = Get-FileHash -Algorithm SHA256 ".\SD-Files\system\resources\edit\index.html"
$remote = Get-FileHash -Algorithm SHA256 "$env:TEMP\rtui-editor-index-upload-check.html"
$local.Hash -eq $remote.Hash
```

Use the same pattern for other SD files: local path under `SD-Files\...`, target filename as the SD-root absolute path `/...`, and fetch from the matching device URL.

## Compile check

This workspace uses Arduino IDE 1.8.x with Espressif ESP32 core `3.3.10`.

Its preferred to use "compile_upload.py --cli --error --compile-only".

The same board settings are mirrored in `.vscode/arduino.json`.



Legacy Arduino IDE 1.8.19

Boards-Manager esp32 - Espressif Systems version 3.3.10

Tools
Board: "ESP32 Dev Module"
Upload Speed: "921600"
CPU Frequency: "240MHz (WiFi/BT)"
Flash Frequency: "80MHz"
Flash Mode: "DIO"
Flash Size: "8MB (64Mb)"
Partition Scheme: "8M with spiffs (3MB APP/1.5MB SPIFFS)"
Core Debug Level: "None"
PSRAM: "Disabled"
Arduino Runs On: "Core 1"
Events Run On: "Core 0"
