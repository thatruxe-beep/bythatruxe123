// D3D9 Present hook (reconstructed approach from DLL.dll.c):
//  - the original loads <sysdir>\d3d9.dll and scans the image for a fixed signature
//    (decompiled around line 2098: pattern 6C 07 .. 89 86 .. 89 86, func start at match+3)
//    to locate Present inside d3d9.dll;
//  - finds the IDirect3DDevice9 object whose first DWORD is the d3d9 device vtable
//    (vtable located by searching the d3d9 image for the Present pointer);
//  - suspends all other threads of the process (decompiled FUN_10003d30),
//    patches the vtable slot, resumes threads;
//  - the Present hook renders the ImGui overlay, then calls the original Present.
#include "archangel.h"
#include <d3d9.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

#include "imgui.h"
#include "backends/imgui_impl_dx9.h"
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
extern void Menu_Frame(void);
extern void ESP_Frame(void);
void DX9_ImGuiInitDevice(IDirect3DDevice9* dev);

typedef HRESULT (STDMETHODCALLTYPE *Present_t)(LPDIRECT3DDEVICE9, const RECT*, const RECT*, HWND, const RECT*);

static Present_t  g_origPresent = NULL;
static IDirect3DDevice9* g_device = NULL;
static void*      g_deviceObj = NULL;
static void*      g_vtable = NULL;
static DWORD      g_vtableSlot = 17;
static HWND       g_hwnd = NULL;
static WNDPROC    g_oldWndProc = NULL;
static int        g_imguiInit = 0;

static const char* featNames[FEAT_COUNT] = { "God", "Speed", "Health", "Armor", "Noclip", "Wallhack" };

// ---------------- original helper: suspend/resume all other threads ----------------
static void Threads_SetSuspend(int suspend) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    DWORD self = GetCurrentThreadId();
    DWORD pid = GetCurrentProcessId();
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid && te.th32ThreadID != self) {
                HANDLE h = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (h) {
                    if (suspend) SuspendThread(h); else ResumeThread(h);
                    CloseHandle(h);
                }
            }
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
}

// ---------------- d3d9: find Present via original signature ----------------
static int FindPresentSig(HMODULE d3d, size_t* off) {
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)d3d;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((char*)d3d + dos->e_lfanew);
    size_t sz = nt->OptionalHeader.SizeOfImage;
    unsigned char* base = (unsigned char*)d3d;
    size_t limit = sz < 0x94000 ? sz : 0x94000;   // original scans first 0x94000 bytes
    for (size_t m = 0x100; m + 16 <= limit; m++) {
        if (base[m+1] == 0x6C && base[m+2] == 0x07 &&
            base[m+7] == 0x89 && base[m+8] == 0x86 &&
            base[m+13] == 0x89 && base[m+14] == 0x86) {
            *off = m + 3;    // original: function start = match + 3
            ArchLog("d3d9: Present sig match at +%zx", *off);
            return 1;
        }
    }
    return 0;
}

static inline int PtrOk(uintptr_t b, uintptr_t e, DWORD v) { return (uintptr_t)v >= b && (uintptr_t)v < e; }

// ---------------- locate device vtable inside d3d9 image ----------------
static int FindVtable(HMODULE d3d, size_t presentOff, void** vtbl) {
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)d3d;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((char*)d3d + dos->e_lfanew);
    size_t sz = nt->OptionalHeader.SizeOfImage;
    uintptr_t base = (uintptr_t)d3d, end = base + sz;
    DWORD target = (DWORD)(base + presentOff);
    for (uintptr_t p = base + 0x1000; p + 4 <= end; p += 4) {
        if (*(DWORD*)p == target) {
            void* vt = (void*)(p - g_vtableSlot * 4);
            // sanity: neighbors are function pointers inside the image
            DWORD* e = (DWORD*)vt;
            int ok = PtrOk(base, end, e[0]) && PtrOk(base, end, e[g_vtableSlot-1]) && PtrOk(base, end, e[g_vtableSlot+1]);
            if (ok) { *vtbl = vt; return 1; }
        }
    }
    return 0;
}

// ---------------- find the device object (first DWORD == vtable) ----------------
static void* FindDevice(void* vtbl) {
    // walk committed user-space pages looking for an object whose first DWORD is the vtable
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t addr = 0x10000; addr < 0x70000000;
         addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize) {
        if (!VirtualQuery((void*)addr, &mbi, sizeof(mbi))) break;
        if (mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_READONLY))) {
            uintptr_t r = (uintptr_t)mbi.BaseAddress;
            uintptr_t rend = r + mbi.RegionSize;
            for (; r + 4 <= rend; r += 4) {
                if (*(void**)r == vtbl) {
                    ArchLog("d3d9: device at %p", (void*)r);
                    return (void*)r;
                }
            }
        }
    }
    return NULL;
}

