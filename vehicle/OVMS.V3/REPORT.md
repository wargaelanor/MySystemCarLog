# REPORT: Порт OVMS v3.3.006 на ESP-IDF 5.4 для LILYGO T-2CAN

## 1. Архитектура

Проект `D:\OpenCode_projeck\OVMS\vehicle\OVMS.V3` — порт Open Vehicle Monitoring System v3.3.006
на **ESP-IDF 5.4** под плату **LILYGO T-2CAN** (ESP32-S3-WROOM-1U-N16R8, слот SIMCOM A7670E-FASE, 2x CAN).

Проведённые работы (компиляция всех исходников + линковка прошивки):

- `main/CMakeLists.txt`: в `SRCS` добавлены отсутствовавшие `background_shell.cpp`, `file_writer.cpp`.
- `components/vehicle_smarteq/CMakeLists.txt`: добавлен отсутствовавший `src/eq_handle.cpp`
  (именно там определена `OvmsVehicleSmartEQ::HandleOBDpolling()`); первая «undefined reference» — файл не собирался.
- `components/poller/src/vehicle_poller.cpp`: определены три базовых виртуальных метода
  `OvmsPoller::VehicleSignal::{IncomingPollReply,IncomingPollError,IncomingPollTxCallback}` (пустые)
  — не хватало vtable базового класса.
- `components/wolfssl`: создан `wolfssl/wolfcrypt/src/port/Espressif/esp32_random.c`
  с реализацией `wc_GenerateSeed()` через `esp_fill_random()` (в IDF 5.4 нет ветки для ESP32-S3);
  файл добавлен в `CMakeLists.txt`.
- `main/ovms_module.cpp`:
  - `ets_install_putc1()`/`ets_install_uart_printf()` заменены на `esp_rom_install_channel_putc(1, ...)`/
    `esp_rom_install_uart_printf()` (`esp_rom_sys.h`);
  - включены `CONFIG_HEAP_TASK_TRACKING=y`, `CONFIG_FREERTOS_USE_TRACE_FACILITY=y`
    (иначе код heap-интегрити выпадает под `#ifdef NOGO` → undefined reference).
- `sdkconfig`: `CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y` (вместо BLE 5.0) — иначе в Bluedroid
  на ESP32-S3 вырезаны `esp_ble_gap_config_adv_data()`/`esp_ble_gap_start_advertising()`
  (§`BLE_42_FEATURE_SUPPORT`).
- Ранее выполненные исправления (из журнала сборок build15–build23):
  `ovms_partitions.cpp` — замена `spi_flash_*` на `esp_flash_*`; `gsmpppos.cpp` — сигнатура
  `pppos_output_cb_fn` + `CONFIG_LWIP_PPP_PAP_SUPPORT=y`; отключение устаревшего `esp32can`
  (CAN1 = штатный TWAI); `VSPI_HOST → SPI3_HOST` в `ovms_peripherals.cpp`, `swcan.cpp`, `spi.cpp`;
  `console_telnet` — `MongooseLock()` напрямую; `mongoose.c` — `cs_md5_final(unsigned char*, ...)`;
  `ovms_metrics.cpp` — каст `bool→dbcNumber`; `ovms_netmanager.cpp` — цикл по `netiflist`
  (dangling-pointer); `ovms_webserver/CMakeLists.txt` — `write_mtime_header` на чистом CMake
  `file(TIMESTAMP "%s" UTC)`; глобальные `-Wno-error=...` флаги.

## 2. Сборка и прошивка

Окружение (Windows PowerShell):
- `IDF_PATH = D:\esp-idf` (v5.4), `IDF_TOOLS_PATH = C:\Users\ServiceCenter\.espressif`,
  `IDF_PYTHON_ENV_PATH = ...\python_env\idf5.4_py3.13_env`.
- PATH: `\python_env\idf5.4_py3.13_env\Scripts`, `\python_env\idf5.4_py3.13_env`,
  `\tools\cmake\3.30.2\bin`, `\tools\ninja\1.12.1`,
  `\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin`, `\tools\winflexbison\2.5.25`.

Команда:
```powershell
python D:\esp-idf\tools\idf.py build
```

Прошивка (T-2CAN, USB-Serial-JTAG):
```
python -m esptool --chip esp32s3 -b 460800 --before default_reset --after hard_reset write_flash `
  --flash_mode dio --flash_size 16MB --flash_freq 80m `
  0x0 build\bootloader\bootloader.bin `
  0x8000 build\partition_table\partition-table.bin `
  0xd000 build\ota_data_initial.bin `
  0x10000 build\ovms3.bin
```
или из `build`: `python -m esptool --chip esp32s3 ... write_flash "@flash_args"`.

Проверка порта: `idf.py -p <PORT> flash monitor`.

## 3. Логи успешной компиляции/линковки

Последняя сборка (build25, `ovms_build25.log`): `EXIT=0`, готовые артефакты в `build\`:

| Файл | Размер |
|---|---|
| `ovms3.bin` | 4630.7 KB |
| `bootloader\bootloader.bin` | 21.8 KB |
| `partition_table\partition-table.bin`, `ota_data_initial.bin` | по настройке |

`Project build complete.` Остаток предупреждений — 2 шт., некритичные:
`ovms_module.cpp:218,227` `[-Wdeprecated-copy]` (implicitly-declared operator= для `Name`).

## 4. Известные ограничения

- Предупреждения `[-Wdeprecated-copy]` в `ovms_module.cpp` (безвредны).
- Есть некритичные format-warning'ы (`%u`/`%d` vs `size_t`, `-Wno-error=format*` глобально).
- BLE сконфигурирован в режиме **Bluedroid / BLE 4.2** (вместо 5.0) из-за используемого
  OVMS-кода (`esp_ble_gap_config_adv_data`/`start_advertising`); получения Logo-скоростей
  BLE 5.0 нет.
- Legacy-компонент `esp32can` отключён — применяется штатный TWAI-контроллер (CAN1)
  и MCP2515 (CAN2) с `SPI3_HOST`.
- Функции heap-диагностики модуля активны только при включённых
  `CONFIG_HEAP_TASK_TRACKING` + `CONFIG_FREERTOS_USE_TRACE_FACILITY` (иначе блок кода
  отключается макросом `NOGO`).
- `wc_GenerateSeed` реализован через программный RNG ESP32-S3 (`esp_fill_random`);
  аппаратное ускорение wolfcrypt (`WOLFSSL_ESP32WROOM32_CRYPT`) отключено — на S3 недоступно.
- Аппаратная проверка (реальная прошивка T-2CAN, CAN TWAI, MCP2515, модем A7670E) не выполнялась —
  проведена только проверка сборки.