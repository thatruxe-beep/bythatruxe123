// Archangel MTA cheat - rebuilt from decompiled DLL.dll.c
// Shared definitions
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>

#define ARCHANGEL_VERSION "1.0"
#define ARCHANGEL_AUTHOR  "Taran"
#define ARCHANGEL_TOOL_UA L"MTA-Tool/1.0"   // original WinHttp user-agent

enum FeatureId {
    FEAT_GOD = 0,
    FEAT_SPEED,
    FEAT_HEALTH,
    FEAT_ARMOR,
    FEAT_NOCLIP,
    FEAT_WALLHACK,
    FEAT_COUNT
};

#define MAX_ESP_PLAYERS 96

// ESP record filled by the MTA-Lua side (screen coords computed in Lua)
typedef struct EspPlayer {
    int   valid;
    float sx, sy;      // screen pos
    float dist;        // distance to local player
    int   isLocal;
    float health;
    char  name[32];
} EspPlayer;

typedef struct ArchConfig {
    int   menuOpen;
    int   keyMenu;                       // VK for menu toggle
    int   keyFeat[FEAT_COUNT];           // VK for each feature (0 = off)
    float speedMul;                      // 1.0 .. 5.0
    int   feat[FEAT_COUNT];              // feature state (0/1)
    int   keyOk;                         // key validated
    char  key[128];                      // key from Key.txt
    char  keyFile[300];                  // full path to Key.txt
    char  keyCheckUrl[256];              // e.g. https://host/check_key.php (empty = check disabled)
} ArchConfig;

extern ArchConfig g_cfg;
extern EspPlayer  g_esp[MAX_ESP_PLAYERS];
extern int        g_espCount;

// state visible from Lua (C side writes, Lua side reads via ar_state())
extern int  g_luaReady;      // 1 when payload injected
extern int  g_hookInstalled; // 1 when Present hook is in
extern int  g_keyValid;      // key check result

// ---- modules ----
void ArchLog(const char* fmt, ...);
const char* ArchDir(void);              // "<game dir>\Archangel\"
void Config_Init(void);                 // reads Key.txt + keybinds.cfg + settings.cfg
void Config_SaveKeybinds(void);
int  KeyCheck_Run(void);                // 1 valid, 0 invalid, -1 not configured/failed
void LUA_Init(void);                    // find lua51.dll + MTA LuaState, inject payload (retries inside)
int  LUA_IsReady(void);
void DX9_Init(void);                    // sig scan, vtable, device, install Present hook
int  DX9_HookInstalled(void);
void Menu_Frame(void);                  // draw ImGui menu (called from Present hook)
void ESP_Frame(void);                   // draw ESP (called from Present hook)

// ---- C functions registered into MTA Lua ----
int  ar_frameBegin(void);
int  ar_pushPlayer(float sx, float sy, float dist, const char* name, int local, float health);
int  ar_state(void);                    // pushes table {god,speed,health,armor,noclip,wallhack,menuOpen,speedMul}
