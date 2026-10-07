# Nissan Leaf 2018 (ZE1) в OVMS3 — состояние, пробелы и план портирования

База для этого документа: локальный checkout `D:\OpenCode_projeck\OVMS`, версия **3.3.006**
(`vehicle/OVMS.V3/changes.txt:6`, OTA-релиз 2026-05-17) плюс **одно локальное отличие** от
upstream 3.3.006 — 4 строки `#ifdef CONFIG_OVMS_COMP_MAX7317` в `vehicle_nissanleaf.cpp`
(коммит `2e62d7e`). Все ссылки `file:line` ниже — на этот локальный baseline.

Драйвер: `vehicle/OVMS.V3/components/vehicle_nissanleaf/`
(`vehicle_nissanleaf.cpp`, `vehicle_nissanleaf.h`, `nl_web.cpp`, `nl_types.h`, `docs/index.rst`).

---

## 1. Что реализовано: метрики, данные и команды

### 1.1 Топология шин

| Элемент | Значение | Ссылка |
|---|---|---|
| CAN1 | EV-CAN (BMS, VCM, CHAdeMO-контроллер) | комментарий `vehicle_nissanleaf.cpp:969` |
| CAN2 | CAR-CAN (приборка, шлюз, TCU, TPMS) | комментарий `vehicle_nissanleaf.cpp:1756` |
| CAN3 (IT-CAN) | не регистрируется вовсе | — |
| Регистрация шин | 2 шины, 500 кбит/с, `autopoweroff=false` | `vehicle_nissanleaf.cpp:224-225` |
| Шлюз ZE1 на штатном OBD-II | закрыт, порт «засыпает» вместе с зажиганием | `docs/index.rst:85`, `:160` |

Стандартный кабель OVMS (OBD-II → DB9, 6 проводов) **непригоден** для ZE1: нужен
8-проводной CAN tap-кабель к 24-пиновому разъёму CAN-gateway за приборной панелью
(`docs/index.rst:23`, `:85`, `:154-193`). Pinout штатного кабеля ZE0 —
`vehicle/hardware/Electrical/NL.txt` (DB9 7/2 ↔ OBD 13/12, DB9 5/4 ↔ OBD 6/14, DB9 9 = +12 В).

### 1.2 Метрики с EV-CAN (CAN1)

| ID | Содержимое | Метрика | file:line |
|---|---|---|---|
| `0x1d4` | запрос/статус кузовных функций | — | `:974` |
| `0x1da` | момент/обороты двигателя | `v.e.rpm`, `v.m.rpm` | `:982` |
| `0x1db` | ток/напряжение батареи, пилот заряда | `v.b.current`, `v.b.voltage`, CHAdeMO-эвристика ZE1 | `:1007`, `:1036`, `:1068-1102` |
| `0x1dc` | мощность из/в батарею | `v.b.energy.used/received` | `:1106` |
| `0x284` | скорость | `v.p.speed` | `:1115`, `:1133` |
| `0x380` | заряд ZE0 (ген1-зарядник), напряжение AC | `v.c.*`, `v.g.*` | `:1139` |
| `0x390` | заряд AZE0 | `v.c.*` | `:1180` |
| `0x50a` | коды ошибок | — | `:1272` |
| `0x54a` | заданная температура HVAC | `v.e.cabintemp` (задание) | `:1280` |
| `0x54b` | вентилятор/дефростер/забор воздуха | `v.e.cabinfanmode`, `v.e.cabinintake` и др. | `:1290` |
| `0x54c` | температура окружающего воздуха | `v.e.ambienttemp` | `:1429` |
| `0x54f` | температура салона (расчётная) | `v.e.cabintemp` | `:1444` |
| `0x55a`, `0x55b` | температуры инвертора/зарядника, ток CC | `v.inv.temp`, `v.c.current` | `:1470`, `:1479` |
| `0x59e` | **приборный SOC** (ZE1: байт 7 × 0.5) | `v.b.soc`, `xnl.v.b.soc.instrument` | `:1491`, `:1506`, `:1510` |
| `0x5bc` | GIDS / bars / ZE0-SOH | `xnl.v.b.gids`, `xnl.v.b.soh.instrument`, `xnl.v.c.duration` | `:1531`, `:1555`, `:1593`, `:1667` |
| `0x5bf` | пилот/заряд ZE0-зарядника (ген1) | `v.c.pilot` | `:1676` |
| `0x5c0` | температура ячеек (байт 2 / 2 − 40) | `v.b.temp` | `:1731`, `:1737` |
| `0x679` | вспомогательное состояние | — | `:1749` |

