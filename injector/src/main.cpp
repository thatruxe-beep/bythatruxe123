// Archangel Injector - standalone 32-bit DLL injector with ImGui menu.
// Same "Archangel" look as the cheat menu: dark + red, ProggyClean 13/15/22 px.
// Menu toggles with INSERT. Injects DLL.dll (the Archangel MTA cheat) into a
// running target process (default: gta_sa.exe / MTA:SA) via the classic
// VirtualAllocEx + WriteProcessMemory + CreateRemoteThread(LoadLibraryA).
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static const char* INJ_VERSION = "1.0";

// ---------------------------------------------------------------- constants
static const ImU32 ACCENT     = IM_COL32(226, 54, 54, 255);
static const ImU32 ACCENT_DK  = IM_COL32(120, 26, 26, 255);
static const ImU32 TEXT_MAIN  = IM_COL32(232, 234, 240, 255);
static const ImU32 TEXT_DIM   = IM_COL32(128, 133, 148, 255);
static const ImU32 LINE_COL   = IM_COL32( 42,  46,  58, 255);
static const ImU32 OK_GREEN   = IM_COL32( 70, 200, 110, 255);
static const ImU32 ERR_RED    = IM_COL32(226,  70,  70, 255);

static const int WIN_W = 384;
static const int WIN_H = 348;

static ImVec4 C4(ImU32 c) {
    return ImVec4(((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f,
                  (c & 0xFF) / 255.0f, ((c >> 24) & 0xFF) / 255.0f);
}

// ---------------------------------------------------------------- state
static HWND              g_hwnd = NULL;
static IDirect3DDevice9* g_dev  = NULL;
static bool              g_menuOpen = false;
static int               g_prevInsert = 0;

static char    g_target[64]  = "gta_sa.exe";
static char    g_dllPath[260] = "DLL.dll";
static char    g_gameDir[260] = "";
static char    g_exeDir[260] = "";
static char    g_status[160] = "";
static int     g_statusOk = 0;   // 0 = none, 1 = ok, -1 = error

struct ProcInfo {
    DWORD pid;
    char  name[64];
};
static ProcInfo g_procs[128];
static int      g_procCount = 0;
static int      g_sel = -1;

static ImFont* g_fontTitle   = NULL;   // 22px
static ImFont* g_fontSection = NULL;   // 15px
static bool    g_themed = false;

// ---------------------------------------------------------------- helpers
static void ExeDir(char* out, int outSz) {
    char path[520];
    GetModuleFileNameA(NULL, path, 520);
    char* sl = strrchr(path, '\\');
    if (sl) *sl = 0;
    snprintf(out, outSz, "%s", path);
}

static void LoadConfig(void) {
    char fn[520], line[512], k[64], v[400];
    snprintf(fn, sizeof(fn), "%s\\Injector.cfg", g_exeDir);
    FILE* f = fopen(fn, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!sscanf(line, "%63[^=]=%399s", k, v)) continue;
        if (!strcmp(k, "target"))  snprintf(g_target,  sizeof(g_target),  "%s", v);
        if (!strcmp(k, "dll"))     snprintf(g_dllPath, sizeof(g_dllPath),  "%s", v);
        if (!strcmp(k, "gamedir")) snprintf(g_gameDir, sizeof(g_gameDir),  "%s", v);
    }
    fclose(f);
    // relative dll path -> resolve next to the exe
    if (!g_dllPath[0] || g_dllPath[0] == '\\') g_dllPath[0] = 0;
    if (g_dllPath[0] && g_dllPath[0] != '\\' && !(g_dllPath[1] == ':'))
        snprintf(g_dllPath, sizeof(g_dllPath), "%s\\%s", g_exeDir, g_dllPath);
}

static void SaveConfig(void) {
    char fn[520], rel[260];
    snprintf(fn, sizeof(fn), "%s\\Injector.cfg", g_exeDir);
    // store dll path relative to exe dir when possible
    snprintf(rel, sizeof(rel), "%s", g_dllPath);
    if (!strncmp(g_dllPath, g_exeDir, strlen(g_exeDir)))
        snprintf(rel, sizeof(rel), "%s", g_dllPath + strlen(g_exeDir) + 1);
    FILE* f = fopen(fn, "w");
    if (!f) return;
    fprintf(f, "target=%s\ndll=%s\ngamedir=%s\n", g_target, rel, g_gameDir);
    fclose(f);
    snprintf(g_status, sizeof(g_status), "Config saved");
    g_statusOk = 1;
}

static void RefreshProcs(void) {
    int oldPidIdx = -1;
    DWORD oldPid = (g_sel >= 0 && g_sel < g_procCount) ? g_procs[g_sel].pid : 0;
    g_procCount = 0;
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h == INVALID_HANDLE_VALUE) { g_sel = -1; return; }
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    if (Process32First(h, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, g_target) == 0 && g_procCount < 128) {
                g_procs[g_procCount].pid = pe.th32ProcessID;
                snprintf(g_procs[g_procCount].name, 64, "%s", pe.szExeFile);
                g_procCount++;
            }
        } while (Process32Next(h, &pe));
    }
    CloseHandle(h);
    g_sel = -1;
    for (int i = 0; i < g_procCount; i++)
        if (g_procs[i].pid == oldPid) { g_sel = i; break; }
}

