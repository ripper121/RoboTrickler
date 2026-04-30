
1. **Check for an existing ESP-IDF solution before writing new code**  
   Review the example code of these paths first:
   - `C:\Users\ripper121\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.8\libraries`
   - `C:\Users\ripper121\Documents\Arduino\libraries`

      Reuse existing solutions when possible. If no suitable solution exists, implement new code in the style and structure of the examples.
## Standard workflow

Use this order for all relevant implementation work:

1. Check examples
2. Define the smallest working scope
3. Keep board mapping and hardware assumptions centralized

## Compile check

This workspace uses Arduino IDE 1.8.x with Espressif ESP32 core `3.3.8`.

Preferred command-line verify from this folder:

```powershell
$build = Join-Path $env:TEMP 'rtui-build-check'
New-Item -ItemType Directory -Force -Path $build | Out-Null
& 'C:\Program Files (x86)\Arduino\arduino-builder.exe' `
  -compile `
  -logger=machine `
  -hardware 'C:\Program Files (x86)\Arduino\hardware' `
  -hardware 'C:\Users\ripper121\AppData\Local\Arduino15\packages' `
  -tools 'C:\Program Files (x86)\Arduino\tools-builder' `
  -tools 'C:\Program Files (x86)\Arduino\hardware\tools\avr' `
  -tools 'C:\Users\ripper121\AppData\Local\Arduino15\packages' `
  -built-in-libraries 'C:\Program Files (x86)\Arduino\libraries' `
  -libraries 'C:\Users\ripper121\Documents\Arduino\libraries' `
  -fqbn 'esp32:esp32:esp32:JTAGAdapter=default,PSRAM=disabled,PartitionScheme=min_spiffs,CPUFreq=240,FlashMode=dio,FlashFreq=80,FlashSize=4M,UploadSpeed=921600,LoopCore=1,EventsCore=0,DebugLevel=none,EraseFlash=all,ZigbeeMode=default' `
  -ide-version=10819 `
  -build-path $build `
  -warnings=none `
  'C:\Users\ripper121\Documents\GitHub\RoboTrickler\Firmware\RoboTricklerUI\RoboTricklerUI.ino'
```

The same board settings are mirrored in `.vscode/arduino.json`.