### 1.3 Метрики с CAR-CAN (CAN2)

| ID | Содержимое | Метрика | file:line |
|---|---|---|---|
| `0x180` | положение педали газа | `v.c.throttle` | `:1761` |
| `0x292` | тормозная педаль | `v.e.footbrake` | `:1767` |
| `0x355` | единицы одометра (miles/km) | `xnl.odometer.units` | `:1773-1785` |
| `0x385` | TPMS (4 колеса) | `v.tpms.pressure.*` | `:1786-1791` |
| `0x421` | селектор передач | `v.e.gear` | `:1792-1815` |
| `0x5a9` | **приборный запас хода** | `xnl.v.b.range.instrument` | `:1816`, `:1821` |
| `0x5b9` | оставшиеся полосы заряда | `xnl.v.b.chargebars.remaining` | `:1825` |
| `0x5b3` | приборный SOH (не для ZE1) | `xnl.v.b.soh.instrument` | `:1835-1852` |
| `0x5c5` | одометр | `v.p.odometer` | `:1853`, `:1858` |
| `0x60d` | двери, фары, замки, кнопка Start | `v.d.*`, `v.e.locked`, `v.e.headlights`, `v.e.awake` | `:1861-1906` |

### 1.4 Опрашиваемые данные (OBD-II / UDS)

Списки опросов переключаются флагом `xnl.ze1` (`:244-251`, повторно `:635-642`):

| Список | PID | Ответ | Назначение | file:line |
|---|---|---|---|---|
| `obdii_polls_ze1` | `0x79b→0x7bb`, группа `0x01` | 39/41 б. | ток/ёмкость → SOH new car | `:72-85`, `:647`, `:728`, `:731` |
| то же | группа `0x02` | 196 б. | 96 напряжений ячеек | `:80`, `:735` |
| то же | группа `0x04` | 14/29 б. | 6 температур батареи → `v.b.temp` | `:82`, `:783-836` |
| то же | группа `0x06` | 96 б. | шунты балансировки (только при заряде) | `:81`, `:759` |
| **только ZE1** | группа `0x61` | 2 б. | **приборный SOH** | `:83`, `:838-854` |
| `obdii_polls_aze0` | `0x743/0x744/0x745→0x797/0x79a` группа `0x90` | 19 б. | VIN | `:90`, `:892` |
| то же | `0x1203` / `0x1205` | 2 б. | счётчики QC / L1-L2 | `:91-92`, `:913`, `:978` |

Состояния опроса: `POLLSTATE_OFF/ON/RUNNING/CHARGING` (`:61-67`). Переход в `ON` — только при
`cfg_enable_write` (`:354`), т.е. **без `xnl canwrite true` активного опроса вообще нет**.
При выключении машины опрос останавливается: «Leaf does not respond to polls when car is off» (`:69-70`).

### 1.5 Команды