static int InjectDll(DWORD pid, const char* dllPath, char* err, int errSz) {
    char abspath[520];
    if (!GetFullPathNameA(dllPath, 520, abspath, NULL)) {
        snprintf(err, errSz, "Bad DLL path");
        return 0;
    }
    HANDLE hf = CreateFileA(abspath, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        snprintf(err, errSz, "Cannot open DLL (err %lu)", GetLastError());
        return 0;
    }
    DWORD sz = GetFileSize(hf, NULL);
    char* buf = (char*)malloc(sz ? sz : 1);
    DWORD rd = 0;
    BOOL ok = ReadFile(hf, buf, sz, &rd, NULL);
    CloseHandle(hf);
    if (!ok || rd != sz) { free(buf); snprintf(err, errSz, "ReadFile failed"); return 0; }

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        free(buf);
        snprintf(err, errSz, "OpenProcess failed (err %lu)", GetLastError());
        return 0;
    }
    void* remote = VirtualAllocEx(hProc, NULL, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        CloseHandle(hProc); free(buf);
        snprintf(err, errSz, "VirtualAllocEx failed");
        return 0;
    }
    if (!WriteProcessMemory(hProc, remote, buf, sz, NULL)) {
        CloseHandle(hProc); free(buf);
        snprintf(err, errSz, "WriteProcessMemory failed");
        return 0;
    }
    free(buf);
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    FARPROC ll = GetProcAddress(k32, "LoadLibraryA");
    HANDLE hThr = CreateRemoteThread(hProc, NULL, 0,
                                     (LPTHREAD_START_ROUTINE)ll, remote, 0, NULL);
    if (!hThr) {
        CloseHandle(hProc);
        snprintf(err, errSz, "CreateRemoteThread failed (err %lu)", GetLastError());
        return 0;
    }
    WaitForSingleObject(hThr, 20000);
    DWORD code = 0;
    GetExitCodeThread(hThr, &code);
    CloseHandle(hThr);
    CloseHandle(hProc);
    snprintf(err, errSz, "LoadLibraryA returned 0x%lX", code);
    return 1;
}

// ---------------------------------------------------------------- theme
static void Menu_InitFonts(void) {
    ImFontAtlas* fs = ImGui::GetIO().Fonts;
    ImFontConfig cfg;
    cfg.SizePixels = 13.0f;
    fs->AddFontDefault(&cfg);
    cfg.SizePixels = 15.0f;
    fs->AddFontDefault(&cfg);
    cfg.SizePixels = 22.0f;
    fs->AddFontDefault(&cfg);
    g_fontSection = fs->Fonts[1];
    g_fontTitle   = fs->Fonts[2];
}

