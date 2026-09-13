// Archangel menu - premium redesign (same "Archangel" look: dark + red,
// ImGui) - higher quality UI: custom theme, custom fonts, icon nav rail,
// toggle switches, keybind capture, status indicators, draggable header.
#include "archangel.h"
#include <stdio.h>
#include <string.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include <math.h>

static const ImU32 ACCENT     = IM_COL32(226, 54, 54, 255);
static const ImU32 ACCENT_SOF = IM_COL32(226, 54, 54,  60);
static const ImU32 ACCENT_DK  = IM_COL32(120, 26, 26, 255);
static const ImU32 TEXT_MAIN  = IM_COL32(232, 234, 240, 255);
static const ImU32 TEXT_DIM   = IM_COL32(128, 133, 148, 255);
static const ImU32 PANEL_BG   = IM_COL32( 22,  25,  33, 255);
static const ImU32 LINE_COL   = IM_COL32( 42,  46,  58, 255);
static const ImU32 OK_GREEN   = IM_COL32( 70, 200, 110, 255);
static const ImU32 WARN_YEL   = IM_COL32(230, 190,  60, 255);

static ImVec4 C4(ImU32 c) {
    return ImVec4(((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f,
                  (c & 0xFF) / 255.0f, ((c >> 24) & 0xFF) / 255.0f);
}

static const char* FEAT_LABELS[FEAT_COUNT] = {
    "God Mode", "Speed", "Health (auto)", "Armor (auto)", "Noclip", "Wallhack (ESP)"
};

static ImFont* g_fontTitle = NULL;    // 22px
static ImFont* g_fontSection = NULL;  // 15px
static bool    g_themed = false;
static int     g_tab = 0;
static int     g_capture = -1;        // index of key being captured (0=menu, 1..N=features), -1 = none

// ---------------------------------------------------------------- fonts/theme
void Menu_InitFonts(void) {
    ImFontAtlas* fs = ImGui::GetIO().Fonts;
    ImFontConfig cfg;
    cfg.SizePixels = 13.0f;   // default size
    fs->AddFontDefault(&cfg);
    cfg.SizePixels = 15.0f;   // section labels
    fs->AddFontDefault(&cfg);
    cfg.SizePixels = 22.0f;   // title
    fs->AddFontDefault(&cfg);
    g_fontSection = fs->Fonts[1];
    g_fontTitle   = fs->Fonts[2];
}

static void ThemeApply(void) {
    if (g_themed) return;
    g_themed = true;
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 12;
    s.ChildRounding = 10;
    s.FrameRounding = 8;
    s.GrabRounding = 8;
    s.PopupRounding = 8;
    s.ScrollbarRounding = 8;
    s.WindowBorderSize = 0;
    s.ChildBorderSize = 0;
    s.FrameBorderSize = 0;
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

// ---------------------------------------------------------------- draw helpers
static void Icon(ImDrawList* dl, int type, ImVec2 ctr, ImU32 col) {
    switch (type) {
    case 0: // diamond (features)
        dl->AddLine(ImVec2(ctr.x, ctr.y-7), ImVec2(ctr.x+7, ctr.y), col, 2.0f);
        dl->AddLine(ImVec2(ctr.x+7, ctr.y), ImVec2(ctr.x, ctr.y+7), col, 2.0f);
        dl->AddLine(ImVec2(ctr.x, ctr.y+7), ImVec2(ctr.x-7, ctr.y), col, 2.0f);
        dl->AddLine(ImVec2(ctr.x-7, ctr.y), ImVec2(ctr.x, ctr.y-7), col, 2.0f);
        break;
    case 1: // keyboard (keys)
        dl->AddRect(ImVec2(ctr.x-8, ctr.y-5), ImVec2(ctr.x+8, ctr.y+5), col, 3.0f, 0, 1.6f);
        dl->AddLine(ImVec2(ctr.x-4, ctr.y-1), ImVec2(ctr.x-2, ctr.y-1), col, 1.6f);
        dl->AddLine(ImVec2(ctr.x+0, ctr.y-1), ImVec2(ctr.x+2, ctr.y-1), col, 1.6f);
        dl->AddLine(ImVec2(ctr.x+4, ctr.y-1), ImVec2(ctr.x+6, ctr.y-1), col, 1.6f);
        dl->AddLine(ImVec2(ctr.x-3, ctr.y+2), ImVec2(ctr.x+3, ctr.y+2), col, 1.6f);
        break;
    case 2: // gear (settings)
        dl->AddCircle(ctr, 5.0f, col, 16, 1.8f);
        dl->AddCircle(ctr, 1.8f, col, 10);
        for (int i = 0; i < 6; i++) {
            float a = (float)i * 1.0472f;
            ImVec2 a1(ctr.x + cosf(a) * 6.5f, ctr.y + sinf(a) * 6.5f);
            ImVec2 a2(ctr.x + cosf(a) * 9.0f, ctr.y + sinf(a) * 9.0f);
            dl->AddLine(a1, a2, col, 2.0f);
        }
        break;
    default: // about (i in circle)
        dl->AddCircle(ctr, 7.0f, col, 18, 1.8f);
        dl->AddLine(ImVec2(ctr.x, ctr.y-1.5f), ImVec2(ctr.x, ctr.y+4.0f), col, 2.0f);
        dl->AddCircleFilled(ImVec2(ctr.x, ctr.y-4.2f), 1.2f, col);
        break;
    }
}

static void StatusDot(ImU32 col, const char* text) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddCircleFilled(ImVec2(p.x + 5, p.y + ImGui::GetTextLineHeight() * 0.5f), 4.0f, col);
    dl->AddCircle(ImVec2(p.x + 5, p.y + ImGui::GetTextLineHeight() * 0.5f), 6.5f, col & 0x00ffffff | 0x60000000);
    ImGui::Dummy(ImVec2(14, 1));
    ImGui::SameLine(0, 6);
    ImGui::TextUnformatted(text);
    ImGui::Dummy(ImVec2(1, 1));
}

static bool ToggleSwitch(const char* id, bool* on) {
    ImGui::PushID(id);
    ImVec2 size(44, 21);
    bool clicked = ImGui::InvisibleButton("##sw", size);
    if (clicked) *on = !*on;
    bool hot = ImGui::IsItemHovered() && !clicked;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetItemRectMin();
    ImVec2 q(p.x + size.x, p.y + size.y);
    float r = size.y * 0.5f;
    ImU32 trackBg  = *on ? IM_COL32(70, 20, 20, 255)  : IM_COL32(28, 31, 40, 255);
    ImU32 trackCol = *on ? ACCENT : IM_COL32(74, 79, 94, 255);
    if (hot) trackBg = *on ? IM_COL32(86, 24, 24, 255) : IM_COL32(34, 38, 48, 255);
    dl->AddRectFilled(p, q, trackBg, r);
    dl->AddRect(p, q, trackCol, r, 0, 1.4f);
    float kx = *on ? q.x - r : p.x + r;
    ImU32 knob = IM_COL32(238, 240, 246, 255);
    dl->AddCircleFilled(ImVec2(kx, p.y + size.y * 0.5f), r - 4.0f, knob);
    ImGui::PopID();
    return clicked;
}

static const char* VkToName(int vk) {
    switch (vk) {
    case 0: return "—";
    case VK_BACK: return "Backspace"; case VK_TAB: return "Tab"; case VK_RETURN: return "Enter";
    case VK_SHIFT: return "Shift"; case VK_CONTROL: return "Ctrl"; case VK_MENU: return "Alt";
    case VK_CAPITAL: return "Caps"; case VK_ESCAPE: return "Esc"; case VK_SPACE: return "Space";
    case VK_PRIOR: return "PgUp"; case VK_NEXT: return "PgDn"; case VK_END: return "End";
    case VK_HOME: return "Home"; case VK_LEFT: return "Left"; case VK_UP: return "Up";
    case VK_RIGHT: return "Right"; case VK_DOWN: return "Down"; case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    }
    static char buf[8][8];
    static int bi = 0;
    char* t = buf[bi]; bi = (bi + 1) % 8;
    t[0] = 0;
    if (vk >= 'A' && vk <= 'Z') { t[0] = (char)vk; }
    else if (vk >= '0' && vk <= '9') { t[0] = (char)vk; }
    else if (vk >= VK_F1 && vk <= VK_F12) { sprintf(t, "F%d", vk - VK_F1 + 1); }
    else t[0] = '?';
    return t;
}

// ---------------------------------------------------------------- keybind capture
static void CaptureUpdate(void) {
    if (g_capture < 0) return;
    static int prev[256] = {0};
    for (int i = 0; i < 256; i++) {
        int now = (GetAsyncKeyState(i) & 0x8000) != 0;
        if (now && !prev[i]) {
            if (g_capture == 0) g_cfg.keyMenu = i;
            else                g_cfg.keyFeat[g_capture - 1] = i;
            g_capture = -1;
            Config_SaveKeybinds();
            break;
        }
        prev[i] = now;
    }
}

// ---------------------------------------------------------------- tabs
static void TabFeatures(void) {
    for (int i = 0; i < FEAT_COUNT; i++) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(C4(i == FEAT_WALLHACK ? ACCENT : TEXT_MAIN), "%s", FEAT_LABELS[i]);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 44 - 8, 0);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1);
        ToggleSwitch(FEAT_LABELS[i], (bool*)&g_cfg.feat[i]);
        if (i == FEAT_SPEED && g_cfg.feat[FEAT_SPEED]) {
            ImGui::Indent(18);
            ImGui::SliderFloat("Speed", &g_cfg.speedMul, 1.0f, 5.0f, "x %.1f");
            ImGui::Unindent(18);
        }
        if (i < FEAT_COUNT - 1) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(p.x, p.y + 1), ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + 1), LINE_COL, 1.0f);
            ImGui::Dummy(ImVec2(1, 3));
        }
    }
}

