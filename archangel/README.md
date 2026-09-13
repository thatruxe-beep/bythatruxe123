# Archangel — MTA:SA (rebuild из DLL.dll.c)

Готовая 32-битная DLL (`out/DLL.dll`), восстановленная по декомпилированному
коду `DLL.dll.c` (оригинальный декомпилированный дамп лежит в корне репозитория).

## Что за оригинал (по результатам анализа дампа)
- Чит для **MTA:SA**: ImGui-меню (оригинал на Dear ImGui 1.92.5), функция
  **Wallhack** (ESP), инжект своего Lua-скрипта в MTA через
  `lua51.dll` (`GetModuleHandleA("lua51.dll") + GetProcAddress("luaL_loadbuffer")` —
  это видно в дампе, ~строка 16570),
- Загрузка ключа из `<dir игры>\Archangel\Key.txt`, проверка ключа по
  `check_key.php` через WinHttp (User-Agent `MTA-Tool/1.0`),
- `keybinds.cfg` — сохранение клавиш,
- Хук Present в d3d9.dll: сигнатурный поиск Present внутри образа d3d9.dll
  (ориентир `6C 07 … 89 86 … 89 86`), поиск vtable устройства, патч слота 17
  с остановкой остальных потоков процесса (`CreateToolhelp32Snapshot` +
  `SuspendThread/ResumeThread` — это функция `FUN_10003d30` в дампе),
- Инжектором в игру DLL вставляется вручную: `Discord.exe` в репозитории —
  декомпилированный manual-map инжектор (загружает образ DLL и вызывает его
  DllMain в процессе MTA).
- **Telegram-бот в этой сборке удалён** (по просьбе).

## Что восстановлено, а что нет
Восстановлено (архитектура 1:1, код переписан чистым C++):
- DllMain + worker-поток (manual-map безопасный: без CRT-инициализации),
- сигнатурный поиск Present в d3d9 + поиск vtable + поиск объекта устройства
  (по первому DWORD = указатель на vtable) + патч с остановкой потоков,
- ImGui-оверлей (меню: Features / Keybinds / Settings / About),
- ESP: Lua собирает игроков (`getScreenFromWorldPosition`,
  `getDistanceBetweenPoints3D`, `getPedHealth`), C++ рисует боксы/имена/дистанцию/хп,
- Lua-мост: поиск состояния MTA (см. ниже), регистрация C-функций
  `ar_frameBegin / ar_pushPlayer / ar_state`, загрузка payload через
  `luaL_loadbuffer + lua_pcall`,
- ключ: чтение `Archangel\Key.txt`, проверка `check_key.php` (WinHttp,
  User-Agent как в оригинале),
- `keybinds.cfg` / `settings.cfg` в `<dir игры>\Archangel\`.

Невозможно восстановить из дампа (данные из .rdata оригинала не входят в
декомпиляцию) — заменено конфигурируемым эквивалентом:
- **16 КБ встроенный Lua-скрипт оригинала** (`DAT_100ae610`) — восстановлен
  заново: `payload/archangel.lua` (фичи: god/speed/health/armor/noclip +
  сборка данных для ESP + toggle-клавиша как в оригинальном фрагменте
  `startTaran` / `_taran_enabled`),
- **офсет LuaState в MTA** — ищется автоматически (скан образа game.exe по
  TMS: серия указателей внутрь lua51.dll → lua_State). Для точного попадания
  под вашу версию MTA можно указать офсет вручную:
  `settings.cfg`: `luastateoffset = 0x...` (от базы game.exe),
- **URL проверки ключа** (хост из .rdata не читается) — указать в
  `settings.cfg`: `keycheckurl = https://host/check_key.php`
  (ответ начинается с `1` = ключ валиден). Без URL — локальный режим
  (ключ из Key.txt просто читается, сеть не ходит).

## Использование
1. Киньте `DLL.dll` туда, где лежит инжектор, и вставьте в MTA **вашим
   инжектором** (Discord.exe / любой manual-map инжектор).
2. Играйте в **оконном режиме** (окно без рамки можно).
3. Меню: по умолчанию **INSERT** (клавиша меняется в `Archangel\keybinds.cfg`:
   `menu=45`).
4. F8 — вкл/выкл чит (Lua-side toggle, как в оригинале; имя клавиши
   `tarankey` в `settings.cfg`).
5. Лог: `Archangel\debug.log` (статусы: d3d9 hook, lua, key).

Конфиги создаются в `<папка игры>\Archangel\`:
```
Archangel\
  Key.txt         <- ваш ключ (одной строкой)
  keybinds.cfg    <- menu=45, god=0, speed=0, ...
  settings.cfg    <- keycheckurl = ..., luastateoffset = 0x..., tarankey = f8
  debug.log       <- лог
```

## Сборка
### Быстро (Linux/Windows, нужен только Zig)
```
pip install ziglang
sh build.sh /path/to/zig      # -> out/DLL.dll (32-bit)
```
### MSVC (Windows)
```
build.bat                    # из x86 Native Tools Command Prompt -> out\DLL.dll
```
### CMake (любая тройка)
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build          # для x86 используйте x86-тулчейн MSVC
```

## Замечания
- DLL намеренно не экспортирует ничего и не зависит от CRT-инициализации —
  она рассчитана на manual-map инжектирование (как оригинал).
- Если версия MTA изменила сигнатуру d3d9 или структуру LuaState — смотрите
  `debug.log`: хук/лоуа ищутся с ретраями ~2–4 минуты после инжекта.
- Это учебная реконструкция для вашей копии кода; используйте только на
  серверах, где читы разрешены.