static void ThemeApply(void) {
    if (g_themed) return;
    g_themed = true;
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 12;  s.ChildRounding = 10;  s.FrameRounding = 8;
    s.GrabRounding = 8;     s.PopupRounding = 8;   s.ScrollbarRounding = 8;
    s.WindowBorderSize = 0; s.ChildBorderSize = 0; s.FrameBorderSize = 0;
    s.PopupBorderSize = 0;
    s.WindowPadding = ImVec2(0, 0);
    s.FramePadding = ImVec2(9, 6);
    s.ItemSpacing = ImVec2(10, 8);
    s.ScrollbarSize = 10;
    s.GrabMinSize = 10;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0.043f, 0.047f, 0.063f, 0.99f);
    c[ImGuiCol_ChildBg]         = ImVec4(0.086f, 0.098f, 0.129f, 1.00f);
    c[ImGuiCol_Border]          = ImVec4(0.165f, 0.180f, 0.227f, 1.00f);
    c[ImGuiCol_Text]            = ImVec4(0.910f, 0.918f, 0.941f, 1.00f);
    c[ImGuiCol_TextDisabled]    = ImVec4(0.502f, 0.522f, 0.580f, 1.00f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.102f, 0.114f, 0.149f, 1.00f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.141f, 0.153f, 0.196f, 1.00f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.165f, 0.176f, 0.227f, 1.00f);
    c[ImGuiCol_SliderGrab]      = ImVec4(0.886f, 0.212f, 0.212f, 1.00f);
    c[ImGuiCol_SliderGrabActive]= ImVec4(0.960f, 0.350f, 0.350f, 1.00f);
    c[ImGuiCol_CheckMark]       = ImVec4(0.886f, 0.212f, 0.212f, 1.00f);
    c[ImGuiCol_Separator]       = ImVec4(0.165f, 0.180f, 0.227f, 1.00f);
    c[ImGuiCol_Button]          = ImVec4(0.110f, 0.122f, 0.157f, 1.00f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.886f, 0.212f, 0.212f, 0.220f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.886f, 0.212f, 0.212f, 0.450f);
    c[ImGuiCol_Header]          = ImVec4(0.886f, 0.212f, 0.212f, 0.180f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.886f, 0.212f, 0.212f, 0.300f);
    c[ImGuiCol_HeaderActive]    = ImVec4(0.886f, 0.212f, 0.212f, 0.450f);
    c[ImGuiCol_ScrollbarBg]     = ImVec4(0.060f, 0.067f, 0.086f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(0.200f, 0.220f, 0.280f, 1.00f);
}

// ---------------------------------------------------------------- actions
static void DoInject(void) {
    if (g_sel < 0 || g_sel >= g_procCount) {
        snprintf(g_status, sizeof(g_status), "Select a process first");
        g_statusOk = -1;
        return;
    }
    DWORD pid = g_procs[g_sel].pid;
    char err[160] = "";
    if (InjectDll(pid, g_dllPath, err, 160)) {
        snprintf(g_status, sizeof(g_status), "PID %lu: %s", pid, err);
        g_statusOk = 1;
    } else {
        snprintf(g_status, sizeof(g_status), "PID %lu: %s", pid, err);
        g_statusOk = -1;
    }
}

static void DoKill(void) {
    if (g_sel < 0 || g_sel >= g_procCount) {
        snprintf(g_status, sizeof(g_status), "Select a process first");
        g_statusOk = -1;
        return;
    }
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, g_procs[g_sel].pid);
    if (!h) {
        snprintf(g_status, sizeof(g_status), "OpenProcess failed (err %lu)", GetLastError());
        g_statusOk = -1;
        return;
    }
    BOOL r = TerminateProcess(h, 0);
    CloseHandle(h);
    snprintf(g_status, sizeof(g_status), r ? "Process killed" : "TerminateProcess failed");
    g_statusOk = r ? 1 : -1;
    RefreshProcs();
}

static void DoOpenGame(void) {
    if (!g_gameDir[0]) {
        snprintf(g_status, sizeof(g_status), "Set game directory first");
        g_statusOk = -1;
        return;
    }
    HINSTANCE r = ShellExecuteA(NULL, "open", g_target, NULL, g_gameDir, SW_SHOW);
    if ((INT_PTR)r <= 32) {
        snprintf(g_status, sizeof(g_status), "Failed to start %s", g_target);
        g_statusOk = -1;
    } else {
        snprintf(g_status, sizeof(g_status), "Started %s", g_target);
        g_statusOk = 1;
    }
}

static void ToggleMenu(void) {
    g_menuOpen = !g_menuOpen;
    if (g_menuOpen) {
        RECT mon;
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &mon, 0);
        int x = mon.left + (mon.right - mon.left - WIN_W) / 2;
        int y = mon.top + (mon.bottom - mon.top - WIN_H) / 2;
        SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, WIN_W, WIN_H, SWP_SHOWWINDOW);
    } else {
        ShowWindow(g_hwnd, SW_HIDE);
    }
}