| Команда | Точка входа | Ограничение | file:line |
|---|---|---|---|
| `xnl lock` / `xnl unlock` | `CommandLock` / `CommandUnlock` | нужен `xnl canwrite` | `:2668`, `:2673`, `:1918` |
| `xnl startcharge` / `xnl stopcharge` | `CommandStartCharge` / `CommandStopCharge` (MITM-патч `0x1db` + CRC) | `xnl canwrite` | `:2712`, `:2705`, `:1009-1017` |
| `xnl climate` | `CommandClimateControl` | `xnl canwrite`; на ZE1 требует отключения TCU (разводка CAN) | `:2657`, `docs/index.rst:205-244` |
| `xnl homelink` | `CommandHomelink` | — | `:2643` |
| `xnl wakeup` | `CommandWakeup` → ZE0 / AZE0 / AZE0_2 по `xnl modelyear` | `xnl canwrite`; ZE0 требует MAX7317 и +12 В на pin 11 TCU | `:2553-2611`, `:2578` |
| Транспорт команд | `SendCommand`: `modelyear ≥ 2016` → CAN2, иначе CAN1, 4/1 байт | `:1916-1930` | — |
| Опрос из консоли | `xnl obd can1/can2 device|broadcast` | `:295-309` | — |

### 1.6 Конфигурация (никто не определяет модель автоматически)

| Параметр | По умолчанию | Роль | file:line |
|---|---|---|---|
| `xnl ze1` | **false** | включает ZE1-специфику (список опросов, CHAdeMO-эвристика, ZE1-температуры) | `:341`, `nl_web.cpp:155` |
| `xnl modelyear` | **2012** | год → формат HVAC-фреймов, шина для команд, выбор пробуждения | `vehicle_nissanleaf.h:47`, `:1274`, `:1342`, `:1366`, `:1925`, `:2557` |
| `xnl canwrite` | **false** | разрешение записи и активного опроса | `:338`, `:1918` |
| `xnl maxGids` / `xnl newCarAh` | `GEN_1_*` | **ёмкость батареи задаётся вручную** (40 кВт·ч: `maxGids 502`, `newCarAh 115`) | `nl_web.cpp:173-183`, `docs/index.rst:254-272` |
| `xnl soc.newcar`, `xnl.soh.newcar` | false | режим пересчёта SOC/SOH «как на новой машине» | `:221`, `:341` |
| `xnl cabintempoffset`, `xnl.speeddivisor`, `xnl.whPerGid`, `xnl.kmPerKWh` | — | подгонка под конкретную машину | `nl_web.cpp` |
| `xnl command.wakeup`, `xnl cfg_ev_request_port` | — | пин MAX7317 для EV SYSTEM ACTIVATION REQUEST | `:2578` |

**VIN не используется для определения года/комплектации** — он только читается в метрику
`v.vin` (`:892-911`). Определение поколения — исключительно ручной конфиг.

---

## 2. Поколения Leaf и точки расхождений в коде

Код размечен на три ветки, но **переключение между ними — не автоматическое**:

| Поколение | Маркеры в коде | Что отличается |
|---|---|---|
| **ZE0** (2011–2015) | `m_ZE0_charger` (`:216`), `BATTERY_TYPE_1_24kWh`, заряд-фрейм `0x380` (`:1139`) | SOH из `0x5bc`, bars в младшей тетради `d[2]`, длительности заряда типов 1/2 (`:1649-1667`), пробуждение через MAX7317 (`:2578`) |
| **AZE0** (2013–2017) | `m_AZE0_charger` (`:218`), `BATTERY_TYPE_2_24kWh` / `BATTERY_TYPE_2_30kWh`, заряд-фрейм `0x390` (`:1180`) | SOH из `0x5b3` (`:1835`), гид-статус из `0x390`, 30 кВт·ч определяется по `mx_gids` |
| **ZE1** (2018+) | `cfg_ze1` (`:341`), список `obdii_polls_ze1` (`:72`), CHAdeMO-патч (`:1068`) | SOC из `0x59e` байт 7 (`:1501-1513`), SOH из опроса группы `0x61` (`:83`), cabin temp 0.5°/bias 40 °C (`:1455-1463`), зарядное состояние **выводится из `0x1db`**, а не из `0x380/0x390` |

