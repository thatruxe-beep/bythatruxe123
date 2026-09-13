// Config: Key.txt, keybinds.cfg, settings.cfg  (Archangel dir next to game exe)
#include "archangel.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

ArchConfig g_cfg;
EspPlayer  g_esp[MAX_ESP_PLAYERS];
int        g_espCount = 0;
int        g_luaReady = 0;
int        g_hookInstalled = 0;
int        g_keyValid = 0;

static char g_dir[320] = {0};   // "<game dir>\Archangel"
static FILE* g_log = NULL;

const char* ArchDir(void) { return g_dir; }

void ArchLog(const char* fmt, ...) {
    char line[512];
    va_list a;
    va_start(a, fmt);
    vsnprintf(line, sizeof(line), fmt, a);
    va_end(a);
    OutputDebugStringA(line);
    if (!g_log) {
        if (g_dir[0]) {
            char p[320];
            snprintf(p, sizeof(p), "%sdebug.log", g_dir);
            g_log = fopen(p, "ab");
        }
    }
    if (g_log) {
        SYSTEMTIME st; GetLocalTime(&st);
        fprintf(g_log, "[%02d:%02d:%02d] %s\n", st.wHour, st.wMinute, st.wSecond, line);
        fflush(g_log);
    }
}

static void Dir_Build(void) {
    char exepath[MAX_PATH];
    GetModuleFileNameA(GetModuleHandleA(NULL), exepath, MAX_PATH);
    char* s = strrchr(exepath, '\\');
    if (s) *s = 0;
    snprintf(g_dir, sizeof(g_dir), "%s\\Archangel\\", exepath);
    CreateDirectoryA(g_dir, NULL);
    // key file: original used "\Archangel\Key.txt" (cwd-relative = game dir)
    snprintf(g_cfg.keyFile, sizeof(g_cfg.keyFile), "%s\\Archangel\\Key.txt", exepath);
    g_cfg.keyCheckUrl[0] = 0;
    g_cfg.key[0] = 0;
    g_cfg.menuOpen = 0;
    g_cfg.keyMenu = VK_DELETE;   // original menu key: Delete
    g_cfg.speedMul = 1.5f;
    for (int i = 0; i < FEAT_COUNT; i++) { g_cfg.keyFeat[i] = 0; g_cfg.feat[i] = 0; }
    g_cfg.feat[FEAT_WALLHACK] = 1;
}

static int File_ReadAll(const char* path, char* out, int outsz) {
    out[0] = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    int n = (int)fread(out, 1, (unsigned)outsz - 1, f);
    fclose(f);
    if (n < 0) n = 0;
    out[n] = 0;
    // strip newlines/CR, trim
    for (int i = 0; i < n; i++) if (out[i] == '\n' || out[i] == '\r') out[i] = 0;
    while (n > 0 && (out[n-1] == ' ' || out[n-1] == '\t')) out[--n] = 0;
    return 1;
}

void Config_Init(void) {
    Dir_Build();
    // key
    File_ReadAll(g_cfg.keyFile, g_cfg.key, sizeof(g_cfg.key));
    if (!g_cfg.key[0]) {
        // fallback: C:\Archangel\Key.txt
        File_ReadAll("C:\\Archangel\\Key.txt", g_cfg.key, sizeof(g_cfg.key));
        if (g_cfg.key[0])
            snprintf(g_cfg.keyFile, sizeof(g_cfg.keyFile), "C:\\Archangel\\Key.txt");
    }
    // keybinds.cfg
    char p[320]; char buf[512];
    snprintf(p, sizeof(p), "%skeybinds.cfg", g_dir);
    if (File_ReadAll(p, buf, sizeof(buf)) && buf[0]) {
        char* save = NULL;
        for (char* tok = strtok_r(buf, "\n\r; \t", &save); tok; tok = strtok_r(NULL, "\n\r; \t", &save)) {
            char k[32]; char v[16];
            if (sscanf(tok, " %31[^=]= %15s", k, v) == 2) {
                long val = strtol(v, NULL, v[0] == '0' && (v[1]=='x'||v[1]=='X') ? 16 : 10);
                if (!strcasecmp(k, "menu")) g_cfg.keyMenu = (int)val;
                else if (!strcasecmp(k, "god")) g_cfg.keyFeat[FEAT_GOD] = (int)val;
                else if (!strcasecmp(k, "speed")) g_cfg.keyFeat[FEAT_SPEED] = (int)val;
                else if (!strcasecmp(k, "health")) g_cfg.keyFeat[FEAT_HEALTH] = (int)val;
                else if (!strcasecmp(k, "armor")) g_cfg.keyFeat[FEAT_ARMOR] = (int)val;
                else if (!strcasecmp(k, "noclip")) g_cfg.keyFeat[FEAT_NOCLIP] = (int)val;
                else if (!strcasecmp(k, "wallhack")) g_cfg.keyFeat[FEAT_WALLHACK] = (int)val;
                else if (!strcasecmp(k, "speedmul")) g_cfg.speedMul = (float)(val / 10.0);
            }
        }
    }
    // settings.cfg (keycheck url)
    snprintf(p, sizeof(p), "%ssettings.cfg", g_dir);
    if (File_ReadAll(p, buf, sizeof(buf)) && buf[0]) {
        char* save = NULL;
        for (char* tok = strtok_r(buf, "\n\r; \t", &save); tok; tok = strtok_r(NULL, "\n\r; \t", &save)) {
            char k[32];
            char* eq = strchr(tok, '=');
            if (eq) { *eq = 0; if (sscanf(tok, " %31s", k) == 1 && !strcasecmp(k, "keycheckurl"))
                strncpy(g_cfg.keyCheckUrl, eq + 1, sizeof(g_cfg.keyCheckUrl) - 1); }
        }
    }
    ArchLog("config init: key='%s' keyfile=%s menukey=0x%02X", g_cfg.key[0] ? g_cfg.key : "<empty>",
            g_cfg.keyFile, g_cfg.keyMenu);
}

void Config_SaveKeybinds(void) {
    char p[320];
    snprintf(p, sizeof(p), "%skeybinds.cfg", g_dir);
    FILE* f = fopen(p, "wb");
    if (!f) return;
    fprintf(f, "menu=%d\ngod=%d\nspeed=%d\nhealth=%d\narmor=%d\nnoclip=%d\nwallhack=%d\nspeedmul=%d\n",
        g_cfg.keyMenu, g_cfg.keyFeat[FEAT_GOD], g_cfg.keyFeat[FEAT_SPEED], g_cfg.keyFeat[FEAT_HEALTH],
        g_cfg.keyFeat[FEAT_ARMOR], g_cfg.keyFeat[FEAT_NOCLIP], g_cfg.keyFeat[FEAT_WALLHACK],
        (int)(g_cfg.speedMul * 10));
    fclose(f);
    ArchLog("keybinds saved: %s", p);
}

void Config_SaveKey(void) {
    if (!g_cfg.keyFile[0] || !g_cfg.key[0]) return;
    FILE* f = fopen(g_cfg.keyFile, "wb");
    if (!f) return;
    fputs(g_cfg.key, f);
    fclose(f);
    ArchLog("key saved: %s", g_cfg.keyFile);
}