static void TabKeys(void) {
    CaptureUpdate();
    // menu key row
    const char* rows[FEAT_COUNT + 1];
    int* keys[FEAT_COUNT + 1];
    rows[0] = "Menu";  keys[0] = &g_cfg.keyMenu;
    for (int i = 0; i < FEAT_COUNT; i++) { rows[i+1] = FEAT_LABELS[i]; keys[i+1] = &g_cfg.keyFeat[i]; }

    for (int r = 0; r < FEAT_COUNT + 1; r++) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(rows[r]);
        ImGui::SameLine(0, 14);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6);
        char buf[160];
        if (g_capture == r) {
            snprintf(buf, sizeof(buf), "Press a key... (Esc = off)");
        } else {
            snprintf(buf, sizeof(buf), "%s", VkToName(*keys[r]));
        }
        ImGui::PushStyleColor(ImGuiCol_Button, g_capture == r ? ACCENT_DK : IM_COL32(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text, g_capture == r ? ACCENT : TEXT_DIM);
        if (ImGui::Button(buf, ImVec2(190, 22))) {
            if (g_capture == r) {
                // clicking again cancels
                g_capture = -1;
            } else {
                *keys[r] = 0;
                g_capture = r;
            }
        }
        ImGui::PopStyleColor(2);
        if (r < FEAT_COUNT) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(p.x, p.y + 1), ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + 1), LINE_COL, 1.0f);
            ImGui::Dummy(ImVec2(1, 3));
        }
    }
    // Esc handling during capture
    if (g_capture >= 0 && (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
        if (g_capture == 0) g_cfg.keyMenu = VK_DELETE;   // back to default
        else                g_cfg.keyFeat[g_capture - 1] = 0;
        g_capture = -1;
        Config_SaveKeybinds();
    }
}

