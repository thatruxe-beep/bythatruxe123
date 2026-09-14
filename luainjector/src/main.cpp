// Archangel Lua Injector - minimal 32-bit DLL: only the Lua injection part.
//
// On inject (DllMain -> worker thread):
//   1. wait for lua51.dll (MTA) to load, up to ~2 minutes;
//   2. grab luaL_loadbuffer / lua_pcall / ... via GetProcAddress;
//   3. locate the game's Lua 5.1 state (forced offset from
//      Archangel\settings.cfg "luastateoffset=" or automatic TMS scan);
//   4. run the embedded payload (payload/script.lua), which logs via
//      registered C function luainj_log() and, if present, loads the user
//      script Archangel\script.lua (CWD = game dir).
// No ImGui, no d3d9 hook, no dialogs. Log: Archangel\luainj.log.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "payload_lua.h"   // generated: unsigned char g_payloadLua[]; unsigned int g_payloadLuaSize;

static const char* VER = "1.0";

// ---------------------------------------------------------------- log
static FILE* g_log = NULL;

static void LogOpen(void) {
    CreateDirectoryA("Archangel", NULL);
    g_log = fopen("Archangel\\luainj.log", "a");
    if (!g_log) g_log = fopen("luainj.log", "a");
    if (g_log) {
        fprintf(g_log, "[luainjector v%s] attached (pid %lu)\n", VER, GetCurrentProcessId());
        fflush(g_log);
    }
}

static void Log(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(g_log, "[luainjector] ");
    vfprintf(g_log, fmt, ap);
    fprintf(g_log, "\n");
    fflush(g_log);
    va_end(ap);
    OutputDebugStringA(fmt);
}

// ---------------------------------------------------------------- lua api
typedef struct lua_State lua_State;
typedef int   (*luaL_loadbuffer_t)(lua_State*, const char*, size_t, const char*);
typedef int   (*lua_pcall_t)(lua_State*, int, int, int);
typedef void  (*lua_register_t)(lua_State*, const char*, int (*)(lua_State*));
typedef const char* (*lua_tostring_t)(lua_State*, int);

static lua_State*       g_L = NULL;
static luaL_loadbuffer_t g_loadbuffer;
static lua_pcall_t      g_pcall;
static lua_register_t   g_register;
static lua_tostring_t   g_tostr;

// C function registered into Lua: luainj_log(msg)
static int luainj_log_cb(lua_State* L) {
    const char* s = g_tostr ? g_tostr(L, 1) : NULL;
    if (s) Log("lua: %s", s);
    return 0;
}

// ------------------------------------------------- LuaState discovery
// (same technique as the full cheat: a Lua 5.1 state owns a TMS - a table of
//  C-API pointers into lua51.dll; find a run of >=8 such pointers in the game
//  module, then a DWORD referencing the TMS => lua_State, validate header)
static uintptr_t Range_Get(HMODULE m, uintptr_t* end) {
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)m;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((char*)m + dos->e_lfanew);
    uintptr_t base = (uintptr_t)m;
    *end = base + nt->OptionalHeader.SizeOfImage;
    return base;
}

static int Ptr_InRange(uintptr_t p, uintptr_t b, uintptr_t e) { return p >= b && p < e; }

static lua_State* State_AutoScan(HMODULE game, HMODULE lua51) {
    uintptr_t gb, ge, lb, le;
    gb = Range_Get(game, &ge);
    lb = Range_Get(lua51, &le);
    unsigned char* p = (unsigned char*)gb;
    unsigned char* pend = (unsigned char*)ge;
    while (p + 40 <= pend) {
        int run = 0;
        while (p + 4 <= pend && run < 12) {
            uintptr_t v = *(uintptr_t*)p;
            if (Ptr_InRange(v, lb, le)) { run++; p += 4; }
            else break;
        }
        if (run >= 8) {
            uintptr_t tms = (uintptr_t)(p - run * 4);
            Log("TMS candidate at %p (%d entries)", (void*)tms, run);
            unsigned char* q = (unsigned char*)gb;
            while (q + 4 <= pend) {
                if (*(uintptr_t*)q == tms) {
                    lua_State* L = *(lua_State**)q;
                    if (L && (uintptr_t)L >= 0x10000) {
                        int tty = *(int*)((char*)L + 4);       // CommonHeader.tty
                        char gc = *(char*)((char*)L + 5);      // CommonHeader.gcref
                        void* next = *(void**)L;
                        if (tty == 0x1D /*LUA_TTHREAD*/ && gc && next) {
                            Log("state found at %p (ref at %p)", L, q);
                            return L;
                        }
                    }
                }
                q += 4;
            }
        }
        p += 4;
    }
    return NULL;
}

