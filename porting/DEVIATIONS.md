# Отклонения от upstream (DEVIATIONS)

Правило: **каждое** осознанное отклонение от `openvehicles/Open-Vehicle-Monitoring-System-3`
фиксируется здесь. База: тег **3.3.006** (`0dbce1e`) + `upstream/master` (слияние `69fd4d728`,
HEAD апстрима `7c8778407` от 2026-10-03).

---

## 1. Правки в существующих файлах суперпроекта

### 1.1 Совместимость с ESP-IDF 5.4 / ESP32-S3 / GCC 14 (базовый порт, см. `vehicle/OVMS.V3/REPORT.md`)

| Файл | Суть отклонения |
|---|---|
| `vehicle/OVMS.V3/CMakeLists.txt` | глобально `-Wno-error=format*` и др. (uint32_t == `long unsigned int` в новом тулчейне) |
| `vehicle/OVMS.V3/main/CMakeLists.txt` | в SRCS добавлены `background_shell.cpp`, `file_writer.cpp` (в апстриме не входили в cmake-список) |
| `vehicle/OVMS.V3/main/ovms_main.cpp` | инициализация WDT/логов под IDF≥5 (`WDT_ALREADY_INITIALIZED` и др.) |
| `vehicle/OVMS.V3/main/ovms_module.cpp` | `ets_install_putc1` → `esp_rom_install_channel_putc`; включены `CONFIG_HEAP_TASK_TRACKING`, `FREERTOS_USE_TRACE_FACILITY` |
| `vehicle/OVMS.V3/main/ovms_metrics.cpp` | каст `bool → dbcNumber` (новый API) |
| `vehicle/OVMS.V3/main/ovms_netmanager.cpp` | итерация по `netiflist` (dangling pointer) |
| `vehicle/OVMS.V3/main/ovms_peripherals.cpp` | `VSPI_HOST → SPI3_HOST` и пр. |
| `vehicle/OVMS.V3/components/ovms_ota/src/ovms_partitions.cpp` | `spi_flash_* → esp_flash_*` |
| `vehicle/OVMS.V3/components/ovms_cellular/src/gsmpppos.cpp` | сигнатура `pppos_output_cb_fn` (IDF 5) |
| `vehicle/OVMS.V3/components/spi/spi.cpp`, `swcan/src/swcan.cpp` | `VSPI_HOST → SPI3_HOST` |
| `vehicle/OVMS.V3/components/console_telnet/*` | `MongooseLock()` напрямую |
| `vehicle/OVMS.V3/components/poller/src/vehicle_poller.cpp` | базовые виртуальные методы `VehicleSignal` (vtable) |
| `vehicle/OVMS.V3/components/vehicle_smarteq/CMakeLists.txt` | добавлен `src/eq_handle.cpp` |
| `vehicle/OVMS.V3/components/strverscmp/CMakeLists.txt` | всегда собирать `strverscmp.c` (newlib IDF4+ его не даёт) |
| `vehicle/OVMS.V3/components/esp32bluetooth/CMakeLists.txt` | `REQUIRES pcp` |
| `vehicle/OVMS.V3/components/mongoose/CMakeLists.txt`, `ovms_webserver/CMakeLists.txt` | правки под новый cmake/`file(TIMESTAMP)` |
| `vehicle/OVMS.V3/components/wolfssl/CMakeLists.txt` | в srcs добавлен `wolfssl/wolfcrypt/src/port/Espressif/esp32_random.c` |
| `vehicle/OVMS.V3/components/wolfssl/port/user_settings.h` | `WOLFSSL_ESPWROOM32` выключен (нет крипто-ускорения на S3) |
| `vehicle/OVMS.V3/components/vehicle_nissanleaf/src/vehicle_nissanleaf.cpp` | вызовы `m_max7317` под `#ifdef CONFIG_OVMS_COMP_MAX7317` (нет MAX7317 на T-2CAN) |
| `vehicle/OVMS.V3/components/vehicle_renaulttwizy/src/rt_ticker.cpp` | то же (для Twizy; не влияет на Leaf) |
| `vehicle/OVMS.V3/dependencies.lock` | регенерирован IDF 5.4 (`target: esp32s3`) |