// ---------------- WndProc subclass for ImGui input ----------------
static LRESULT CALLBACK WndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return TRUE;
    return CallWindowProcA(g_oldWndProc, hWnd, msg, wParam, lParam);
}

// ---------------- Present hook ----------------
static int g_prevFeat[FEAT_COUNT] = {0};
static int g_prevMenu = 0;

static HRESULT STDMETHODCALLTYPE HookedPresent(IDirect3DDevice9* dev, const RECT* s, const RECT* d, HWND f, const RECT* ex) {
    if (!g_imguiInit) DX9_ImGuiInitDevice(dev);
    if (g_imguiInit && g_hwnd) {
        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX9_NewFrame();
        ImGui::NewFrame();
        // keybinds (edge detection)
        for (int i = 0; i < FEAT_COUNT; i++) {
            int k = g_cfg.keyFeat[i];
            if (!k) { g_prevFeat[i] = 0; continue; }
            int now = (GetAsyncKeyState(k) & 0x8000) != 0;
            if (now && !g_prevFeat[i]) g_cfg.feat[i] = !g_cfg.feat[i];
            g_prevFeat[i] = now;
        }
        int km = g_cfg.keyMenu;
        if (km) {
            int now = (GetAsyncKeyState(km) & 0x8000) != 0;
            if (now && !g_prevMenu) g_cfg.menuOpen = !g_cfg.menuOpen;
            g_prevMenu = now;
        }

        if (g_cfg.menuOpen) Menu_Frame();
        if (g_cfg.feat[FEAT_WALLHACK]) ESP_Frame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }
    return g_origPresent(dev, s, d, f, ex);
}

// ---------------- init (worker thread) ----------------
int DX9_HookInstalled(void) { return g_hookInstalled; }

int DX9_TryInit(void) {
    if (g_hookInstalled) return 1;
    HMODULE d3d = GetModuleHandleA("d3d9.dll");
    if (!d3d) { ArchLog("d3d9: not loaded yet"); return 0; }

    size_t off;
    if (!FindPresentSig(d3d, &off)) { ArchLog("d3d9: signature not found (d3d9 version?)"); return 0; }
    void* vt;
    if (!FindVtable(d3d, off, &vt)) { ArchLog("d3d9: vtable not found"); return 0; }
    g_vtable = vt;
    void* dev = FindDevice(vt);
    if (!dev) { ArchLog("d3d9: device object not found yet"); return 0; }
    g_deviceObj = dev;

    DWORD* vte = (DWORD*)vt;
    g_origPresent = (Present_t)vte[g_vtableSlot];

    g_hwnd = GetForegroundWindow();
    if (!g_hwnd) g_hwnd = FindWindowA("GTA Window", NULL);
    if (!g_hwnd) { ArchLog("d3d9: no game window"); return 0; }

    // subclass for input
    g_oldWndProc = (WNDPROC)GetWindowLongA(g_hwnd, GWL_WNDPROC);
    SetWindowLongA(g_hwnd, GWL_WNDPROC, (LONG)WndProcHook);

    // patch vtable with threads suspended (like the original)
    Threads_SetSuspend(1);
    DWORD oldprot;
    VirtualProtect(vte, sizeof(void*), PAGE_READWRITE, &oldprot);
    vte[g_vtableSlot] = (DWORD)&HookedPresent;
    VirtualProtect(vte, sizeof(void*), oldprot, &oldprot);
    FlushInstructionCache(GetCurrentProcess(), vte, sizeof(void*));
    Threads_SetSuspend(0);

    g_hookInstalled = 1;
    ArchLog("d3d9: Present hook installed (orig %p, hwnd %p)", g_origPresent, g_hwnd);
    return 1;
}

void DX9_Init(void) {
    for (int i = 0; i < 240 && !g_hookInstalled; i++) {   // up to ~4 min
        if (DX9_TryInit()) break;
        Sleep(1000);
    }
    if (!g_hookInstalled) ArchLog("d3d9: FAILED to install hook");
    // ImGui is lazily initialized on the first Present (real device needed)
}

// called from the hook once, with the real device
void DX9_ImGuiInitDevice(IDirect3DDevice9* dev) {
    if (g_imguiInit) return;
    g_device = dev;
    if (!ImGui::CreateContext()) return;
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // ImGui default font is embedded ProggyClean (matches original "ProggyClean.ttf")
    ImGui_ImplWin32_Init((void*)g_hwnd);
    if (ImGui_ImplDX9_Init(dev)) g_imguiInit = 1;
    ArchLog("imgui: initialized");
}
