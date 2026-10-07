# Журнал разработки (JOURNAL)

Хронология всех действий команды. Каждая сессия/шаг — отдельный блок с датой.

---

## 2026-10-06 — Сессия 1: инициализация проекта (техлид)

**Контекст:** клон upstream `openvehicles/Open-Vehicle-Monitoring-System-3` (тег 3.3.006,
commit `0dbce1e`, detached HEAD). В рабочей копии уже найдены результаты предыдущей сессии —
порт сборки на ESP-IDF 5.4 под LILYGO T-2CAN (см. `vehicle/OVMS.V3/REPORT.md`).

Выполнено:

1. **Разведка репозитория.**
   - Обнаружен предварительный порт: 26 изменённых файлов против upstream + новые файлы
     `components/gpio_maps/` (карта GPIO T-2CAN), `sdkconfig.defaults`, `REPORT.md`.
   - Сборка по предыдущему отчёту успешна (build25, `ovms3.bin` 4630.7 KB), аппаратная
     проверка ещё не проводилась.
   - «Шум» в `git status` (~2300 файлов) — различия переводов строк (autocrlf), в индекс
     попадают только реальные изменения (`git add -n` → 30 путей).

2. **Настройка git.**
   - `origin` → `https://github.com/wargaelanor/MySystemCarLog.git` (доступ: gh авторизован
     как `wargaelanor`).
   - `upstream` → `https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3.git`
     (для обновлений от апстрима).
   - Исправлен одноразовый refspec клона (`+refs/tags/3.3.006:...` → `+refs/heads/*:...`),
     иначе `git fetch origin` падал.
   - Создана рабочая ветка `port/lilygo-t2can` от тега 3.3.006.
   - У `main` в целевом репо — один пустой «Initial commit» (README: `# MySystemCarLog`),
     история не связана с OVMS; запушено будет без force (см. план, фаза 0).

3. **Каркас документации** — каталог `porting/` (журнал, план, команда, архитектура);
   далее исследовательские отчёты агентов в `porting/`.

**Решения техлида:**
- Держать минимальные отклонения от upstream: новые файлы добавляем, существующие правим
  только по необходимости; каждый отклонённый файл фиксируется в журнале.
- Документация проекта — в `porting/`, чтобы не смешивать с апстримовской Sphinx-документацией `docs/`.
- Мусорные файлы (`*.avif` в корне) не коммитить, добавить в `.git/info/exclude`.

---

## 2026-10-06 — Сессия 2: команда, исследования, слияние апстрима, сборка (техлид)

### Делегирование (4 исследовательских агента, параллельно)
- HW LILYGO T-2CAN → `porting/hw-lilygo-t2can.md` (пин-аут сверен со схемой T-2Can_V1.0.pdf: расхождений по GPIO нет; найдены баги: `MODEM_GPIO_RST`≠`MODEM_GPIO_RESET` в `ovms_peripherals.cpp:122`, отсутствие CAN1-драйвера при выкл. esp32can, can3 с пинами -1, нет RC на RESET MCP2515).
- Модем A7670E → `porting/hw-a7670e.md` (драйвер `simcom_7670` целен в A7670; LASE≠FASE только GNSS; риск 1.8В UART vs 3.3В; PWRKEY требует NPN; `GetNetTypes()` отдаёт «2G 3G 4G» — у A7670 нет 3G).
- Сервер OVMS → `porting/server-openvehicles.md` (чек-лист подключения к openvehicles.com: vehicle id + server.v2 + password + auto; схема MP-C 0x30; TLS требует включённых `CONFIG_MG_ENABLE_SSL`+`MBEDTLS_PSK` — у нас выключены).
- Nissan Leaf 2018 → `porting/vehicle-nissanleaf-2018.md` (ZE1 заявлен с 3.3.005, но `xnl ze1` по умолчанию false; OBD-II за гейтвеем — нужен tap-кабель к 24-пину; найдены гэпы, часть из которых уже починена в апстрим-мастере).

### Git
- `origin` → `wargaelanor/MySystemCarLog`; добавлен `upstream` → openvehicles; исправлен клонированный refspec тега (иначе `fetch origin` падал).
- Клон оказался **shallow** → выполнен `git fetch --unshallow upstream` (история 7151 коммит).
- Ветка `port/lilygo-t2can` от тега 3.3.006; коммиты: `2e62d7e` (база порта), `9f36622`+`a7f95f6` (документация).

### Слияние апстрима (решение техлида)
- `upstream/master` (`7c8778407`) впереди на 362 коммита, **пересечение с нашими 26 файлами пустое** → влито без конфликтов: `69fd4d728`.
- Приобретено: 44 коммита Nissan Leaf (ZE1-фиксы: charge duration, 5bc, battery types, 12V current, CAN2-фильтры, SOH, тайминги poller и др.), фиксы cellular/PPP.

### Сборка (ESP-IDF 5.4, команда из REPORT.md)
- Ошибка 1: `vehicle_vwegolf/CMakeLists.txt` апстрима ссылается на несуществующий `vehicle_vwegolf_climate.cpp` → исправлено на реальный `vehicle_vwegolf_bat_ctrl.cpp` (дефект апстрима `dc86d82da`, см. DEVIATIONS.md §5).
- Ошибка 2: `etnga_metrics.cpp` — нет `#include <numeric>` для `std::accumulate` → добавлен.
- Ошибка 3 (линковка): тот же vwegolf — `VWeGolfBatteryControl::Ticker1` неопределён → следствие ошибки 1.
- **Результат: `Project build complete`, `ovms3.bin` 0x448230 (39% партиции свободно), BUILD_EXIT=0.**
- Окружение сборки: `export.bat` не находит тулчейн → используется явный PATH (см. REPORT.md §2).

