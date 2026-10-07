# Сервер openvehicles.com: модуль ↔ сервер ↔ приложение (OVMS3 v3.3.006)

Задача: заставить модуль OVMS3 (LILYGO T-2CAN) работать с **оригинальным** сервером
`https://www.openvehicles.com/` и официальным приложением OVMS. Только исследование/документация,
код в этой работе не менялся.

---

## 1. Как работает связь модуль ↔ сервер (V2 MP)

### Кто что реализует в этом репо

| Элемент | Файл |
|---|---|
| Клиент V2 (MP): логин, крипто, телеметрия | `vehicle/OVMS.V3/components/ovms_server_v2/src/ovms_server_v2.cpp` (+ `.h`) |
| База клиентов (`OvmsServer : pcp`) | `vehicle/OVMS.V3/components/ovms_server/src/ovms_server.cpp` — пустышка, только power mode |
| Веб-форма конфига V2 | `vehicle/OVMS.V3/components/ovms_webserver/src/web_cfg_server_v2.cpp` (`/cfg/server/v2`) |
| Мастер первой настройки (шаг 4 «vehicle & server») | `vehicle/OVMS.V3/components/ovms_webserver/src/web_cfg_init.cpp` (`CfgInit4`, ~880–990) |
| Автостарт | `vehicle/OVMS.V3/components/ovms_webserver/src/web_cfg_autostart.cpp` |
| Доверенные TLS-CA | `vehicle/OVMS.V3/components/ovms_tls/src/ovms_tls.cpp` |

Компонент `ovms_server` — **не** mp-реализация (в `ovms_server.h` только `OvmsServer : pcp`);
весь MP-протокол живёт в `ovms_server_v2.*`.

### Адрес, порты, TLS

- Хост: `server.v2/server`. В веб-форме прямо перечислены публичные серверы:
  `api.openvehicles.com` (APAC) и `ovms.dexters-web.de` (EU) — `web_cfg_server_v2.cpp:127-128`.
- Порт: `server.v2/port`; если пусто — код подставляет **6867 без TLS / 6870 с TLS**
  (`ovms_server_v2.cpp:871`).
- Порты официального сервера (плагин `ApiV2`, `docs/source/server/plugins.rst:61-63` и
  `docs/source/protocol_httpapi/format.rst`):
  - **tcp/6867** — сырой v2 MP (без TLS),
  - **tcp/6870** — v2 MP поверх SSL (нужен `conf/ovms_server.pem` на сервере),
  - **tcp/6868 / tcp/6869** — HTTP/HTTPS REST API (для приложений/интеграций, не для модуля).
