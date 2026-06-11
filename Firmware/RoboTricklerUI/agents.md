
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

## SD file rules

- `fw_update.url` is for internal firmware use only. Keep the update URL in firmware code (`DEFAULT_FW_UPDATE_URL`) and do not add it to SD files, generated `config.txt`, the config generator UI, or SD-facing docs. `fw_update.check` may remain user-configurable.

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
Flash Size: "4MB (32Mb)"
Partition Scheme: "Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"
Core Debug Level: "None"
PSRAM: "Disabled"
Arduino Runs On: "Core 1"
Events Run On: "Core 0"
