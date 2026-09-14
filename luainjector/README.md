# Archangel Lua Injector (только Lua-инжекция)

Минимальный 32-bit DLL: **только** lua-инжектор, без меню, без d3d9-хука,
без фичей. Инжектится как обычный DLL (например, через Injector.exe из
папки `../injector`, поле "DLL file" -> `luainjector.dll`).

## Что делает
1. После инжекта поднимает worker-поток;
2. ждёт загрузку `lua51.dll` (MTA) до ~2 минут;
3. находит LuaState игры (автоскан TMS, либо принудительный сдвиг —
   `Archangel\settings.cfg`: `luastateoffset=0x...`);
4. регистрирует в Lua функцию `luainj_log(msg)` (пишет в
   `Archangel\luainj.log`) и запускает встроенный пейлоад
   (`payload/script.lua`);
5. встроенный пейлоад ищет **твой** скрипт `Archangel\script.lua`
   (CWD = директория игры) и выполняет его; ошибки пишутся в лог.

## Файлы
- `Archangel\luainj.log` — лог (появляется после инжекта);
- `Archangel\script.lua` — твой lua (опционально);
- `Archangel\settings.cfg` — опц. `luastateoffset=` (hex).

## Сборка
`sh build.sh` — Zig cross (x86-windows-gnu), ImGui не нужна.