- TLS: `server.v2/tls` (bool). Клиент — mongoose `mg_connect_opt` с
  `opts.ssl_ca_cert = MyOvmsTLS.GetTrustedList()` и SNI = хост (`ovms_server_v2.cpp:913-923`).
  Встроенные корневые CA: UserTrust, DigiCert Global/G2, Starfield, Baltimore,
  **ISRG X1 (Let's Encrypt)**, Amazon Root CA1 (`components/ovms_tls/CMakeLists.txt:8`),
  плюс пользовательские из `/store/trustedca`. SSL в клиенте компилируется только при
  `CONFIG_MG_ENABLE_SSL=y` — см. риск №1 (§6).

### Схема аутентификации (что реально шлёт модуль)

1. Модуль шлёт (`SendLogin()`, `ovms_server_v2.cpp:977-1010`):

   ```
   MP-C 0 <token> <base64(hmac-md5(token, password/server.v2))> <vehicle/id>\r\n
   ```

   То есть **protection scheme 0x30** (общий секрет = *server/vehicle password*),
   префикс `MP-C` = «car».
2. Сервер отвечает `MP-S 0 <token> <digest>`; модуль проверяет digest и что токены не совпадают
   (защита от replay) — `ProcessServerMsg()`, `ovms_server_v2.cpp:248-277`.
3. Далее ключ RC4 = `hmac-md5(server_token + client_token, password)`, пропускается 1024 байта
   шифротекста в каждую сторону; всё общение — RC4 + base64, строки через CR+LF
   (`encryption_scheme_0x30.rst`; код `ovms_server_v2.cpp:279-294`, `:342-349`).
4. Схема **0x31** (username + password / API-token, без RC4, рассчитана на TLS) описана в
   `encryption_scheme_0x31.rst`; сервер её умеет, **но модуль — нет**: строка
   `strcat(hello,"MP-C 0 ")` жёстко зашита (`ovms_server_v2.cpp:999`). С модуля нельзя
   логиниться логином аккаунта openvehicles.com — только паролем машины.
5. `server.v2/paranoid=yes` — доп. «paranoid»-туннель: `MP-0 ET<ptoken>`, ключ =
   `hmac-md5(ptoken, password/module)`, то есть задействуется **module password**
   (`ovms_server_v2.cpp:296-319`). По умолчанию выключен.
6. Приложение логинится тем же способом, но с префиксом **MP-A** (`startup.rst:14`);
   batch-клиенты — `MP-B`, серверы — `MP-S`. Ответ сервера в 0x31:
   `MP-S 1 <user> <vehicleid> <список остальных машин>`.
7. Команды приложения (`MP-0 C…`) исполняются модулем в **secure-режиме**
   (`bs->SetSecure(true); // this is an authorized channel`, `ovms_server_v2.cpp:537`) —
   владелец *server password* получает выполнение shell-команд на модуле.

### Auto provisioning (AP-C / AP-S)

- Механизм описан в `docs/source/protocol_v2/auto_provisioning.rst`: модуль шлёт
  `AP-C <scheme> <apkey>`, apkey обычно **VIN**, секрет подтверждения обычно **ICCID SIM**;
  сервер отвечает `AP-S` с зашифрованным (RC4 + base64) блоком параметров либо `AP-X`.
- **В прошивке V3 это не реализовано**: поиск `AP-C|AP-S|auto.?provision` по
  `vehicle/OVMS.V3/**/*.cpp` даёт 0 совпадений. Практический вывод: **vehicle id и пароль
  задаются только вручную** (веб/CLI), автоматически модуль идентичность не получает.

### Статусы и события

Состояния `WaitNetwork → ConnectWait → Connecting → Authenticating → Connected / Disconnected /
WaitReconnect`, события `server.v2.waitnetwork`, `.connectwait`, `.connecting`, `.authenticating`,
`.connected`, `.disconnected`, `.waitreconnect`, `.stopped`
(`ovms_server_v2.cpp:830-860`, `:2332`). Ошибка аутентификации:
`Authentication error (wrong ID/password)` → повтор через 120 с (`:216-221`).

---

## 2. Регистрация / активация модуля на openvehicles.com

1. **Пользовательский аккаунт**: `https://www.openvehicles.com/user/register` — поля
   *Username*, *E-mail*, *Vehicle Type*, часовой пояс + CAPTCHA. Покупка модуля, серийный номер
   или активационный ключ **в форме не требуются**.
2. Подтвердить e-mail, войти: `https://www.openvehicles.com/user/login`.
3. **Внутри аккаунта создать vehicle account** — «You will need to create a user account first.
   Within your user account you then need to create a vehicle account. You'll need to pick a
   unique vehicle ID» (`docs/source/userguide/installation.rst:56-62`).
   - Vehicle ID уникальный; по FAQ сервер принимает **только заглавные буквы и цифры**
     (`https://www.openvehicles.com/helpmev2`, п. «VehicleID»), тогда как веб-форма модуля
     допускает буквы/цифры/`-` (`web_cfg_server_v2.cpp:66`) — **дефис не используйте**,
     иначе модуль залогинится с ID, которого нет в базе сервера.
   - Задать **vehicle (server) password** — «a shared secret between the OVMS server, your car,
     and your smartphone App» (helpmev2, п. «OVMS Server Password»). В документации модуля:
     «vehicle password (aka *server password* — as entered on the server when you registered
     your vehicle)» (`installation.rst:127`).
4. Отдельной операции «выпуска модуля» **нет**: активация = первое успешное соединение
   `MP-C 0 …` с корректными `vehicle/id` + `password/server.v2`. Запись vehicle в БД сервера
   должна существовать, иначе сервер отклонит digest (в модуле — ошибка аутентификации).
5. Запасной публичный сервер (Европа): `https://dexters-web.de/` (`installation.rst:60`).
6. Ограничения на сторонние модули отсутствуют: «Everything is open, and APIs are public.
   **Other car modules can talk to the server**, and other Apps can show the status and control
   the car» (`docs/source/introduction.rst`). Сервер проверяет только vehicle ID + пароль —
   ни серийника, ни токена модуля, ни подтверждения покупки.
7. Логин/пароль аккаунта используются **не для модуля**, а для HTTP API
   (`https://api.openvehicles.com:6869/api/token`), веб-панели и, возможно, секции
   «www.openvehicles.com» в настройках приложения.

---

## 3. Конфигурация модуля

### Таблица ключей

| Параметр | Инстанс | Назначение | По умолчанию / заметки |
|---|---|---|---|
| `server.v2` | `server` | хост V2-сервера | пусто → ошибка `Parameter server.v2/server must be defined` |
| `server.v2` | `tls` | включить TLS | `no`; при `yes` нужен `CONFIG_MG_ENABLE_SSL=y` |
| `server.v2` | `port` | порт | пусто → `6867` (no TLS) / `6870` (TLS) |
| `password` | `server.v2` | **server (vehicle) password** — общий секрет модуль↔сервер↔приложение | protected; пусто → ошибка `server.v2/password must be defined`; раньше был `server.v2/password`, мигрируется (`ovms_config.cpp:621-628`) |
| `vehicle` | `id` | Vehicle ID в логине MP-C | пусто → ошибка `vehicle/id must be defined` |
| `server.v2` | `updatetime.connected` | интервал пуша при подключённом приложении | `60` с |
| `server.v2` | `updatetime.idle` | интервал пуша без приложений | `600` с |
| `server.v2` | `timeout.rx` | rx-таймаут | `960` с |
| `server.v2` | `paranoid` | paranoid-шифрование (ключ — module password) | `no` |
| `server.v2` | `workaround.ios_tpms_display` | хак для iOS App 1.8.6 | `yes` |
| `password` | `module` | **module password**: веб/SSH/USB-консоль, paranoid | protected; после factory reset пусто |
| `auto` | `server.v2` | автостарт V2-клиента | `no` |
| `auto` | `vehicle.type` | тип машины (драйвер vehicle) | — |
| `auto` | `init` | уровень автостарта `yes/minimal/no` | `yes` |
| `auto` | `modem` | автостарт сотового модема | `no` |
| `auto` | `wifi.mode` | `ap/client/apclient` | `ap` |
| `wifi.ssid` | `<ssid>` | пароль Wi-Fi сети (protected) | — |
| `modem` | `apn`, `apn.user`, `apn.password` | APN для канала данных | — |
| `network` | `dns` | DNS (бывший `PARAM_GPRSDNS`) | — |
| `auto` | `server.v3` | автостарт MQTT (V3) — для справки | `no` |

Маппинг старых «номеров параметров» V2 (`pmap[]`, `ovms_server_v2.cpp:127-158`):
`PARAM_SERVERIP → server.v2/server`, `PARAM_SERVERPASS → password/server.v2`,
`PARAM_MODULEPASS → password/module`, `PARAM_VEHICLEID → vehicle/id`,
`PARAM_GPRSAPN → modem/apn`, `PARAM_GPRSUSER/PASS → modem/apn.user/.password`,
`PARAM_GPRSDNS → network/dns`.

### Где хранится пароль

Конфиг — защищённый стор на флеше: `#define OVMS_CONFIGPATH "/store/ovms_config"`
(`main/ovms_config.cpp:54`), файлы `<param>/<instance>` (`ovms_config.h:102`). Секции
`password/*` и `wifi.ssid/*` объявлены **protected** — значения не читаются даже из CLI
(видны только имена инстансов). «My Tree» как хранилища в OVMS **нет**: module password лежит
в `/store/ovms_config/password` (инстанс `module`), vehicle password — там же
(инстанс `server.v2`). Смена: веб `Config → Password` (`web_cfg.cpp:288-304`) либо
`config set password module <новый>`.

### Shell-команды (порядок действий)

```
# 1) сеть (Wi-Fi клиент; AP оставляем для настройки)
config set wifi.ssid MYSSID mypassword
config set auto wifi.mode client

# 2) идентичность и сервер
config set vehicle id MYCAR                       # тот же ID, что создан на сайте
config set server.v2 server api.openvehicles.com
config set server.v2 tls yes                      # будет порт 6870
# config set server.v2 port 6870                  # опционально
config set password server.v2 <VehiclePassword>   # пароль машины с сайта
config set password module <ModulePassword>       # пароль самого модуля (веб/SSH/консоль)

# 3) автостарт
config set auto vehicle.type <тип>
config set auto server.v2 yes
config set auto modem yes                         # только если нужен сотовый канал
config set modem apn <apn>                        # при работе через модем
config set auto init yes

# 4) запуск и проверка
server v2 start
server v2 status
module status                                     # метрика s.v2.connected = yes
```

Веб-эквиваленты: `Config → Server V2` (`/cfg/server/v2`), `Config → Autostart`,
`Config → Password`, либо мастер при первом подключении к AP `OVMS`/`OVMSinit` →
`http://192.168.4.1/` (`installation.rst:75-139`).

### Ловушки мастера

- В `CfgInit4` при пустом поле пароля **подставляется module password**
  (`web_cfg_init.cpp:909-910`) — легко получить «одинаковые» vehicle и module password.
- Мастер принудительно ставит `server.v2/tls = yes` (`:938`) и хост по умолчанию
  `api.openvehicles.com` (`:969-975`); после успешного теста включает `auto/server.v2` (`:891-892`).
- В вебе поле подписано явно: «enter the password for the **vehicle ID**, *not* your user account
  password» (`web_cfg_server_v2.cpp:135-137`).

---

## 4. Подключение приложения (Android / iOS)

1. Настройки приложения → сервер: **`api.openvehicles.com`**, порт по умолчанию
   (6867 plain / **6870 TLS**); для EU-сервера — `ovms.dexters-web.de`, порт 6867.
2. Секция «www.openvehicles.com» в настройках: логин/пароль **учётной записи сервера**
   (HTTP API, уведомления) — не путать с vehicle password.
3. В приложении `Settings → New vehicle` заполнить (по Mark Webb-Johnson, тема node/2406):
   - **Vehicle ID** — тот же, что в модуле и на сайте;
   - **Vehicle Label** — произвольный ник;
   - **OVMS Server Password** = `password/server.v2` модуля;
   - **OVMS Module Password** = `password/module` (нужен для привилегированных операций).
4. Порядок: сайт (аккаунт + vehicle) → модуль (`vehicle/id`, `server.v2/*`,
   `password/server.v2`, `auto server.v2`) → приложение. Контроль: `server v2 status` →
   `Connected`, метрики появляются в приложении.
5. Интервалы данных: 60 с при подключённом приложении, 600 с в простое (`updatetime.*`);
   типовой расход V2 — 1–3 МБ/мес (`docs/source/userguide/components.rst`).
6. Классическому приложению нужен именно канал **V2 (MP)**; MQTT/V3 оно не использует.

---

## 5. Зависимости от модема / сети

- V2-клиент стартует только при наличии сети: подписки на `network.up`, `network.down`,
  `network.reconfigured`, `network.mgr.init/stop`, стартовое состояние `WaitNetwork` до
  `MyNetManager.m_connected_any` (`ovms_server_v2.cpp:2094-2145`, `:2299-2303`).
  То есть нужен любой поднятый IP-стек: Wi-Fi client, AP, Ethernet **или** сотовый PPP.
- Сотовый канал: компонент `ovms_cellular` (`RegisterParam("modem", …)`, автостарт по
  `auto/modem`), драйверы в `components/simcom` — для нашего T-2CAN это
  `simcom_7670.cpp` («Experimental support for SIMCOM A7670E», регистрация драйвера 4660).
  Нужны `modem/apn` (+ при необходимости `apn.user`/`apn.password`), СИМ-карта с балансом
  и корректный `auto/wifi.mode`. PPP поднимает драйвер модема, после чего срабатывает
  `network.up` и V2 уходит в `ConnectWait`.
- TLS требует корректного системного времени (сертификат сервера) и корневых CA —
  ISRG X1 встроен, остальное кладётся в `/store/trustedca` (см. `userguide/ssltls.rst`).
- **MQTT/V3** (`components/ovms_server_v3`, ключи `server.v3/server|user|port|clientid|topic.prefix`)
  — отдельный, необязательный канал: `auto/server.v3` по умолчанию `no`. Классическому
  приложению OVMS он не нужен; docs отмечают, что MQTT жрёт заметно больше трафика, чем MP V2
  (`userguide/homeassistant.rst`, `userguide/components.rst` — «production users should use
  OVMS Server v2 protocol»).
- Независимость: сервер V2 не зависит от того, как поднята сеть — работает и по Wi-Fi, и по
  модему; падение сети роняет соединение (`network.down` → `WaitNetwork`) и клиент сам
  переподключается (10 с / 60 с / 120 с в зависимости от причины).

---

## 6. Риски и открытые вопросы

1. **TLS может быть выключен в нашей сборке.** В активном `vehicle/OVMS.V3/sdkconfig` ключа
   `CONFIG_MG_ENABLE_SSL` нет вообще, а `CONFIG_MBEDTLS_PSK_MODES is not set` — то есть
   `MG_ENABLE_SSL` (depends on `MBEDTLS_PSK_MODES`, `main/Kconfig:241`) даже не выбирается.
   При этом `server.v2/tls=yes` в коде даст
   `Error: Connection failed (SSL support disabled)` (`ovms_server_v2.cpp:915-922`).
   В эталонном `support/sdkconfig.lilygo_tc` включено:
   `CONFIG_MBEDTLS_PSK_MODES=y`, `CONFIG_MG_ENABLE_SSL=y`, `CONFIG_MG_SSL_IF_MBEDTLS=y`.
   **Действие: перед тестами выровнять sdkconfig** (иначе придётся использовать незащищённый
   порт 6867, что для публичного сервера нежелательно).
2. **vehicle password = полный shell доступ** к модулю через сервер
   (`ProcessCommand` → `SetSecure(true)`, `ovms_server_v2.cpp:537`). Пароль машины должен быть
   длинным и уникальным; не совпадать с module password.
3. **Расхождение синтаксиса Vehicle ID**: сервер (FAQ) — только `[A-Z0-9]`, веб-форма модуля —
   `[A-Za-z0-9-]`. Проверить фактическую валидацию на стороне сайта нельзя без входа в аккаунт.
4. **Не подтверждено (нужен вход/тест)**: точный UI-путь создания vehicle на openvehicles.com
   (пункты меню после логина), лимит машин на аккаунт, обязательность подтверждения e-mail,
   наличие rate-limit/блокировок «не своих» модулей, реальная открытость портов
   6867/6868/6869/6870 снаружи (в репо описан только конфиг плагинов сервера).
5. **Auto provisioning не реализован в V3** — нельзя получить vehicle id/пароль автоматически
   по VIN/ICCID (см. §1), только ручной ввод.
6. **Схема 0x31 модулем не поддерживается** (зашит `MP-C 0`) — логин «username+password
   аккаунта» с модуля невозможен; это же значит, что смена vehicle password на сайте требует
   синхронной правки `password/server.v2` в модуле.
7. **Нет живого соединения с api.openvehicles.com** в рамках этой работы (исследование, без
   правок кода) — фактический handshake `MP-C 0 … / MP-S 0 …` и работу приложения предстоит
   проверить на стенде T-2CAN.
8. Прочее: мастер по умолчанию подставляет module password как vehicle password (§3),
   а `updatetime.*`/`timeout.rx` лучше не трогать до первого успешного коннекта.

---

## 7. Источники

### Локальные (в репо)

- `docs/source/protocol_v2/index.rst`, `startup.rst`, `encryption_scheme_0x30.rst`,
  `encryption_scheme_0x31.rst`, `auto_provisioning.rst`, `backwards_compatibility.rst`,
  `messages.rst`, `commands.rst`, `terms.rst`, `welcome.rst`
- `docs/source/server/index.rst`, `docs/source/server/plugins.rst`,
  `docs/source/server/installation.rst`
- `docs/source/introduction.rst`, `docs/source/userguide/installation.rst` (строки 52-139),
  `docs/source/userguide/configuration.rst`, `docs/source/userguide/commands.rst`,
  `docs/source/userguide/ssltls.rst`, `docs/source/userguide/components.rst`,
  `docs/source/userguide/homeassistant.rst`, `docs/source/protocol_httpapi/format.rst`
- Код: `vehicle/OVMS.V3/components/ovms_server_v2/src/ovms_server_v2.cpp`,
  `components/ovms_server/src/ovms_server.{h,cpp}`,
  `components/ovms_webserver/src/web_cfg_server_v2.cpp`, `web_cfg_init.cpp`,
  `web_cfg_autostart.cpp`, `web_cfg.cpp`,
  `components/ovms_tls/src/ovms_tls.cpp`, `components/ovms_cellular/src/ovms_cellular.cpp`,
  `components/simcom/src/simcom_7670.cpp`, `components/ovms_server_v3/src/ovms_server_v3.cpp`,
  `main/ovms_config.cpp`, `main/ovms_command.cpp`, `main/Kconfig`,
  `vehicle/OVMS.V3/sdkconfig`, `vehicle/OVMS.V3/support/sdkconfig.lilygo_tc`
- `porting/TEAM.md` (описание этой задачи)

### Веб

- Регистрация: https://www.openvehicles.com/user/register
- Вход: https://www.openvehicles.com/user/login
- Поддержка / FAQ (VehicleID, Module Password, Server Password): https://www.openvehicles.com/helpmev2
- Пользователи, «create an account … and register an open vehicle»: https://www.openvehicles.com/users
- Форум, тема «Connecting iPhone app» (поля приложения, порт 6867/6870, markwj #2):
  https://www.openvehicles.com/node/2406
- Форум, тема «Trying to fetch API token» (api.openvehicles.com:6869, 401):
  https://www.openvehicles.com/node/4514
- Раздел форума «Server v2»: https://www.openvehicles.com/forum/21
- Документация (online): https://docs.openvehicles.com/en/latest/
  - Installation / OVMS Server account: https://docs.openvehicles.com/en/latest/userguide/installation.html
  - Components / OVMS Server v2, v3: https://docs.openvehicles.com/en/latest/userguide/components.html
  - Protocol v2: https://docs.openvehicles.com/en/latest/protocol_v2/index.html
  - OVMS Server (плагины, порты): https://docs.openvehicles.com/en/latest/server/index.html
- Исходники сервера: https://github.com/openvehicles/Open-Vehicle-Server
- Приложения: https://github.com/openvehicles/Open-Vehicle-Android ,
  https://github.com/openvehicles/Open-Vehicle-iOS
- Статус сервисов: https://status.openvehicles.com/
- Альтернативный публичный сервер: https://dexters-web.de/

