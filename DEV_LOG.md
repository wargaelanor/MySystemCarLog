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