static void TabSettings(void) {
    ImGui::TextColored(C4(TEXT_DIM), "Key file");
    ImGui::TextWrapped("%s", g_cfg.keyFile);
    ImGui::Spacing();
    ImGui::TextColored(C4(TEXT_DIM), "Key");
    ImGui::Text("%s", g_cfg.key[0] ? g_cfg.key : "(empty)");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (g_keyValid)
        StatusDot(OK_GREEN, "Key check: OK");
    else if (g_cfg.keyCheckUrl[0])
        StatusDot(IM_COL32(226, 70, 70, 255), "Key check: FAILED");
    else
        StatusDot(TEXT_DIM, "Key check: off (local mode)");
    if (g_luaReady)
        StatusDot(OK_GREEN, "Lua payload: injected");
    else
        StatusDot(WARN_YEL, "Lua payload: waiting...");
    if (g_hookInstalled)
        StatusDot(OK_GREEN, "Present hook: active");
    else
        StatusDot(WARN_YEL, "Present hook: waiting...");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("Save config", ImVec2(-1, 26))) {
        Config_SaveKeybinds();
        Config_SaveKey();
    }
    ImGui::TextColored(C4(TEXT_DIM), "Config dir:");
    ImGui::TextWrapped("%s", ArchDir());
}

static void TabAbout(void) {
    ImGui::Dummy(ImVec2(1, 8));
    // centered title block
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::PushFont(g_fontTitle);
    float tw = ImGui::CalcTextSize("ARCHANGEL").x;
    ImGui::PopFont();
    ImGui::SetCursorPosX((w - tw) * 0.5f);
    ImGui::PushFont(g_fontTitle);
    ImGui::TextColored(C4(ACCENT), "ARCHANGEL");
    ImGui::PopFont();
    float subw = ImGui::CalcTextSize("for MTA:SA").x;
    ImGui::SetCursorPosX((w - subw) * 0.5f);
    ImGui::TextColored(C4(TEXT_DIM), "for MTA:SA");
    ImGui::Dummy(ImVec2(1, 12));
    const char* info[][2] = {
        { "Version", ARCHANGEL_VERSION },
        { "Author",  ARCHANGEL_AUTHOR },
        { "ImGui",   IMGUI_VERSION },
        { "Build",   "rebuild v1" },
    };
    for (int i = 0; i < 4; i++) {
        float l = ImGui::CalcTextSize(info[i][0]).x;
        ImGui::SetCursorPosX((w - 200) * 0.5f + (100 - l));
        ImGui::TextColored(C4(TEXT_DIM), "%s", info[i][0]);
        ImGui::SameLine(0, 10);
        float v = ImGui::CalcTextSize(info[i][1]).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12 + (100 - l - v));
        ImGui::Text("%s", info[i][1]);
    }
    ImGui::Dummy(ImVec2(1, 12));
    ImGui::PushFont(g_fontSection);
    ImGui::TextColored(C4(TEXT_DIM), "Rebuilt from decompiled original (DLL.dll.c)");
    ImGui::PopFont();
}