Границы в коде, где «год» меняет поведение: `:1274` (≥2013 — империал/метрик-зависимая
логика), `:1342` (<2013), `:1366` (<2016), `:1743` (≥2013), `:1925` (≥2016 — команды на CAN2),
`:2041`, `:2113`, `:2118`, `:2557` (выбор пробуждения).

**Вывод для портирования 2018 года:** три независимых ошибки конфигурации (`ze1` не выставлен,
`modelyear` остался 2012, `canwrite` false) дают три разных режима «почти ничего не работает»,
и ни один из них не диагностируется автопроверкой.

---

## 3. Статус поддержки ZE1 в апстриме и сообществе

### 3.1 Что уже есть в baseline 3.3.006

- «ZE1 initial release» — версия **3.3.005** (2025-07-18), `changes.txt:297-300` (блок релиза начинается на `changes.txt:288`).
- ZE1-список опросов, опрос SOH группы `0x61`, ZE1-приборный SOC `0x59e`, ZE1-детект CHAdeMO
  по `0x1db`, ZE1-температура салона — всё это уже в драйвере.
- Дока `docs/index.rst` целиком описывает установку ZE1 (tap-кабель, шлюз, отключение TCU).

### 3.2 Отзывы сообщества (реальные машины)

| Машина | Что работает | Что не работает | Источник |
|---|---|---|---|
| 2018 ZE1, tap-кабель @CAN-gateway (samr037, 2024-07) | полноценная установка, OVMS за накладкой руля | — | issue #323, коммент `#issuecomment-2227069811` |
| 2017 JDM ZE1, 40 кВт·ч, tap (cods4) | «работает почти идеально» | температура батареи, индикация QC, климат «только пока вилка в розетке» | issue #323 |
| 2020 ZE1 (ehilfer), latest build | замки, климат on/off, общий поток метрик | температура батареи «требует модели-специфичного декода» | issue #323 |
| любой ZE1 **без** tap-кабеля | только опрос при включённом зажигании | всё остальное «из коробки» не работает | issue #1540 |
| — | — | READY не детектится → SOC перестаёт обновляться | issue #1542 |

### 3.3 Чего в локальном baseline НЕТ, но уже есть в upstream master

Это главный источник «готовых» решений — diff baseline↔master
(`C:\Users\SERVIC~1\AppData\Local\Temp\leaf_diff.txt`, 685 строк):

