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
| `vehicle/OVMS.V3/main/ovms_peripherals.cpp` | `VSPI_HOST → SPI3_HOST` и пр.; NULL-guard пинов can3 (`VSPI_PIN_MCP2515_2_* < 0`) и `m_mcp2515_2 = NULL`; can1: `#elif CONFIG_OVMS_COMP_TWAICAN → new twaican(...)`; `BIT(MODEM_GPIO_RX/TX) → BIT64(...)` (пины 43/44 > 31, `unsigned long` на xtensa 32-бит → UB и обнуление пин-маски в `gpio_config`) |
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

### 1.3 Правки под железо T-2CAN (сессия 3)

| Файл | Суть |
|---|---|
| `vehicle/OVMS.V3/main/Kconfig` | новый `OVMS_COMP_TWAICAN` (default n, `depends on SOC_TWAI_SUPPORTED`) и взаимное исключение `OVMS_COMP_ESP32CAN ↔ OVMS_COMP_TWAICAN` — только один бэкенд может быть can1 |
| `vehicle/OVMS.V3/components/can/src/can.cpp` | `includeCAN` учитывает `CONFIG_OVMS_COMP_TWAICAN` |
| `vehicle/OVMS.V3/main/ovms_peripherals.h` | член `m_twai_can` и include `twaican.h` под `#elif CONFIG_OVMS_COMP_TWAICAN` |
| `vehicle/OVMS.V3/components/gpio_maps/t2can_can.h` | `MODEM_GPIO_RST → MODEM_GPIO_RESET` (совпадает с `ovms_peripherals.cpp`, иначе GPIO16 RESET не инициализируется) |
| `vehicle/OVMS.V3/components/simcom/src/simcom_7670.cpp` | `GetNetTypes()`: `"auto 2G 3G 4G" → "auto 2G 4G"` (у A7670 нет 3G) |

## 2. Новые файлы (не в апстриме)

| Файл | Назначение |
|---|---|
| `vehicle/OVMS.V3/components/gpio_maps/t2can_can.h` | карта GPIO платы LILYGO T-2CAN V1.0 (сверена со схемой, см. `porting/hw-lilygo-t2can.md`) |
| `vehicle/OVMS.V3/components/gpio_maps/CMakeLists.txt` | cmake-регистрация компонента gpio_maps (в апстриме только `component.mk`) |
| `vehicle/OVMS.V3/components/twaican/` (`CMakeLists.txt`, `component.mk`, `src/twaican.h`, `src/twaican.cpp`) | can1-бэкенд на ESP-IDF TWAI driver для ESP32-S3 (~555 строк, паттерн mcp2515: alert task + очередь + one-TX-in-flight) |
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

- Активный `sdkconfig` (не версионируется) генерируется из `sdkconfig.defaults` — единственный
  источник истины (существующий `sdkconfig` имеет приоритет над defaults, поэтому после правки
  defaults его нужно удалять и перегенерировать: `rm sdkconfig && idf.py build`).
- Ключевые значения для T-2CAN: `CONFIG_OVMS_COMP_ESP32CAN is not set` (+ `CONFIG_OVMS_COMP_TWAICAN=y`),
  `CONFIG_LWIP_PPP_PAP_SUPPORT=y` (иначе `gsmpppos.cpp:241` не находит `ppp_set_auth`/`PPPAUTHTYPE_PAP`),
  `CONFIG_HEAP_TASK_TRACKING=y`, `CONFIG_FREERTOS_USE_TRACE_FACILITY=y`,
  `CONFIG_ESP_WIFI_STATIC_TX_BUFFER=y`, `CONFIG_MG_ENABLE_SSL` **выключен**
  (нужен для `server.v2 tls yes` — см. `porting/server-openvehicles.md`).
- **BLE 4.2 vs 5.0 (сессия 3):** на ESP32-S3 `SOC_BLE_50_SUPPORTED=1` → по умолчанию
  `CONFIG_BT_BLE_50_FEATURES_SUPPORTED=y`, `42=n`. Но legacy-функции, которые использует
  OVMS (`esp_ble_gap_start_advertising`, `esp_ble_gap_config_adv_data` из `esp32bluetooth`),
  компилируются только под `#if BLE_42_FEATURE_SUPPORT` (`esp_gap_ble_api.c:30-143`,
  `bt_target.h:219`) → линковка падала на undefined reference. Kconfig запрещает одновременную
  работу 4.2 и 5.0; в `sdkconfig.defaults` выставлено `CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y` +
  `CONFIG_BT_BLE_50_FEATURES_SUPPORTED=n` (как в IDF-примерах `ble_ancs`/`ble_compatibility_test`
  для `esp32s3`). На классическом ESP32 (апстрим-цель) 5.0 не поддерживается, поэтому upstream
  это не замечает.
- **BT впервые включён в воспроизводимый конфиг:** старый бэкап (`%TEMP%\ovms_sdkconfig.bak`)
  имел `# CONFIG_BT_ENABLED is not set`, новый (из defaults) — `=y` (сознательно: компонент
  `esp32bluetooth` должен собираться). Коекс-ключи BT/WiFi выключены в обоих.
- **ICE компилятора:** `xtensa-esp32s3-elf-gcc` падает (internal compiler error, IRA pass) на
  `esp_lcd/rgb/esp_lcd_panel_rgb.c` при полной параллельной сборке (недетерминировано, под
  нагрузкой), изолированно (`ninja -j1 <obj>`) собирается. Обход: собрать этот объект
  однопоточно, затем возобновить `idf.py build`.
- **Результат чистой сборки (сессия 3):** `rm sdkconfig && idf.py build` → `Project build complete`,
  `ovms3.bin` 0x4b99c0 (32% partition free), exit 0.

## 5. Известные дефекты апстрима (для заведения issue)

1. `dc86d82da` (vwegolf): CMakeLists ссылается на несуществующий `vehicle_vwegolf_climate.cpp`
   и не включает `vehicle_vwegolf_bat_ctrl.cpp` → сборка `master` сломана для всех.
2. (уточняется по результатам дальнейшей работы)