// ---------------------------------------------------------------- UI
static void UiFrame(void) {
    ThemeApply();
    ImGui::SetNextWindowSize(ImVec2(WIN_W, WIN_H), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                              ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
    if (!ImGui::Begin("##InjWin", NULL, flags)) {
        ImGui::End();
        return;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 wsz(ImGui::GetWindowSize());
    ImVec2 wr(wp.x + wsz.x, wp.y + wsz.y);

    dl->AddRect(ImVec2(wp.x + 0.5f, wp.y + 0.5f), ImVec2(wr.x - 0.5f, wr.y - 0.5f),
                IM_COL32(60, 64, 80, 255), 12.0f, 0, 1.5f);
    dl->AddLine(ImVec2(wp.x + 14, wp.y + 0.5f), ImVec2(wr.x - 14, wp.y + 0.5f), ACCENT, 2.0f);

    // header (draggable)
    float headerH = 46;
    {
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##hdr", ImVec2(wsz.x - 44, headerH));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            SetWindowPos(g_hwnd, HWND_TOPMOST, (int)(wp.x + d.x), (int)(wp.y + d.y),
                         0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
    }
    {
        ImVec2 cb(wp.x + wsz.x - 34, wp.y + 11);
        if (ImGui::InvisibleButton("##close", ImVec2(24, 24))) g_menuOpen = 0;
        if (ImGui::IsItemHovered())
            dl->AddCircleFilled(cb + ImVec2(12, 12), 11.0f, IM_COL32(226, 54, 54, 200));
        dl->AddLine(cb + ImVec2(7, 7),  cb + ImVec2(17, 17), TEXT_MAIN, 2.0f);
        dl->AddLine(cb + ImVec2(17, 7), cb + ImVec2(7, 17),  TEXT_MAIN, 2.0f);
    }
    ImGui::SetCursorPos(ImVec2(16, 9));
    {
        ImGui::PushFont(g_fontTitle);
        ImGui::TextColored(C4(ACCENT), "ARCHANGEL");
        ImGui::PopFont();
        ImGui::SameLine(0, 10);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 7);
        ImGui::TextColored(C4(TEXT_DIM), "INJECTOR");
        char vt[32]; snprintf(vt, sizeof(vt), "v%s", INJ_VERSION);
        float vw = ImGui::CalcTextSize(vt).x;
        ImGui::SetCursorScreenPos(ImVec2(wp.x + wsz.x - 44 - vw - 12, wp.y + 15));
        ImGui::TextColored(C4(TEXT_DIM), "%s", vt);
    }
    dl->AddLine(ImVec2(wp.x + 1, wp.y + headerH), ImVec2(wr.x - 1, wp.y + headerH), LINE_COL, 1.0f);

    // body
    ImGui::SetCursorPos(ImVec2(14, headerH + 10));
    {
        ImGui::TextColored(C4(TEXT_DIM), "Target process");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##target", g_target, sizeof(g_target),
                         ImGuiInputTextFlags_CharsNoBlank);

        ImGui::Spacing();
        ImGui::TextColored(C4(TEXT_DIM), "Running processes");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70, 0);
        if (ImGui::Button("Refresh", ImVec2(70, 20))) { RefreshProcs(); g_status[0] = 0; }

        float listH = wsz.y - headerH - 10 - 236;
        if (listH < 50) listH = 50;
        ImGui::BeginChild("##list", ImVec2(-1, listH));
        if (g_procCount == 0) {
            ImGui::TextColored(C4(TEXT_DIM), "(no %s running)", g_target);
        }
        for (int i = 0; i < g_procCount; i++) {
            char id[32], line[96];
            snprintf(id, sizeof(id), "##proc%d", i);
            snprintf(line, sizeof(line), "%-10lu  %s", g_procs[i].pid, g_procs[i].name);
            ImGui::PushStyleColor(ImGuiCol_Button,
                i == g_sel ? ACCENT_DK : IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text,
                i == g_sel ? ACCENT : TEXT_MAIN);
            if (ImGui::Button(line, ImVec2(-1, 20))) g_sel = i;
            ImGui::PopStyleColor(2);
        }
        ImGui::EndChild();

        // dll file row
        ImGui::Spacing();
        ImGui::TextColored(C4(TEXT_DIM), "DLL file");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##dll", g_dllPath, sizeof(g_dllPath));

        // status line
        if (g_status[0]) {
            ImGui::Spacing();
            ImGui::TextColored(C4(g_statusOk == 1 ? OK_GREEN :
                                  (g_statusOk == -1 ? ERR_RED : TEXT_DIM)), "%s", g_status);
        }

        // action buttons
        ImGui::Spacing();
        float bw = (ImGui::GetContentRegionAvail().x - 20) / 3.0f;
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(120, 26, 26, 255));
        if (ImGui::Button("Inject", ImVec2(bw, 28))) { DoInject(); }
        ImGui::PopStyleColor(1);
        ImGui::SameLine(0, 10);
        if (ImGui::Button("Kill", ImVec2(bw, 28))) { DoKill(); }
        ImGui::SameLine(0, 10);
        if (ImGui::Button("Open game", ImVec2(bw, 28))) { DoOpenGame(); }

        // save / exit row
        ImGui::Spacing();
        if (ImGui::Button("Save config", ImVec2(110, 22))) SaveConfig();
        ImGui::SameLine(0, 10);
        if (ImGui::Button("Exit", ImVec2(70, 22))) PostQuitMessage(0);
    }

    // footer
    {
        float fy = wsz.y - 22;
        dl->AddLine(ImVec2(wp.x + 1, wp.y + fy), ImVec2(wr.x - 1, wp.y + fy), LINE_COL, 1.0f);
        char foot[96];
        snprintf(foot, sizeof(foot), "v%s  •  Taran  •  menu: Insert", INJ_VERSION);
        float fw = ImGui::CalcTextSize(foot).x;
        ImGui::SetCursorScreenPos(ImVec2(wp.x + (wsz.x - fw) * 0.5f, wp.y + fy + 3));
        ImGui::TextColored(C4(TEXT_DIM), "%s", foot);
    }
    ImGui::End();
}

