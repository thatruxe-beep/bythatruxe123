// DllMain + worker thread.
// NOTE: this DLL is manual-mapped into the MTA process by the loader
// (Discord.exe) - the CRT start-up does NOT run, so DllMain must stay
// trivial (no CRT globals / no exceptions). The worker only uses Win32
// APIs + stdio (stream state lives in the CRT dll, safe to call).
#include "archangel.h"
#include <stdio.h>

static DWORD WINAPI Worker(LPVOID) {
    Sleep(3000);                        // let injection & game settle
    ArchLog("=== Archangel worker started ===");

    Config_Init();

    // original behavior on injection: "Authorization" dialog (FUN_10002c00)
    // - message line, EDIT pre-filled with the key from Key.txt, OK/Cancel
    int auth = ShowAuthorizationDialog(g_cfg.key, (int)sizeof(g_cfg.key), "Enter key:");
    if (auth == 1) {
        Config_SaveKey();   // store entered key back to Key.txt
        ArchLog("worker: authorization OK");
    } else {
        g_cfg.key[0] = 0;   // original clears the key on cancel
        ArchLog("worker: authorization cancelled - no key");
    }

    int krc = KeyCheck_Run();
    g_keyValid = (krc == 1);
    if (krc == 0) ArchLog("worker: key REJECTED by server");

    DX9_Init();                          // retries internally (~4 min)
    LUA_Init();                          // retries internally (~2 min)

    // keep-alive: re-check key periodically (soft: menu shows state, no kill)
    int ticks = 0;
    for (;;) {
        Sleep(1000);
        if (g_cfg.keyCheckUrl[0] && (++ticks % 600) == 0) {
            int r = KeyCheck_Run();
            g_keyValid = (r == 1);
            ArchLog("worker: periodic key check -> %d", r);
        }
    }
    return 0;
}

BOOL APIENTRY DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
    (void)hinst; (void)reserved;
    static volatile LONG started = 0;
    if (reason == DLL_PROCESS_ATTACH) {
        if (InterlockedCompareExchange(&started, 1, 0) == 0)
            CreateThread(NULL, 0, Worker, NULL, 0, NULL);
    }
    return TRUE;
}
