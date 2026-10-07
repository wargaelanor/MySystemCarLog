# LILYGO T-2CAN ↔ OVMS3 v3.3.006: пин-аут, сверка `t2can_can.h`, зависимости от периферии

Задача (см. `porting/TEAM.md`): собрать пин-аут платы LILYGO T-2CAN из официальных источников,
сверить с `vehicle/OVMS.V3/components/gpio_maps/t2can_can.h`, показать, от какой периферии
OVMS Module v3.3 зависит код, и как выбирается GPIO-карта. **Только исследование и документация**:
ни код, ни `sdkconfig*`, ни карты GPIO, ни `REPORT.md` не менялись.

## Итог в шести пунктах

1. Пин-аут восстановлен из официальной схемы `project/T-2Can_V1.0.pdf` (2 страницы, экспорт EasyEDA:
   текст и провода разобраны по координатам, ключевые узлы проверены рендерами) и сверен с
   `pin_config.h` из репозитория LILYGO: модуль U3 (40 выводов), две CAN-цепи, CNC1/CNC2, заголовок
   U8 (2×13), клеммники P1/P2, две кнопки.
2. **Расхождений по GPIO, которые задаёт `t2can_can.h`, нет**: SPI 13/11/12, `MCP2515_CS=10`,
   `MCP2515_1_INT=8`, TWAI `TX=7/RX=6`, UART0 43/44, модем 15/16/17 — всё совпадает со схемой.
   Направление TWAI подтверждено трассировкой (GPIO7 → `TXB_CAN` → UY1 → `CANB_RXD` → U1.TXD;
   GPIO6 ← U1.RXD) и `pin_config.h` (`CAN_TX 7`, `CAN_RX 6`).
3. Расхождения есть именные и структурные: `MODEM_GPIO_RST` (карта) ≠ `MODEM_GPIO_RESET`
   (`ovms_peripherals.cpp:122`) → GPIO16 прошивкой не инициализируется; `GPIO9 → RESET# MCP2515`
   схемой подключён, в карте не описан, драйвером `mcp2515` не используется (у него вообще нет
   пина сброса), а подтяжки на нете `RESET#` на плате нет.
4. CAN-канала два, **оба постоянно терминированы 120 Ом** (RZ1/RZ2): P2 (нижний) = канал A =
   MCP2515 → в OVMS это «can2»; P1 (верхний) = канал B = встроенный TWAI → это «can1» в терминах
   OVMS. Имена нетов канала B перекрёстные (`CANB_TXD` идёт в `RXD` трансивера) — дефект схемы,
   на работу не влияет.
5. Главная зависимость: `# CONFIG_OVMS_COMP_ESP32CAN is not set` (`sdkconfig:3655`), а драйвера
   TWAI в дереве OVMS нет (единственные вхождения `twai_` — комментарии `esp32can.cpp:191,241,857`)
   → сейчас «can1» не обслуживается ничем, вопреки `REPORT.md:29-30,83-84` («CAN1 = штатный
   TWAI-контроллер»).
6. Чистая сборка от `sdkconfig.defaults` упадёт: запрета на `esp32can` там нет, а Kconfig даёт ему
   `default y` (`main/Kconfig:759-761`) → `esp32can.cpp:544` ссылается на несуществующий на ESP32-S3
   `CAN_TX_IDX`. Сам `sdkconfig` (25.09.2026) старше и не согласован с `sdkconfig.defaults`
   (30.09.2026).

---

## 1. Пин-аут LILYGO T-2CAN (официальные источники)

### 1.1 Как собрано

| Что | Источник |
|---|---|
| Схема платы (2 стр.), координаты текста и проводов | `https://github.com/Xinyuan-LilyGO/T-2Can` → `project/T-2Can_V1.0.pdf` |
| Обозначения выводов модуля, `ESP_BOOT` | `.../libraries/private_library/pin_config.h` |
| Перечень файлов репо | `https://api.github.com/repos/Xinyuan-LilyGO/T-2Can/git/trees/main?recursive=1` |
| Рендеры для визуальной проверки | с.1 PDF → `T-2Can_V1.0.pdf` (снимки `u8.png`, `conn.png`, `cnc.png`, `p1.png`, `p2.png`) |
| Локальная разборка | скрипты `u1..u57.py` (PyMuPDF: `get_text("words")` = текст по координатам, `get_drawings()` = провода) |
| Паспорт MCP2515 | `docs/MCP2515T-E-SO.pdf`, `MCP2515.pdf` |