| Фича upstream | Что даёт | Примечание |
|---|---|---|
| `SetCan2Mcp2515Filter()` | аппаратный приёмный фильтр MCP2515 на CAN2 под `cfg_ze1` (0x355, 0x385, 0x5a9, 0x5b9, 0x421, 0x5c5, 0x60d, 0x79a) и под AZE0 | снимает нагрузку CPU; на ZE1-ветке **намеренно отсекает `0x180`/`0x292`** |
| `BAT_12V_CURRENT_PID 0x1183` + `PollReply_12VCurrent` | ток 12 В АКБ (1/256 А), `SetAutoStale(30)`, флаг `v.e.charging12v` | в baseline 12 В определяется грубо по `>12.8 В` (`:2094`) |
| Переработанный `charge_duration` для ZE1 (PR #1530, #1536) | таблицы 25/50/80/100 % для L3/L2/L1, данные из `0x5bc mx=1..16` | в baseline ZE1-строка `if (cfg_ze1 \|\| mx == 21) → BATTERY_TYPE_2` упрощена |
| `enum battery_type { UNKNOWN, TYPE_1, TYPE_2 }` | автоопределение типа батареи по CAN | в baseline enum другой: `1_24kWh/2_24kWh/2_30kWh`, без 40/62 |
| Фикс `0x380` AC voltage: `*0.5f + 70.0f` | корректное напряжение сети | в baseline `*2` (`:1141`), закомментирован `+70` — известный дефект (issue #1525) |
| Комментарий к `0x5a9` | «incorrect on a ZE1 (409 returned when display shows 129 km)» | **подтверждённый баг приборного хода на ZE1** — в baseline фикса нет |
| Блокировка `0x180`/`0x292` (`/* ... */`) | исключает педали из логики READY | связано с issue #1542 |
| Примечания про BUS2 | «BUS2 is off on the ZE1 when car is off or Charging» | объясняет, почему VIN/QC/L1L2 на CAN2 не опрашиваются в `POLLSTATE_OFF/CHARGING` в baseline (`{0,3600,0,0}` и `{0,0,0,3600}`) |
| `Ticker10`-блок 12 В для `cfg_ze1` | автоочистка устаревшего тока 12 В, т.к. CAN2 может уходить в сон | отсутствует в baseline |

---

## 4. GAP-анализ: что нужно сделать для ZE1 2018

Сложность: **S** — правка десятков строк, проверяема на стенде; **M** — логика с состоянием,
нужен стенд/машина; **L** — требует новых данных с автомобиля или изменения архитектуры.

| # | Задача | Сложность | Нужные данные / источник | Основание |
|---|---|---|---|---|
| 1 | Донести патчи upstream master (AC voltage `0x380`, `charge_duration`, `battery_type`, 12 В ток) или смержить master | **S–M** | только git | diff `leaf_diff.txt:268-270`, `:429-515` |
| 2 | Фикс приборного запаса хода `0x5a9` на ZE1 (сейчас 409 при 129 км) | **L** | захват `0x5a9` при известном SOC/дальности на приборке | `leaf_diff.txt:593`, `:1816-1823` |
| 3 | Температура батареи ZE1: разбор 29-байтового ответа группы `0x04` (сейчас парсер читает 14-байтовую раскладку, в коде есть «capture is incomplete») | **M** | полный дамп `0x7bb 64 04` на 40 кВт·ч | `:783-836`, жалобы cods4/ehilfer |
| 4 | Детект READY на ZE1: `vehicle_nissanleaf_car_on(true)` требует `v.e.footbrake > 0`, а `0x292` на ZE1 может не приходить → опрос не стартует | **M** | проверка наличия `0x292`/`0x180` на CAR-CAN ZE1 | `:1897-1902`, issue #1542 |
| 5 | Состояние заряда AC на ZE1 (сейчас только CHAdeMO-эвристика по `0x1db`, `v.c.state` для AC не выводится) | **M** | захват `0x390`/`0x5bf`/`0x74a` при L2-заряде на ZE1 | `:1068-1102`, `:1180` |
| 6 | `xnl ze1` / `modelyear` / ёмкость — автодетект (по VIN из опроса `0x90` или по набору фреймов) | **L** | VIN-декодер Nissan (проверить, что `v.vin` на ZE1 вообще читается) | `:892-911`, `vehicle_nissanleaf.h:47` |
| 7 | Корректная ёмкость 40/62 кВт·ч без ручного `maxGids/newCarAh` | **L** | эталонные значения от Leaf Spy на конкретной машине | `docs/index.rst:262-270` |
| 8 | Отказоустойчивость BUS2 при «спящем» шлюзе (stale-metrics, автоочистка) | **S** | — | upstream `Ticker10`-блок |
| 9 | Удалённый климат на ZE1 без разводки TCU (сейчас обязателен отвод 2 CAN-проводов TCU) | **L** | альтернативный пробуждающий паттерн на CAR-CAN | `docs/index.rst:205-244` |
| 10 | Восстановление `0x180`/`0x292` при активном MCP2515-фильтре (если фильтр будет включён) | **S** | подтверждение, что эти ID на ZE1 есть | `leaf_diff.txt:529-545` |
| 11 | VALET, Homelink-кнопки, внешний аккумулятор/импорт-экспорт (`HandleExporting`) — проверить применимость к ZE1 | **S** | — | `:2076`, `:2086` |
| 12 | Таблица «CAN Bus Mapping» в локальной доке (в baseline её нет, есть только в upstream master) | **S** | upstream `docs/index.rst` | локальный `index.rst` не содержит секции |

---

## 5. План верификации на автомобиле (2018 Leaf 40 кВт·ч)

Порядок обязателен: сначала конфиг, потом кабель, потом калибровка.

**Этап 0 — до подключения (5 мин)**
1. `config set xnl ze1 true`, `config set xnl modelyear 2018`, `config set xnl canwrite true`.
2. `config set xnl maxGids 502`, `config set xnl newCarAh 115` (для 40 кВт·ч).
3. `config set xnl soc.newcar false`, `config set xnl.soh.newcar false` — сначала «сырые» значения.
4. `module status`, `xnl obd can1 device 79b 0201` — проверить, отвечает ли BMS.

**Этап 1 — подключение**
5. Снять минус АКБ, снять приборку, вынуть CAN-gateway, вставить tap-кабель
   (см. `docs/images/Leaf-ZE1-CAN-Tap-Wiring.pdf`, `ze1-can-gateway-module.jpg`).
6. Подключить DB9 к OVMS; проконтролировать 12 В на DB9 pin 9 (`NL.txt`).
7. Поднять питание, `can log`/`ovms` → убедиться, что идут `0x59e`, `0x60d`, `0x5bc`.

**Этап 2 — режимы (метрики сверять с приборкой и Leaf Spy)**
8. **OFF** (ключ вынут): опрос должен стоять в `POLLSTATE_OFF`; проверить stale-метрики.
9. **ACC/ON** (`0x60d` `d[1]>>1&3` = 1/2): `v.e.awake=true`, `v.b.soc` обновляется.
10. **READY** (нажать тормоз до позиции 3): проверить, срабатывает ли `vehicle_nissanleaf_car_on(true)` —
    если нет, это подтверждает GAP #4 (отсутствует `0x292`).
11. **Зарядка L2**: `v.c.state`, `v.c.pilot`, `v.c.current`, `v.c.duration` — сверить с ожиданием.
12. **QC/CHAdeMO**: подтвердить эвристику `:1068-1102` (только при `!car_on`, `U > 200 В`, `I < −25 А`).
13. **Поездка**: `v.p.speed` против спидометра, `v.p.odometer` против одометра, `xnl.v.b.range.instrument` (GAP #2).

**Этап 3 — команды (по одной, после снятия риска)**
14. `xnl lock` / `xnl unlock` при зажигании ACC (CAR-CAN должна быть разбужена — по доке, включив A/C).
15. `xnl stopcharge` в процессе заряда (MITM-патч), затем `xnl startcharge`.
16. `xnl climate` — только после отключения CAN-проводов TCU.
17. `xnl wakeup` — минимально, т.к. пишет в MAX7317/TCU.

**Этап 4 — регрессия**
18. Проверить, что AZE0/ZE0-поведение не изменилось: те же метрики в том же формате
    (побочный эффект любого изменения в общих ветках `IncomingFrameCan1/Can2`).

---

## 6. Риски и открытые вопросы

**Риски**
- Любая правка в `IncomingFrameCan1/Can2` затрагивает все поколения Leaf и e-NV200 — регрессия ловится только на машине.
- Включение MCP2515-фильтра из upstream на самодельном кабеле может отсечь фреймы, если проводка подключена к другой шине gateway (CAR vs EV).
- `xnl canwrite true` даёт OVMS право писать в шину: ошибка в `CommandStopCharge` (перезапись `0x1db` с CRC) теоретически может вызвать сбой заряда.
- Разводка/обрезка CAN у TCU генерирует коды ошибок (`ze1-error-codes-after-tcu-disconnected.jpg`) и отключает Nissan Connect.

**Открытые вопросы**
1. Присутствуют ли на CAR-CAN ZE1 фреймы `0x180` (газ) и `0x292` (тормоз)? От этого зависит и READY (#4), и выбор фильтра (#10).
2. Какой фрейм несёт состояние AC-заряда на ZE1 — продолжает ли работать `0x390`, либо это уже `0x5bf`/опрос `0x74a`?
3. Почему `0x5a9` на ZE1 даёт 409 при 129 км — единица измерения, сдвиг или другой масштаб?
4. Читаются ли 29 байт группы `0x04` на 40 кВт·ч полностью и какова раскладка 6 температур (есть ли 6-я, отдельный NTC)?
5. Можно ли определить модель/год/ёмкость из VIN, опрашиваемого по `0x90`, без ручного `xnl modelyear`?
6. Будет ли upstream-ветка с `SetCan2Mcp2515Filter` доступна в следующем релизе, и мержить ли её до или после собственных правок?
7. Откуда в baseline берётся `+70 В` оффсет AC-напряжения и почему он закомментирован — дефект расчёта или особенность конкретной прошивки VCM?
8. Что требуется, чтобы удалённый климат работал на ZE1 без физического отвода проводов TCU?

---

## 7. Источники

**Локальный код**
- `vehicle/OVMS.V3/components/vehicle_nissanleaf/src/vehicle_nissanleaf.cpp` (2821 строка)
- `vehicle/OVMS.V3/components/vehicle_nissanleaf/src/vehicle_nissanleaf.h`
- `vehicle/OVMS.V3/components/vehicle_nissanleaf/src/nl_web.cpp`, `nl_types.h`
- `vehicle/OVMS.V3/components/vehicle_nissanleaf/docs/index.rst`, `docs/images/*`
- `vehicle/OVMS.V3/changes.txt` (3.3.006 = `:6`; 3.3.005 = `:288`; «ZE1 initial release» = `:297-300`)
- `vehicle/hardware/Electrical/NL.txt`

**GitHub (upstream `openvehicles/Open-Vehicle-Monitoring-System-3`)**
- https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/issues/323
- https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/issues/323#issuecomment-2227069811
- https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/issues/1540
- https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/issues/1542
- https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/issues/1525
- https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/pull/1382
- https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/pull/1386
- https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/pull/1484
- https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/pull/1530
- https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/pull/1536
- https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/pull/1119
- https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/pull/1122

**Внешние**
- https://nissanleaf.carhackingwiki.com/index.php/CAN_Buses
- https://nissanleaf.carhackingwiki.com/index.php/M101_(6CH_CAN_Gateway)
- https://nissanleaf.carhackingwiki.com/index.php/Telematics_Control_Unit_(TCU)
- https://docs.openvehicles.com/en/latest/
- http://www.mynissanleaf.com/viewtopic.php?f=37&t=32935
- https://github.com/dalathegreat/leaf_can_bus_messages

**Сохранённые артефакты исследования**
- `C:\Users\ServiceCenter\.local\share\opencode\tool-output\tool_111588152001SC8F1CXZ0AFiVG` — выгрузка issue #323
- `C:\Users\SERVIC~1\AppData\Local\Temp\upstream_leaf.cpp` — upstream master `vehicle_nissanleaf.cpp`
- `C:\Users\SERVIC~1\AppData\Local\Temp\leaf_diff.txt` — diff master↔baseline (685 строк)

**Ограничения исследования**
- PDF с проводкой (`Leaf-ZE1-CAN-Tap-Wiring.pdf`) — растровый, текстовый слой отсутствует; извлечение текста невозможно, используется только как чертёж-приложение.
- Форум `forum.openvehicles.com` недоступен из этой среды; `mcp.exa.ai` возвращает 403.
- Сравнение с upstream выполнено на уровне одного файла драйвера (не вся ветка master).
