# Archangel Injector

Самостоятельный 32-bit Windows инжектор для `DLL.dll` (чит Archangel MTA).
Тот же стиль, что у меню чита: тёмный + красный ImGui (ProggyClean 13/15/22 px).

## Использование
1. Положи `Injector.exe` и `DLL.dll` в одну папку (путь к DLL можно
   переопределить в меню или в `Injector.cfg`).
2. Запусти игру/MTA (`gta_sa.exe` — имя целевого процесса меняется в меню).
3. Нажми **Insert** — откроется меню инжектора:
   - список запущенных процессов (обновляется сам, клик — выбор);
   - **Inject** — VirtualAllocEx + WriteProcessMemory + CreateRemoteThread(LoadLibraryA);
   - **Kill** — завершить выбранный процесс;
   - **Open game** — запустить игру из указанной директории;
   - **Save config** — сохранить `Injector.cfg`; **Exit** — полностью закрыть.
4. В игре чит работает как раньше: меню на **Delete**.

`Injector.cfg` (рядом с exe):
```
target=gta_sa.exe
dll=DLL.dll
gamedir=C:\Games\GTA San Andreas
```

## Сборка
- `sh build.sh` — Zig cross-компилятор (x86-windows-gnu, GUI-subsystem).
- нужен `../archangel/imgui` (ту же ImGui 1.93 WIP использует чит).