Метод: координаты подписаны EasyEDA в системе координат страницы, поэтому выдержки (outline)
обходятся без правок. Значения и номера выводов считались подтверждёнными только после того, как
совпали **три** независимых источника: текст символа, нет-метка и провод, а для критичных узлов —
ещё и рендер.

### 1.2 U3 — ESP32-S3-WROOM-1U, 40 выводов (номер = физический вывод модуля)

| № | GPIO | Нет/куда ведёт | № | GPIO | Нет/куда ведёт |
|---|---|---|---|---|---|
| 1 | GND | GND | 21 | IO13 | `SPI_MISO` → U5.14 (SO) |
| 2 | 3V3 | VDD3V3 | 22 | IO14 | → U8.21 |
| 3 | EN | `ESP32_EN`: подтяжка R2 10K → 3V3, кнопка SW1 → GND (RESET) | 23 | IO21 | → U8.20 |
| 4 | IO4 | → U8.14 | 24 | IO47 | → U8.19 |
| 5 | IO5 | → U8.16 | 25 | IO48 | → U8.18 |
| 6 | IO6 | `TXB_CAN` → UY1.B1 (MS4553S) | 26 | IO45 | → U8.17 |
| 7 | IO7 | `RXB_CAN` → UY1.B2 | 27 | IO0 | `IO0`: R1 10K, кнопка S1 → GND (BOOT) |
| 8 | IO15 | `IO15` → U8.15 — PWRKEY модема | 28 | IO35 | → U8.5 — ⚠ PSRAM (OCT) |
| 9 | IO16 | `IO16` → U8.13 — RESET модема | 29 | IO36 | → U8.11 — ⚠ PSRAM |
| 10 | IO17 | `IO17` → U8.22 — DTR модема | 30 | IO37 | → U8.9 — ⚠ PSRAM |
| 11 | IO18 | → U8.23 | 31 | IO38 | → U8.7 |
| 12 | IO8 | `INT` → U5.12 (INT#), подтяжка RS2 10K → VDD3V3 | 32 | IO39 | → U8.6 |
| 13 | IO19 | `USB_N` через RS8 0R | 33 | IO40 | → U8.12 |
| 14 | IO20 | `USB_P` через RS9 0R | 34 | IO41 | → U8.10 |
| 15 | IO3 | → U8.26 | 35 | IO42 | → U8.8 |
| 16 | IO46 | → U8.25 | 36 | RXD0 (IO44) | `Uart_RX` → CNC1.4 |
| 17 | **IO9** | **`RESET#` → U5.17** (в карте не описан) | 37 | TXD0 (IO43) | `Uart_TX` → CNC1.3 |
| 18 | IO10 | `SPI_CS` → U5.16 (CS#) | 38 | IO2 | `IO2` → CNC2.4 |
| 19 | IO11 | `SPI_MOSI` → U5.15 (SI) | 39 | IO1 | `IO1` → CNC2.3 |
| 20 | IO12 | `SPI_SCLK` → U5.13 (SCK) | 40 | GND | GND |

Нумерация выводов совпадает со стандартным пин-аутом ESP32-S3-WROOM-1; сетевые имена взяты из
символов на с.1 схемы (маркировка `PIU3xx` + текст вывода).

### 1.3 U5 — MCP2515, кварц и питание

| Вывод | Нет | Комментарий |
|---|---|---|
| 1 TXCAN / 2 RXCAN | `TXA_CAN` / `RXA_CAN` | → UY2 (MS4553S) → `CANA_TXD` / `CANA_RXD` → U2 |
| 7 OSC2 / 8 OSC1 | X1 | **X1 = 16 МГц ±10 ppm**, C16/C17 = 12 пФ → GND |
| 9 VSS | GND | проверено проводом до символа земли |
| 12 INT# | `INT` | → U3.IO8, подтяжка RS2 10K → VDD3V3 |
| 13 SCK / 14 SO / 15 SI / 16 CS# | `SPI_SCLK` / `SPI_MISO` / `SPI_MOSI` / `SPI_CS` | → U3.IO12 / IO13 / IO11 / IO10 |
| 17 #RESET | `RESET#` | → U3.IO9; **подтяжки на плате нет** |
| 18 VDD | VDD3V3 | C12 100 нФ + C13 10 мкФ → GND |

### 1.4 Две CAN-цепи: путь сигнала от контроллера до клеммника

| | Канал A (нижний, «can2» в OVMS) | Канал B (верхний, «can1» в OVMS) |
|---|---|---|
| Контроллер | U5 MCP2515 (SPI) | встроенный TWAI ESP32-S3 |
| Выход контроллера | `TXA_CAN` ← U5.1 | `TXB_CAN` ← U3.IO7 (TX) |
| Вход контроллера | `RXCAN` → U5.2 | `RXB_CAN` → U3.IO6 (RX) |
| Преобразователь уровня | UY2 MS4553S (A=VDD3V3, B=VCC_5V), OE ← R3 100K ← VCCA | UY1 MS4553S (A=VDD3V3, B=VCC_5V), OE ← RS1 100K ← VCCA |
| Трансивер | U2 TD501MCAN: 1 RXD ← `CANA_RXD`, 2 TXD → `CANA_TXD` | U1 TD501MCAN: 1 RXD ← `CANB_RXD`, 2 TXD ← `CANB_TXD` |
| Питание трансивера | **VCC_5V** через феррит (L2, `BLM18PG121SN1D`), C4 100 нФ + C5/C6 4.7 мкФ | **VCC_5V** через феррит L1 `BLM18PG121SN1D`, C1 100 нФ + C2/C3 4.7 мкФ |
| Фильтр общего режима | LY2 `SDCW3225S-2-102TF` | LY1 `SDCW3225S-2-102TF` |
| Терминация | RZ2 120R (CANHA↔CANLA) | RZ1 120R (CANHB↔CANLB) |
| Клеммник | **P2** `WJ15EDGVC-3.81-4P`: 1 `DGNDA`, 2 `CANHA`, 3 `CANLA`, 4 `SGNDA` | **P1** `WJ15EDGVC-3.81-4P`: 1 `DGNDB`, 2 `CANHB`, 3 `CANLB`, 4 `SGNDB` |

Обратите внимание на перекрёстные имена нетов канала B: `CANB_TXD` уходит в **RXD** трансивера
(вывод 1), `CANB_RXD` — в **TXD** (вывод 2). Это просто кривые имена: фактически
`U3.IO7(TX) → ... → U1.TXD` и `U3.IO6(RX) ← ... ← U1.RXD`, то есть направление корректное
(подтверждено также `pin_config.h`: `CAN_TX 7`, `CAN_RX 6`).

Со стороны A того же вида ошибок нет: `MCP2515.TXCAN → TXA_CAN → UY2 → CANA_TXD → U2.TXD`.

### 1.5 Разъёмы, кнопки, заголовок U8

| Узел | Состав |
|---|---|
| **CNC1** (ZX-SH1.0-4PWT, шелкогр. «Uart») | 1 GND, 2 VDD3V3, 3 `Uart_TX` (IO43), 4 `Uart_RX` (IO44) |
| **CNC2** | 1 GND, 2 VDD3V3, 3 `IO1`, 4 `IO2` |
| **S1** `TS-1069-A1B3-D4` | IO0 ↔ GND = **BOOT** (R1 10K подтяжка на IO0) |
| **SW1** `TS-1069-A1B3-D4` | `ESP32_EN` ↔ GND = **RESET** (R2 10K подтяжка EN) |
| RS8 / RS9 | 0R на IO19 / IO20 → `USB_N` / `USB_P` (USB-OTG мультиплексор ESP32-S3) |
| **U8** 2×13 | по `pin_config.h` + рендер: 1 VDD3V3, 3 VCC_5V, **2/4/24 GND**, 5 IO35, 6 IO39, 7 IO38, 8 IO42, 9 IO37, 10 IO41, 11 IO36, 12 IO40, 13 IO16, 14 IO4, 15 IO15, 16 IO5, 17 IO45, 18 IO48, 19 IO47, 20 IO21, 21 IO14, 22 IO17, 23 IO18, 25 IO46, 26 IO3 |

Модем A7670E подключается **только к U8** (`IO15` PWRKEY, `IO16` RESET, `IO17` DTR, `IO4/IO5` UART,
`IO18`/`IO44`…); на развязке платы это единственный путь, поэтому пины модема в `t2can_can.h`
физически реализованы через перемычку к U8.

### 1.6 Защита линии (на каждую линию свой набор)

| Элемент | Тип | Включение |
|---|---|---|
| RZ1/RZ2 | 120 Ом | **между CANH и CANL** — постоянная терминация обоих каналов |
| CY1/CY2 | 100 пФ | линия ↔ `DGNB` |
| TV1/TV2 | `SMD4532-090NF` (MOV) | линия ↔ `DGNB` (сторона клеммника) |
| TVS1…TVS3 | `P0150SD` | шунтирующие гасители (сторона трансивера) |
| F1/F2 | `SMD1812P014TF` (PPTC) | **в разрыве** `CANHB` / `CANLB` |
| RY1 1 МОм ∥ CY3 1000 пФ/2 кВ | | между `SGNDB` и `DGNB` |

---

## 2. Сверка `t2can_can.h` ↔ схема

| Что в карте | Значение | Схема / `pin_config.h` | Вердикт |
|---|---|---|---|
| `VSPI_PIN_MISO` | 13 | `SPI_MISO` → U5.14 (SO); `pin_config.h: SPI_MISO 13` | совпадает |
| `VSPI_PIN_MOSI` | 11 | `SPI_MOSI` → U5.15 (SI); `SPI_MOSI 11` | совпадает |
| `VSPI_PIN_CLK` | 12 | `SPI_SCLK` → U5.13 (SCK); `SPI_SCLK 12` | совпадает |
| `MCP2515_1_CS` | 10 | `SPI_CS` → U5.16 (CS#); `MCP2515_CS 10` | совпадает |
| `MCP2515_1_INT` | 8 | `INT` → U5.12 (INT#) + RS2 10K; `MCP2515_INT 8` | совпадает |
| `ESP32CAN_PIN_TX` | 7 | IO7 → `TXB_CAN` → UY1 → `CANB_TXD` → U1.TXD; `CAN_TX 7` | совпадает, направление TX |
| `ESP32CAN_PIN_RX` | 6 | IO6 → `RXB_CAN` → UY1 → `CANB_RXD` → U1.RXD; `CAN_RX 6` | совпадает, направление RX |
| `MCP2515_2_CS` / `_INT` | -1 | второго MCP2515 нет | корректно |
| `MODEM_TX` / `MODEM_RX` | 43 / 44 | `Uart_TX`/`Uart_RX` → CNC1.3/4; `UART_TXD0 43`, `UART_RXD0 44` | совпадает |
| `MODEM_GPIO_PWR` | 15 | `IO15` → U8.15 (PWRKEY) | совпадает |
| `MODEM_GPIO_DTR` | 17 | `IO17` → U8.22 (DTR) | совпадает |
| **`MODEM_GPIO_RST`** | 16 | `IO16` → U8.13 (RESET), но код ждёт `MODEM_GPIO_RESET` | **расхождение имени** |
| `MODEM_GPIO_RING` | не задан | в карте нет нета/вывода | отсутствует в обеих |
| — | не задан | **IO9 → `RESET#` U5.17**; `pin_config.h: MCP2515_RST 9` | **пропущено в карте** |
| — | — | `pin_config.h: ESP_BOOT 0` ↔ IO0 кнопка S1 | в карте IO0 нет (BOOT не нужен прошивке) |

Семантика: `VSPI_*` используются и как пины шины, и как источник `SPI3_HOST`
(`ovms_peripherals.cpp:87-89`), поэтому переименование в `SPI3_HOST` в `REPORT.md:29-30` относится
к выбору хоста, а не к номерам пинов.

---

## 3. Зависимости кода от периферии OVMS Module v3.3 и механизм выбора карты

### 3.1 Как выбирается GPIO-карта (Kconfig → sdkconfig → `#include`)

| Шаг | Файл:строки | Что происходит |
|---|---|---|
| Выбор модели платы | `main/Kconfig:19-38` | `OVMS_HW_MODEL`, default `OVMS_HW_BASE_3_0` |
| Выбор способа маппинга | `main/Kconfig:40-57` | `OVMS_HW_SELECT_GPIO_MAP`, default `OVMS_HW_DEFAULT_GPIO_MAP` |
| Имя файла карты | `main/Kconfig:59-64` | `OVMS_GPIO_MAP_FILE`, default `NONE`, доступен только при `OVMS_HW_REMAP_GPIO` |
| Подключение карты | `main/ovms_peripherals.h:146-148` | `#ifdef CONFIG_OVMS_HW_REMAP_GPIO` → `#include CONFIG_OVMS_GPIO_MAP_FILE` |
| Карта по умолчанию | `main/ovms_peripherals.h:86-143` | блок `#ifdef CONFIG_HW_DEFAULT_GPIO_MAP` (штатный Module v3) |
| Что включено сейчас | `sdkconfig:3475-3480` | `OVMS_HW_BASE_3_0=y`, `OVMS_HW_DEFAULT_GPIO_MAP is not set`, `OVMS_HW_REMAP_GPIO=y`, `OVMS_GPIO_MAP_FILE="t2can_can.h"` |
| Задано в defaults | `sdkconfig.defaults:36-39` | `REMAP=y`, `DEFAULT_GPIO_MAP=n`, `MAP_FILE="t2can_can.h"` |
| Путь поиска | `components/gpio_maps/CMakeLists.txt` | `INCLUDE_DIRS .` → указывать имя файла, а не путь |

Побочный эффект `OVMS_HW_BASE_3_0`: `CONFIG_OVMS_HW_SPIMEM_AGGRESSIVE` (`main/Kconfig:66-69`)
требует `OVMS_HW_BASE_3_1`, поэтому агрессивный перенос в SPI-RAM не активируется — это нормально
для S3.

### 3.2 Компоненты периферии: что в `sdkconfig` включено/выключено

| Компонент | `sdkconfig` | default в Kconfig | Обращения к пинам закрыты `#ifdef` | Что из этого следует |
|---|---|---|---|---|
| `OVMS_COMP_ESP32CAN` (can1) | **is not set** (`:3655`) | **y** (`Kconfig:759-761`) | `ovms_peripherals.h:50-52,164-166`; `.cpp:147-150`; `ovms_housekeeping.cpp:237-239` | сейчас **can1 не создаётся вовсе**; драйвера TWAI в дереве нет |
| `OVMS_COMP_MCP2515` (can2, can3) | `=y` (`:3657`) | y (`Kconfig:766-768`) | `ovms_peripherals.h:42-44,180-183`; `.cpp:96-103,168-173` | создаются `can2` (CS=10/INT=8) и `can3` (CS=-1/INT=-1) |
| `OVMS_COMP_MAX7317` | is not set (`:3653`) | y | `ovms_peripherals.h:54-56,160-162`; `.cpp:91-94,142-145`; `ovms_housekeeping.cpp:261-268`; `ovms_led.cpp:94-96`; `simcom_powering.h:83-93` | нет расширителя портов → `MODEM_EGPIO_*` разрешаются в прямые GPIO |
| `OVMS_COMP_ADC` | is not set (`:3659`) | y | `ovms_housekeeping.cpp:75-100,175,360`; `ovms_peripherals.h:38-40,176-178`; `.cpp:162-166` | датчики не читаются |
| `OVMS_COMP_EXT12V` | is not set (`:3660`) | y | `ovms_peripherals.cpp:175-177,211-213` | |
| `OVMS_COMP_EXTERNAL_SWCAN` | is not set (`:3658`) | y | `ovms_peripherals.cpp:207-209` | |
| `OVMS_COMP_SDCARD` | is not set (`:3685`) | y (`Kconfig:887-892`) | `ovms_peripherals.h:66-68,189-191`; `.cpp:105-115,179-182` | SD-карта не инициализируется |
| `OVMS_COMP_CELLULAR` / `_CELLULAR_SIMCOM` | `=y` (`:3682`, `:3684`) | y (`Kconfig:880-883`) | `ovms_peripherals.h:70-72,193-195`; `.cpp:184-205` | SIMCOM работает, т.к. `depends on … (MAX7317 \|\| HW_REMAP_GPIO)` выполнен через `REMAP` |
| `OVMS_COMP_OBD2ECU` | `=y` (`:3687`) | y | `ovms_peripherals.cpp:207-209` | |

### 3.3 Обращения к пинам (file:line)

| Файл:строки | Что делает |
|---|---|
| `main/ovms_peripherals.cpp:85` | `gpio_install_isr_service` |
| `main/ovms_peripherals.cpp:87-89` | пины SPI из карты (MISO/MOSI/CLK) → `SPI3_HOST` |
| `main/ovms_peripherals.cpp:91-94` | CS MAX7317 (закрыт `#ifdef MAX7317`) |
| `main/ovms_peripherals.cpp:96-103` | CS/INT MCP2515; для can3 берутся `VSPI_PIN_MCP2515_2_CS/INT = -1` → `gpio_set_direction(-1)` даёт `ESP_ERR_INVALID_ARG` (лог-ошибки, не краш) |
| `main/ovms_peripherals.cpp:105-115` | пины SD (закрыт `#ifdef SDCARD`) |
| `main/ovms_peripherals.cpp:117-133` | **117** `#ifdef CONFIG_OVMS_HW_REMAP_GPIO`; **118-121** `MODEM_GPIO_PWR`; **122-125** `MODEM_GPIO_RESET` (level 1, «active LOW - NOT USED YET»); **126-128** `MODEM_GPIO_RING` (в карте нет); **129-132** `MODEM_GPIO_DTR` |
| `main/ovms_peripherals.cpp:140` | `spi_bus_initialize(SPI3_HOST, …)` |
| `main/ovms_peripherals.cpp:142-145` | `new max7317("egpio", …)` |
| `main/ovms_peripherals.cpp:147-150` | `new esp32can("can1", …)` |
| `main/ovms_peripherals.cpp:168-173` | `new mcp2515(…)`: строка 170 → `can2`, **строка 172 → `can3` с `-1,-1`** |
| `main/ovms_peripherals.cpp:184-205` | `new simcom(…)` |
| `main/ovms_housekeeping.cpp:75-100,175,360` | цикл чтения ADC (`#ifdef OVMS_COMP_ADC`) |
| `main/ovms_housekeeping.cpp:237-239` | `#ifdef OVMS_COMP_ESP32CAN` → `m_espcan->SetPowerMode` |
| `main/ovms_housekeeping.cpp:261-268` | `#ifdef OVMS_COMP_MAX7317` → `m_max7317->AutoInit()` |
| `main/ovms_led.cpp:72-74,94-96` | индикация через MAX7317 (`m_max7317->Output(pin, …)`) |
| `main/simcom_powering.h:83-93` | включение модема: `MODEM_EGPIO_*` → либо MAX7317, либо прямые GPIO |
| `components/esp32can/src/esp32can.cpp:544,549` | `gpio_matrix_out/…(CAN_TX_IDX / CAN_RX_IDX)` — индексы захардкожены, на ESP32-S3 не существуют |
| `components/esp32can/src/esp32can_regdef.h:40` | `MODULE_ESP32CAN 0x3ff6b000` — адрес ESP32, на S3 это `0x6002B000` |
| `components/mcp2515/src/mcp2515.cpp:70-71,80,101-102,126` | работа с `MCP2515_INT_GPIO` / пинами из конструктора |
| `components/mcp2515/src/mcp2515.cpp:170,185,319` | `Start()` и `Reset()` шлют `CMD_RESET` по SPI — **аппаратный RESET# не используется** |
| `components/ovms_led/…`, `components/spi/…` | пины только из аргументов конструктора |

### 3.4 Ключевой вывод по разделу

Выбор карты (`OVMS_GPIO_MAP_FILE="t2can_can.h"`) управляет **только** значениями макросов
`SPI_*` / `ESP32CAN_*` / `MCP2515_*` / `MODEM_*`. Список же создаваемых объектов (`can1`…`can3`,
ADC, SD, MAX7317, модем) определяется **отдельно** — `CONFIG_OVMS_COMP_*` в `sdkconfig`.
Карта и компоненты независимы: карта говорит «куда», `sdkconfig` — «что вообще подключать».

---

## 4. Риски и открытые вопросы

### 4.1 Подтверждённые риски

| # | Риск | Основание |
|---|---|---|
| R1 | **«can1» сейчас не работает ни с чем.** Есть `esp32can` (выключен), TWAI-драйвера нет | `sdkconfig:3655`; `twai_` встречается только в `build/**/sdkconfig.h` и комментариях `esp32can.cpp:191,241,857`; утверждение `REPORT.md:29-30,83-84` («CAN1 = штатный TWAI-контроллер») не подтверждается |
| R2 | **Чистая сборка упадёт**: без запрета в defaults `esp32can` включится (default y) и упрётся в несуществующий `CAN_TX_IDX` | `Kconfig:759-761`; `esp32can.cpp:544,549`; в `sdkconfig.defaults` запрета нет |
| R3 | `esp32can` в принципе не переносим на S3 | `esp32can_regdef.h:40` (`0x3ff6b000` вместо `0x6002B000`), `esp32can.cpp:544` |
| R4 | `sdkconfig` старше/не согласован с `sdkconfig.defaults` | даты файлов: 25.09.2026 против 30.09.2026 |
| R5 | **IO9 (`RESET#` MCP2515) без подтяжки**: вывод висит, драйвер сброса не даёт | схема: на нете `RESET#` только U3.17 ↔ U5.17; `MCP2515.pdf` p4 (вывод 17 «Active-low device Reset input», внутренней подтяжки нет; п.9.0/p.57 рекомендует RC); `mcp2515.cpp:185` даёт только SPI-сброс |
| R6 | `MODEM_GPIO_RST` vs `MODEM_GPIO_RESET` → GPIO16 не инициализируется прошивкой | `t2can_can.h` против `ovms_peripherals.cpp:122` |
| R7 | `can3` создаётся с `CS=-1/INT=-1` → штатные `ESP_ERR_INVALID_ARG` в логе при инициализации | `ovms_peripherals.cpp:172`, `.cpp:96-103` |
| R8 | **GPIO33-37 заняты PSRAM** (`CONFIG_SPIRAM_MODE_OCT=y`) → U8.5/11/9 (IO35/36/37) непригодны для периферии | `sdkconfig:1724,1730` |
| R9 | Оба канала жёстко терминированы 120 Ом — при подключении к уже терминированной шине будет двойная терминация | RZ1/RZ2 на схеме (проверено проводами) |
| R10 | Питание A7670E: `VBAT(min)=3,4 В`, а логика платы 3,3 В | требует отдельной проверки схемы питания модема |

### 4.2 Открытые вопросы (требуют подтверждения)

1. **Связь `DGNDA/B`, `SGNDA/B` с общей землёй**: в зоне CAN символа общей GND не найдено;
   изоляция развязки подтверждается RY1 1 МОм ∥ CY3 1000 пФ/2 кВ, но точка соединения не локализована.
2. **Вывод 5 `CANG` TD501MCAN** — функция на схеме не подписана (по аналогии — общий вход,
   но из документации источника не подтверждено).
3. **Пара A1↔B1 MS4553S** принята по стандартной схеме включения; прямого подтверждения в
   источниках LILYGO нет.
4. **Соответствие шелкографии клеммникам**: номера P1/P2 взяты из рендера (`conn.png`) и
   согласуются с трассировкой, но буквенные метки «A»/«B» у правого края — метки рамки чертежа,
   а не обозначения клеммников.
5. **Ветка `T_2Can_Fd` в `pin_config.h`** описывает плату с MCP2518 (INT_0=9, INT_1=3), а на
   нашей плате U5 = MCP2515 + кварц 16 МГц → нужна ветка не-Fd; расхождение с файлом LILYGO.
6. **Файл `pin.jpg` в репозитории** — происхождение неизвестно, в отчёт не включён.
7. Ссылки `lilygo.cc/products/t-2can` ведут на JS-оболочку и цитируемыми не являются.

---

## 5. Источники

**Официальные (LILYGO)**
1. `https://github.com/Xinyuan-LilyGO/T-2Can` — дерево репозитория
   (`https://api.github.com/repos/Xinyuan-LilyGO/T-2Can/git/trees/main?recursive=1`).
2. `https://github.com/Xinyuan-LilyGO/T-2Can/blob/main/project/T-2Can_V1.0.pdf` — схема платы
   (с.1: U3, U5, UY1/2, U1/U2, U8, P1/P2, CNC1/CNC2, защита; с.2: остальное).
3. `https://github.com/Xinyuan-LilyGO/T-2Can/blob/main/libraries/private_library/pin_config.h` —
   обозначения выводов (`CAN_TX/CAN_RX`, `SPI_*`, `MCP2515_CS/RST/INT`, `ESP_BOOT`, UART).
4. `https://github.com/Xinyuan-LilyGO/T-2Can/blob/main/project/T-2Can-Fd_V1.0.pdf` —
   схема варианта Fd (MCP2518), для сравнения.
5. `https://github.com/Xinyuan-LilyGO/T-2Can/tree/main/examples` — `can/can.ino`,
   `esp32_can/esp32_can.ino`; `debug/examples/self_test/self_test.ino`.
6. `https://github.com/Xinyuan-LilyGO/T-2Can/blob/main/README.md` — назначение платы.
7. `https://github.com/Xinyuan-LilyGO/T-2Can/blob/main/docs/MCP2515T-E-SO.pdf`,
   `MCP2515.pdf` — таблица выводов (p.4), раздел про сброс (p.57, §9), RESET-инструкция SPI (p.65).

**Код и конфигурация (локально, не изменялись)**
8. `vehicle/OVMS.V3/components/gpio_maps/t2can_can.h` — сверяемая карта.
9. `vehicle/OVMS.V3/main/Kconfig:19-38,40-57,59-64,66-69,759-771,880-892`.
10. `vehicle/OVMS.V3/sdkconfig:1724,1730,3475,3478-3480,3652-3687`; `sdkconfig.defaults:36-39`.
11. `vehicle/OVMS.V3/main/ovms_peripherals.{h,cpp}`, `ovms_housekeeping.cpp`, `ovms_led.cpp`,
    `ovms_module.cpp`, `simcom_powering.h`.
12. `vehicle/OVMS.V3/components/{esp32can,mcp2515,spi,gpio_maps,can}`.
13. `vehicle/OVMS.V3/REPORT.md` (кодировка CP866), строки 29-30 и 83-84.
14. `vehicle/OVMS.V3/components/gpio_maps/CMakeLists.txt`, `Readme.md`.
15. `support/sdkconfig.lilygo_tc`, `support/lilygo_tc_v10/_v10_a/_v11.h` — апстримная поддержка
    LILYGO (это T-Call A7670, не T-2CAN).

**Инструменты разборки**: Python 3.13 (`pypdf`, `PyMuPDF`), скрипты `u1..u57.py`,
рендеры `u8.png`, `conn.png`, `cnc.png`, `p1.png`, `p2.png` (во временной папке).
