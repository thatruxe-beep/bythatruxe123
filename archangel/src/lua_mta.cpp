// MTA Lua bridge:
//  - original grabs GetModuleHandleA("lua51.dll") + GetProcAddress("luaL_loadbuffer")
//    (decompiled in DLL.dll.c, ~line 16570) and runs the embedded payload in the
//    game's own Lua state. The 16KB payload blob (DAT_100ae610) and the exact
//    LuaState offset are .rdata - not recoverable from the decompiled dump - so the
//    payload is reconstructed (payload/archangel.lua) and the state is located via
//    settings.cfg offset or an automatic TMS scan.
#include "archangel.h"
#include <stdio.h>
#include <string.h>
#include "payload_lua.h"   // generated: unsigned char g_payloadLua[]; unsigned int g_payloadLuaSize;

typedef struct lua_State lua_State;
typedef int   (*luaL_loadbuffer_t)(lua_State*, const char*, size_t, const char*);
typedef int   (*lua_pcall_t)(lua_State*, int, int, int);
typedef void  (*lua_register_t)(lua_State*, const char*, int (*)(lua_State*));
typedef void  (*lua_newtable_t)(lua_State*);
typedef void  (*lua_pushnumber_t)(lua_State*, double);
typedef void  (*lua_pushstring_t)(lua_State*, const char*);
typedef void  (*lua_settable_t)(lua_State*, int);
typedef double(*lua_tonumber_t)(lua_State*, int);
typedef const char*(*lua_tostring_t)(lua_State*, int);

static lua_State*      g_L = NULL;
static luaL_loadbuffer_t g_loadbuffer;
static lua_pcall_t     g_pcall;
static lua_register_t  g_register;
static lua_newtable_t  g_newtable;
static lua_pushnumber_t g_pushnum;
static lua_pushstring_t g_pushstr;
static lua_settable_t  g_settable;
static lua_tonumber_t  g_tonum;
static lua_tostring_t  g_tostr;

// ---------- C -> Lua registered functions ----------
int ar_frameBegin(lua_State* L) { (void)L; g_espCount = 0; return 0; }

int ar_pushPlayer(lua_State* L) {
    if (g_espCount >= MAX_ESP_PLAYERS) return 0;
    EspPlayer* e = &g_esp[g_espCount];
    e->valid = 1;
    e->sx = (float)g_tonum(L, 1);
    e->sy = (float)g_tonum(L, 2);
    e->dist = (float)g_tonum(L, 3);
    const char* n = g_tostr(L, 4);
    if (n) { strncpy(e->name, n, sizeof(e->name) - 1); e->name[31] = 0; }
    e->isLocal = (int)g_tonum(L, 5);
    e->health = (float)g_tonum(L, 6);
    g_espCount++;
    return 0;
}

int ar_state(lua_State* L) {
    g_newtable(L);
    g_pushstr(L, "god");       g_pushnum(L, g_cfg.feat[FEAT_GOD]);      g_settable(L, -3);
    g_pushstr(L, "speed");     g_pushnum(L, g_cfg.feat[FEAT_SPEED]);    g_settable(L, -3);
    g_pushstr(L, "health");    g_pushnum(L, g_cfg.feat[FEAT_HEALTH]);   g_settable(L, -3);
    g_pushstr(L, "armor");     g_pushnum(L, g_cfg.feat[FEAT_ARMOR]);    g_settable(L, -3);
    g_pushstr(L, "noclip");    g_pushnum(L, g_cfg.feat[FEAT_NOCLIP]);   g_settable(L, -3);
    g_pushstr(L, "wallhack");  g_pushnum(L, g_cfg.feat[FEAT_WALLHACK]); g_settable(L, -3);
    g_pushstr(L, "speedmul");  g_pushnum(L, g_cfg.speedMul);            g_settable(L, -3);
    return 1;
}

// ---------- LuaState discovery ----------
static uintptr_t Range_Get(HMODULE m, uintptr_t* end) {
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)m;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((char*)m + dos->e_lfanew);
    uintptr_t base = (uintptr_t)m;
    *end = base + nt->OptionalHeader.SizeOfImage;
    return base;
}

static int Ptr_InRange(uintptr_t p, uintptr_t b, uintptr_t e) { return p >= b && p < e; }