// ---------------------------------------------------------------- main frame
void Menu_Frame(void) {
    ThemeApply();

    ImGui::SetNextWindowSize(ImVec2(372, 486), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                              ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
    if (!ImGui::Begin("##ArchangelWin", (bool*)&g_cfg.menuOpen, flags)) {
        ImGui::End();
        return;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 wsz(ImGui::GetWindowSize());
    ImVec2 wr(wp.x + wsz.x, wp.y + wsz.y);

    // window frame + top accent line
    dl->AddRect(ImVec2(wp.x + 0.5f, wp.y + 0.5f), ImVec2(wr.x - 0.5f, wr.y - 0.5f),
                IM_COL32(60, 64, 80, 255), 12.0f, 0, 1.5f);
    dl->AddLine(ImVec2(wp.x + 14, wp.y + 0.5f), ImVec2(wr.x - 14, wp.y + 0.5f), ACCENT, 2.0f);

    // ----- header (draggable) -----
    float headerH = 46;
    // drag area (under everything, created first so close button stays on top)
    {
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##hdr", ImVec2(wsz.x - 44, headerH));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            ImGui::SetWindowPos(ImVec2(wp.x + d.x, wp.y + d.y));
        }
    }
    // close button (custom x)
    {
        ImVec2 cb(wp.x + wsz.x - 34, wp.y + 11);
        if (ImGui::InvisibleButton("##close", ImVec2(24, 24))) g_cfg.menuOpen = 0;
        if (ImGui::IsItemHovered())
            dl->AddCircleFilled(cb + ImVec2(12, 12), 11.0f, IM_COL32(226, 54, 54, 200));
        dl->AddLine(cb + ImVec2(7, 7),  cb + ImVec2(17, 17), TEXT_MAIN, 2.0f);
        dl->AddLine(cb + ImVec2(17, 7), cb + ImVec2(7, 17),  TEXT_MAIN, 2.0f);
    }
    // title text (on top, not interactive)
    ImGui::SetCursorPos(ImVec2(16, 9));
    {
        ImGui::PushFont(g_fontTitle);
        ImGui::TextColored(C4(ACCENT), "ARCHANGEL");
        ImGui::PopFont();
        ImGui::SameLine(0, 10);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 7);
        ImGui::TextColored(C4(TEXT_DIM), "MTA:SA");
        float vw = ImGui::CalcTextSize("v" ARCHANGEL_VERSION).x;
        ImGui::SetCursorScreenPos(ImVec2(wp.x + wsz.x - 44 - vw - 12, wp.y + 15));
        ImGui::TextColored(C4(TEXT_DIM), "v%s", ARCHANGEL_VERSION);
    }
    dl->AddLine(ImVec2(wp.x + 1, wp.y + headerH), ImVec2(wr.x - 1, wp.y + headerH), LINE_COL, 1.0f);

    // ----- body: nav rail + content -----
    ImGui::SetCursorPos(ImVec2(10, headerH + 10));
    // nav rail
    {
        ImVec2 base = ImGui::GetCursorScreenPos();
        const char* navids[4] = { "##nav0", "##nav1", "##nav2", "##nav3" };
        for (int i = 0; i < 4; i++) {
            ImVec2 p(base.x + i * 50, base.y);
            dl->AddRectFilled(p, ImVec2(p.x + 44, p.y + 44),
                              g_tab == i ? IM_COL32(40, 22, 24, 255) : IM_COL32(28, 31, 40, 255),
                              10.0f);
            if (g_tab == i)
                dl->AddRect(p, ImVec2(p.x + 44, p.y + 44), ACCENT, 10.0f, 0, 1.4f);
            else if (ImGui::IsMouseHoveringRect(p, ImVec2(p.x + 44, p.y + 44)))
                dl->AddRect(p, ImVec2(p.x + 44, p.y + 44), IM_COL32(90, 96, 114, 255), 10.0f, 0, 1.2f);
            ImGui::SetCursorScreenPos(p);
            if (ImGui::InvisibleButton(navids[i], ImVec2(44, 44))) g_tab = i;
            Icon(dl, i, base + ImVec2(i * 50 + 22, 22),
                 g_tab == i ? ACCENT : TEXT_DIM);
        }
    }
    // content panel
    {
        ImGui::SameLine(0, 10);
        float avail = wsz.x - 10 - 4*50 + 6 - 10 - 14;
        if (avail < 180) avail = 180;
        ImGui::BeginChild("##content", ImVec2(avail, wsz.y - headerH - 10 - 34), 0,
                          ImGuiWindowFlags_NoScrollbar);
        ImDrawList* dlw = ImGui::GetWindowDrawList();
        ImVec2 cp = ImGui::GetWindowPos();
        ImVec2 cq(ImGui::GetWindowSize());
        dlw->AddRect(ImVec2(cp.x + 0.5f, cp.y + 0.5f),
                     ImVec2(cp.x + cq.x - 0.5f, cp.y + cq.y - 0.5f),
                     IM_COL32(50, 54, 68, 255), 10.0f, 0, 1.2f);
        ImGui::SetCursorPos(ImVec2(14, 12));
        if (g_tab == 0) TabFeatures();
        else if (g_tab == 1) TabKeys();
        else if (g_tab == 2) TabSettings();
        else TabAbout();
        ImGui::EndChild();
    }

    // ----- footer -----
    {
        float fy = wsz.y - 22;
        dl->AddLine(ImVec2(wp.x + 1, wp.y + fy), ImVec2(wr.x - 1, wp.y + fy), LINE_COL, 1.0f);
        const char* foot = "v" ARCHANGEL_VERSION "  •  " ARCHANGEL_AUTHOR;
        float fw = ImGui::CalcTextSize(foot).x;
        ImGui::SetCursorScreenPos(ImVec2(wp.x + (wsz.x - fw) * 0.5f, wp.y + fy + 3));
        ImGui::TextColored(C4(TEXT_DIM), "%s", foot);
    }
    ImGui::End();
}