static HMODULE GameModule(void) {
    static const char* names[] = { "game.exe", "MTA.exe", "mta.exe", "GTA.exe", "SA.exe",
                                   "gta_sa.exe", "GTA_SSA.exe" };
    for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])); i++) {
        HMODULE m = GetModuleHandleA(names[i]);
        if (m) return m;
    }
    // fallback: the main module of the process
    return GetModuleHandleA(NULL);
}

// forced offset (hex) from Archangel\settings.cfg, same key as the full cheat
static uintptr_t ForcedOffset(void) {
    char buf[1024];
    FILE* f = fopen("Archangel\\settings.cfg", "rb");
    if (!f) return 0;
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = 0;
    fclose(f);
    char* k = strstr(buf, "luastateoffset");
    if (!k) return 0;
    char* eq = strchr(k, '=');
    if (!eq) return 0;
    return strtoull(eq + 1, NULL, 16) & ~15UL;
}

static int TryInit(void) {
    if (g_L) return 1;
    HMODULE lua51 = GetModuleHandleA("lua51.dll");
    if (!lua51) { Log("lua51.dll not loaded yet"); return 0; }
    HMODULE game = GameModule();
    if (!game) { Log("game module not found"); return 0; }

    g_loadbuffer = (luaL_loadbuffer_t)GetProcAddress(lua51, "luaL_loadbuffer");
    g_pcall      = (lua_pcall_t)      GetProcAddress(lua51, "lua_pcall");
    g_register   = (lua_register_t)   GetProcAddress(lua51, "lua_register");
    g_tostr      = (lua_tostring_t)   GetProcAddress(lua51, "lua_tostring");
    if (!(g_loadbuffer && g_pcall && g_register && g_tostr)) {
        Log("missing lua51.dll exports");
        return 0;
    }

    lua_State* L = NULL;
    uintptr_t off = ForcedOffset();
    if (off) {
        L = (lua_State*)((uintptr_t)game + off);
        if (!L || *(int*)((char*)L + 4) != 0x1D) {
            Log("forced offset 0x%lx invalid", off);
            L = NULL;
        }
    }
    if (!L) L = State_AutoScan(game, lua51);
    if (!L) { Log("lua state not found yet"); return 0; }

    g_register(L, "luainj_log", (int (*)(lua_State*))luainj_log_cb);

    int rc = g_loadbuffer(L, (const char*)g_payloadLua, g_payloadLuaSize, "luainjector");
    if (rc) {
        Log("loadbuffer failed rc=%d", rc);
        g_pcall(L, 0, 0, 0);
        return 0;
    }
    rc = g_pcall(L, 0, 0, 0);
    if (rc) {
        const char* err = g_tostr(L, -1);
        Log("pcall failed rc=%d err=%s", rc, err ? err : "?");
        g_pcall(L, 0, 0, 0);
        return 0;
    }
    g_L = L;
    Log("payload injected (%u bytes) into %p", g_payloadLuaSize, (void*)L);
    return 1;
}

static DWORD WINAPI Worker(LPVOID) {
    LogOpen();
    Log("worker started, waiting for MTA lua...");
    for (int i = 0; i < 120 && !g_L; i++) {
        if (TryInit()) break;
        Sleep(1000);
    }
    if (!g_L) Log("giving up: lua state was not found");
    return 0;
}

static HMODULE g_mod = NULL;
static HANDLE  g_thr = NULL;

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
    (void)mod; (void)reason;
    if (reason == DLL_PROCESS_ATTACH) {
        g_mod = mod;
        DisableThreadLibraryCalls(mod);
        g_thr = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Worker, NULL, 0, NULL);
        if (g_thr) CloseHandle(g_thr);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_log) { fclose(g_log); g_log = NULL; }
    }
    return TRUE;
}
