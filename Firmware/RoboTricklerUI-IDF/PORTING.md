# RoboTricklerUI ESP-IDF Port

This project is being ported from the Arduino firmware in `../RoboTricklerUI` to a native ESP-IDF firmware.

## Current native scope

- Centralized ESP32-WROOM-32E board/pin mapping in `main/board.h`
- Native SDSPI/FATFS SD-card mount at `/sdcard`
- Native cJSON-based `config.txt` and profile loading
- Native UART1 scale polling for the existing GG, AD, KERN, KERN-ABT, SBI, and CUSTOM request modes
- Native GPIO bit-banged shift-register stepper and beeper control
- Main trickler loop with stable-weight detection, bulk move support, and profile step execution
- Native ST7796 `esp_lcd` panel driver with LVGL 9.5 and XPT2046 touch integration
- Generated SquareLine UI sources copied into the IDF app and wired to the trickler callbacks
- Wi-Fi station setup with DHCP/static-IP support, static DNS, reconnect polling, mDNS `robo-trickler.local`, and HTTP routes
- SD-backed static file serving plus `/list` and `/system/resources/edit` create/delete/upload handlers
- SD `/update.bin` OTA and browser upload OTA using `esp_ota_ops`

Remaining validation is hardware-side: LCD color order/rotation, touch calibration, stepper bit mapping, scale protocol timing, Wi-Fi behavior on the target network, and both OTA flows on-device.

## ESP-IDF sources checked

- `C:\esp\v6.0\esp-idf\examples\storage\sd_card\sdspi`
  - Reused pattern: `sdmmc_host_t`, `sdspi_device_config_t`, `esp_vfs_fat_sdspi_mount`
- `C:\esp\v6.0\esp-idf\examples\peripherals\uart\uart_echo`
  - Reused pattern: `uart_driver_install`, `uart_param_config`, `uart_set_pin`, `uart_read_bytes`
- `C:\esp\v6.0\esp-idf\examples\wifi\getting_started\station`
  - Reused pattern: native station initialization and connection flow
- `C:\esp\v6.0\esp-idf\examples\protocols\static_ip`
  - Reused pattern: static IP and DNS configuration through `esp_netif`
- `C:\esp\v6.0\esp-idf\examples\protocols\http_server\file_serving`
  - Reused pattern: chunked file serving, upload, and delete handlers
- `C:\esp\v6.0\esp-idf\examples\protocols\http_server\restful_server`
  - Checked for HTTP route structure and static-file response patterns
- `C:\esp\v6.0\esp-idf\examples\system\ota\native_ota_example`
  - Reused pattern: passive partition selection, `esp_ota_begin/write/end`, and boot partition switch
- `C:\esp\v6.0\esp-idf\examples\peripherals\lcd\spi_lcd_touch`
  - Reused pattern: SPI LCD bus, LVGL tick/task, draw buffers, and touch read callback
- `C:\esp\v6.0\esp-idf\components`
  - Reused IDF driver components for GPIO, UART, SDSPI/SPI, FATFS, SDMMC, `esp_lcd`, `esp_http_server`, `esp_wifi`, `esp_netif`, `app_update`, and `esp_timer`
  - Checked `components/esp_lcd`; no in-tree ST7796 panel driver exists in IDF v6.0, so a small project-local panel driver was added.

## Build

Build command used:

```powershell
. C:\esp\v6.0\esp-idf\export.ps1; idf.py build
```

Result: build passes and generates `build\RoboTricklerUI-IDF.bin`.