### Субмодули (решение техлида: fork-стратегия)
- Локальные правки предыдущей сессии жили грязью внутри `mongoose` и `wolfssl` (не попадали в git) → воспроизводимость под угрозой.
- Созданы форки: `wargaelanor/mongoose`, `wargaelanor/wolfssl`; правки запечены коммитами `3458fbf` и `aa396c3`, запушены в ветку `ovms-port`; URL в `.gitmodules` переключены на форки (DEVIATIONS.md §3).

### Документация
- Созданы: `porting/DEVIATIONS.md`, 4 отчёта агентов; обновлены `PLAN.md`, `DEVIATIONS.md`.
- `*.avif`, `Qwen_text_*.txt` (бриф прошлой сессии) — в `.git/info/exclude`, не коммитятся.

### Пуш в MySystemCarLog (Фаза 0 закрыта, кроме чистой сборки)
- `README.md`: конфликт add/add при слиянии с «Initial commit» → оставлен апстримовский OVMS README (принцип минимальных отклонений), коммит `e002f6540`.
- `git push origin port/lilygo-t2can` → ветка на GitHub; `git push port/lilygo-t2can:main` → main обновлён ff: `c761f2ca0..e002f6540`. Без force.
- Остаток Фазы 0: чистая сборка (`rm sdkconfig && idf.py build`) — делегируется после сверки sdkconfig.defaults.

---

## 2026-10-07 — Сессия 3: CAN1 (twaican), фиксы под железо, чистая сборка (техлид)

### Делегирование
- **Агент A (general): CAN1-бэкенд** — новый компонент `components/twaican/` (ESP-IDF TWAI
  driver, паттерн mcp2515: alert task + очередь + one-TX-in-flight, ~555 строк), правки
  `main/Kconfig` (`OVMS_COMP_TWAICAN`, взаимное исключение с `ESP32CAN`), `can.cpp`
  (`includeCAN`), `ovms_peripherals.h/.cpp` (can1 через twaican, NULL-guard can3).
  Техлид провёл ревью диффов — принято (`m_tx_frame` пишется в `canbus::Write` can.cpp:1475;
  `m_mcp2515_2->` нигде не разыменовывается).
- `@jeff` упёрся в лимит шагов на механике → правки `t2can_can.h`/`simcom_7670.cpp` выполнены
  техлидом самостоятельно.

### Исправления
- `MODEM_GPIO_RST → MODEM_GPIO_RESET` (t2can_can.h) — выражение `ovms_peripherals.cpp:128-130`
  теперь активно (GPIO16 RESET модема).
- `GetNetTypes()` → `"auto 2G 4G"` (simcom_7670.cpp:73).
- **BLE 4.2/5.0 (линковка):** `esp_ble_gap_start_advertising`/`esp_ble_gap_config_adv_data`
  компилируются только под `BLE_42_FEATURE_SUPPORT` (`esp_gap_ble_api.c:30-143`, `bt_target.h:219`),
  а на S3 по умолчанию включён BLE 5.0 → undefined reference. В `sdkconfig.defaults` выставлено
  `BT_BLE_42_FEATURES_SUPPORTED=y` + `BT_BLE_50_FEATURES_SUPPORTED=n` (шаблон IDF-примеров для
  esp32s3); Kconfig запрещает одновременную работу 4.2/5.0.
- **`BIT(44) → BIT64`** в `ovms_peripherals.cpp`: пины UART модема 43/44 не помещались в
  32-битную `unsigned long`-пин-маску `gpio_config` (UB, маска обнулялась; варнинг
  `-Wshift-count-overflow` в логе сборки).

### Сборка (чистая, из sdkconfig.defaults)
- `rm sdkconfig` → регенерация из defaults; сборка дошла до линковки, упала на BLE-символах
  (см. выше) → фикс defaults → пересборка.
- **ICE компилятора повторился** (IRA pass, `esp_lcd_panel_rgb.c`, недетерминировано под
  полной параллельной нагрузкой) → обход: `ninja -j1 <obj>`, затем возобновление.
- **Результат: `Project build complete`, `ovms3.bin` 0x4b99c0 (32% partition free), exit 0.**
  Предупреждения `-Wshift-count-overflow` исчезли после `BIT64`.
- Ранее активный конфиг бэкаплен: `%TEMP%\ovms_sdkconfig.bak` (для сравнения: 498 различий
  с новым, из них 9 изменений значений — все учтены в `sdkconfig.defaults`).

### Документация
- `porting/DEVIATIONS.md`: §1.3 (правки под T-2CAN), §2 (twaican), §4 (конфигурация:
  BLE 4.2/5.0, BT=y, ICE-обход, результат чистой сборки).
- `PLAN.md`: Фаза 0 закрыта полностью; Фаза 1 — can1/twaican, MODEM_GPIO_RESET, can3-guard
  отмечены; Фаза 2 — GetNetTypes отмечен.