// ---------------------------------------------------------------- window
static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return 1;
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    ExeDir(g_exeDir, sizeof(g_exeDir));
    LoadConfig();
    if (!g_dllPath[0])
        snprintf(g_dllPath, sizeof(g_dllPath), "%s\\DLL.dll", g_exeDir);

    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = "ArchangelInjector";
    RegisterClassExA(&wc);

    g_hwnd = CreateWindowExA(0, "ArchangelInjector", "Archangel Injector",
                             WS_POPUP, 0, 0, WIN_W, WIN_H,
                             NULL, NULL, hInst, NULL);
    if (!g_hwnd) return 1;

    LPDIRECT3D9 pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS p;
    ZeroMemory(&p, sizeof(p));
    p.BackBufferWidth  = WIN_W;
    p.BackBufferHeight = WIN_H;
    p.hDeviceWindow    = g_hwnd;
    p.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    p.Flags            = 0;
    p.Windowed         = TRUE;
    p.BackBufferFormat = D3DFMT_UNKNOWN;
    if (!pD3D || pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hwnd,
                                    D3DCREATE_SOFTWARE_VERTEXPROCESSING, &p, &g_dev) != D3D_OK) {
        if (pD3D) pD3D->Release();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    Menu_InitFonts();                       // must run before backend init!
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX9_Init(g_dev);
    ThemeApply();

    RefreshProcs();
    ToggleMenu();                           // open menu at start

    MSG msg;
    DWORD lastRefresh = 0;
    bool running = true;
    while (running) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!running) break;

        int ins = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (ins && !g_prevInsert) ToggleMenu();
        g_prevInsert = ins;

        DWORD now = GetTickCount();
        if (now - lastRefresh >= 500) { RefreshProcs(); lastRefresh = now; }

        if (g_menuOpen) {
            ImGui_ImplDX9_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            UiFrame();
            ImGui::Render();
            g_dev->Clear(1, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0);
            g_dev->BeginScene();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_dev->EndScene();
            g_dev->Present(NULL, NULL, NULL, NULL);
        }
        Sleep(2);
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_dev) g_dev->Release();
    if (pD3D) pD3D->Release();
    return 0;
}