### 1.2 Исправления дефектов `upstream/master` (после слияния `69fd4d728`)

| Файл | Суть |
|---|---|
| `components/vehicle_vwegolf/CMakeLists.txt` | коммит апстрима `dc86d82da` ссылается на `src/vehicle_vwegolf_climate.cpp`, который **нигде не закоммичен**, и не добавляет реально существующий `src/vehicle_vwegolf_bat_ctrl.cpp` → заменено на后者. **Кандидат на фикс в апстрим** (issue/PR). |
| `components/vehicle_toyota_etnga/src/etnga_metrics.cpp` | добавлен `#include <numeric>` (`std::accumulate` не виден без него на новом тулчейне) |

## 2. Новые файлы (не в апстриме)

| Файл | Назначение |
|---|---|
| `vehicle/OVMS.V3/components/gpio_maps/t2can_can.h` | карта GPIO платы LILYGO T-2CAN V1.0 (сверена со схемой, см. `porting/hw-lilygo-t2can.md`) |
| `vehicle/OVMS.V3/components/gpio_maps/CMakeLists.txt` | cmake-регистрация компонента gpio_maps (в апстриме только `component.mk`) |
| `vehicle/OVMS.V3/sdkconfig.defaults` | дефолты конфигурации для T-2CAN (esp32s3, 16MB, PSRAM-OCT, USB-JTAG консоль, GPIO map) |
| `vehicle/OVMS.V3/REPORT.md` | отчёт предыдущей сессии о порте (сборка build15–build25) |
| `DEV_LOG.md`, `PLAN.md`, `DEVIATIONS.md`, `TEAM.md`, `porting/*` | документация проекта |

## 3. Субмодули (fork-стратегия)

Правки нужны для ESP-IDF 5.4, но пушить в `openvehicles/*` мы не можем — поэтому
URL в `.gitmodules` переключены на форки под аккаунтом проекта:

| Субмодуль | Было | Стало | Коммит с правками | Содержание правки |
|---|---|---|---|---|
| `mongoose/mongoose` | `openvehicles/mongoose` | **`wargaelanor/mongoose`** (ветка `ovms-port`) | `3458fbf` | `cs_md5_final(unsigned char*)`; NULL-guard `iface->vtable->recved` в `mg_call` |
| `wolfssl/wolfssl` | `openvehicles/wolfssl` | **`wargaelanor/wolfssl`** (ветка `ovms-port`) | `aa396c3` | `wc_port.h`: `xSemaphoreHandle → SemaphoreHandle_t`; новый `esp32_random.c` (`wc_GenerateSeed` через `esp_fill_random`) |

Остальные субмодули (`zlib`, `libzip`, `wolfssh`) — без изменений, URL апстримовые.

## 4. Конфигурация сборки

- Активный `sdkconfig` (не версионируется) собран из `sdkconfig.defaults` + ручных правок
  предыдущей сессии: `CONFIG_OVMS_COMP_ESP32CAN is not set`, BLE 4.2, `CONFIG_MG_ENABLE_SSL`
  **выключен** (нужен для `server.v2 tls yes` — см. `porting/server-openvehicles.md`).
- **Задача:** свести `sdkconfig.defaults` и активный `sdkconfig` (проверка чистой сборки
  `rm sdkconfig && idf.py build`) — иначе чистая сборка включит `esp32can` и упадёт
  (см. `porting/hw-lilygo-t2can.md`, п.10–11).

## 5. Известные дефекты апстрима (для заведения issue)

1. `dc86d82da` (vwegolf): CMakeLists ссылается на несуществующий `vehicle_vwegolf_climate.cpp`
   и не включает `vehicle_vwegolf_bat_ctrl.cpp` → сборка `master` сломана для всех.
2. (уточняется по результатам дальнейшей работы)