// find MTA LuaState:
//  Lua 5.1 states own a TMS (table of C-API function pointers into lua51.dll).
//  Scan the game module for a run of >=8 consecutive pointers into lua51.dll -> TMS,
//  then for a DWORD pointing at TMS (lua_State->tm) -> candidate L, validate header.
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
            ArchLog("lua: TMS candidate at %p (%d entries)", (void*)tms, run);
            // find lua_State holding this TMS
            unsigned char* q = (unsigned char*)gb;
            while (q + 4 <= pend) {
                if (*(uintptr_t*)q == tms) {
                    lua_State* L = *(lua_State**)q;
                    if (L && (uintptr_t)L >= 0x10000) {
                        int tty = *(int*)((char*)L + 4);       // CommonHeader.tty
                        char gc = *(char*)((char*)L + 5);      // CommonHeader.gcref
                        void* next = *(void**)L;
                        if (tty == 0x1D /*LUA_TTHREAD*/ && gc && next) {
                            ArchLog("lua: state found at %p (ref at %p)", L, q);
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

int LUA_IsReady(void) { return g_L != NULL; }

static HMODULE GameModule(void) {
    static const char* names[] = { "game.exe", "MTA.exe", "mta.exe", "GTA.exe", "SA.exe" };
    for (int i = 0; i < (int)(sizeof(names)/sizeof(names[0])); i++) {
        HMODULE m = GetModuleHandleA(names[i]);
        if (m) return m;
    }
    return NULL;
}

// returns 1 if ready
int LUA_TryInit(uintptr_t forcedOffset) {
    if (g_L) return 1;
    HMODULE lua51 = GetModuleHandleA("lua51.dll");
    if (!lua51) { ArchLog("lua: lua51.dll not loaded yet"); return 0; }
    HMODULE game = GameModule();
    if (!game) { ArchLog("lua: game module not found"); return 0; }

    void* f = (void*)GetProcAddress(lua51, "luaL_loadbuffer");
    void* p = (void*)GetProcAddress(lua51, "lua_pcall");
    void* r = (void*)GetProcAddress(lua51, "lua_register");
    void* nt = (void*)GetProcAddress(lua51, "lua_newtable");
    void* pn = (void*)GetProcAddress(lua51, "lua_pushnumber");
    void* ps = (void*)GetProcAddress(lua51, "lua_pushstring");
    void* st = (void*)GetProcAddress(lua51, "lua_settable");
    void* tn = (void*)GetProcAddress(lua51, "lua_tonumber");
    void* ts = (void*)GetProcAddress(lua51, "lua_tostring");
    if (!(f && p && r && nt && pn && ps && st && tn && ts)) { ArchLog("lua: missing exports"); return 0; }
    g_loadbuffer = (luaL_loadbuffer_t)f;
    g_pcall = (lua_pcall_t)p;
    g_register = (lua_register_t)r;
    g_newtable = (lua_newtable_t)nt;
    g_pushnum = (lua_pushnumber_t)pn;
    g_pushstr = (lua_pushstring_t)ps;
    g_settable = (lua_settable_t)st;
    g_tonum = (lua_tonumber_t)tn;
    g_tostr = (lua_tostring_t)ts;

    lua_State* L = NULL;
    if (forcedOffset) {
        L = (lua_State*)((uintptr_t)game + forcedOffset);
        if (L && *(int*)((char*)L + 4) != 0x1D) { ArchLog("lua: forced offset 0x%lx invalid (tt=%d)", forcedOffset, *(int*)((char*)L+4)); L = NULL; }
    }
    if (!L) L = State_AutoScan(game, lua51);
    if (!L) { ArchLog("lua: state not found yet"); return 0; }

    // register C helpers
    g_register(L, "ar_frameBegin", (int(*)(lua_State*))ar_frameBegin);
    g_register(L, "ar_pushPlayer", (int(*)(lua_State*))ar_pushPlayer);
    g_register(L, "ar_state",      (int(*)(lua_State*))ar_state);

    int rc = g_loadbuffer(L, (const char*)g_payloadLua, g_payloadLuaSize, "archangel");
    if (rc) { ArchLog("lua: loadbuffer failed rc=%d", rc); g_L = NULL; return 0; }
    rc = g_pcall(L, 0, 0, 0);
    if (rc) {
        const char* err = g_tostr(L, -1);
        ArchLog("lua: pcall failed rc=%d err=%s", rc, err ? err : "?");
        g_pcall(L, 0, 0, 0); // clean stack
        g_L = NULL;
        return 0;
    }
    g_L = L;
    g_luaReady = 1;
    ArchLog("lua: payload injected (%u bytes)", g_payloadLuaSize);
    return 1;
}

void LUA_Init(void) {
    // offset from settings.cfg (hex), 0 = auto-scan
    uintptr_t off = 0;
    char p[320];
    FILE* f = NULL;
    snprintf(p, sizeof(p), "%ssettings.cfg", ArchDir());
    f = fopen(p, "rb");
    if (f) {
        char buf[1024];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = 0;
        char* k = strstr(buf, "luastateoffset");
        if (k) {
            char* eq = strchr(k, '=');
            if (eq) off = strtoull(eq + 1, NULL, 16) & ~15UL;
        }
        fclose(f);
    }
    for (int i = 0; i < 120 && !g_L; i++) {   // up to ~2 min, every ~1s
        if (LUA_TryInit(off)) break;
        Sleep(1000);
    }
}
