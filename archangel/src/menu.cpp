// ImGui menu (original menu UI was ImGui 1.92.5 + "Taran" window)
#include "archangel.h"
#include <stdio.h>
#include <string.h>

#include "imgui.h"
#include "backends/imgui_impl_dx9.h"

static int g_tab = 0;
static int g_vkeyMenu = 0;
static int g_vkeyFeat[FEAT_COUNT] = {0};
static const char* g_featNames[FEAT_COUNT] = {
    "God Mode", "Speed", "Health (auto)", "Armor (auto)", "Noclip", "Wallhack (ESP)"
};

static int VKeyPopup(int* val) {
    *val = 0;
    const char* names[] = { "None","0","1","2","3","4","5","6","7","8","9",
        "A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
        "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12","Insert","Home","PgUp","Del","End","PgDn",
        "Up","Down","Left","Right","Space","Enter","Backspace","Tab","Escape","CapsLock","NumLock","Scroll" };
    for (int i = 0; i < 76; i++) {
        int vk = 0;
        if (i == 0) vk = 0;
        else if (i <= 10) vk = '0' + (i - 1);
        else if (i <= 36) vk = 'A' + (i - 11);
        else if (i <= 48) vk = VK_F1 + (i - 37);
        else {
            static const int special[] = { VK_INSERT, VK_HOME, VK_PRIOR, VK_DELETE, VK_END, VK_NEXT,
                                           VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_SPACE, VK_RETURN,
                                           VK_BACK, VK_TAB, VK_ESCAPE, VK_CAPITAL, VK_NUMLOCK, VK_SCROLL };
            vk = special[i - 49];
        }
        if (ImGui::Selectable(names[i], *val == vk && i != 0)) { *val = vk; return 1; }
    }
    (void)g_vkeyMenu;
    return 0;
}

void Menu_Frame(void) {
    ImGui::SetNextWindowSize(ImVec2(340, 470), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Archangel  -  MTA", (bool*)&g_cfg.menuOpen, ImGuiWindowFlags_NoCollapse)) {
        // header
        ImGui::TextColored(ImVec4(0.85f, 0.2f, 0.2f, 1.0f), "ARCHANGEL v%s", ARCHANGEL_VERSION);
        ImGui::Text("by %s", ARCHANGEL_AUTHOR);
        ImGui::Separator();
        if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Features")) {
                g_tab = 0;
                for (int i = 0; i < FEAT_COUNT; i++) {
                    ImGui::Checkbox(g_featNames[i], (bool*)&g_cfg.feat[i]);
                }
                if (g_cfg.feat[FEAT_SPEED])
                    ImGui::SliderFloat("Speed x", &g_cfg.speedMul, 1.0f, 5.0f, "%.1f");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Keybinds")) {
                g_tab = 1;
                if (g_vkeyMenu == 0 && ImGui::Button("Change menu key")) g_vkeyMenu = 1;
                else if (g_vkeyMenu) {
                    ImGui::SameLine();
                    ImGui::Text("menu:");
                    VKeyPopup(&g_vkeyMenu);
                    g_cfg.keyMenu = g_vkeyMenu ? g_vkeyMenu : VK_INSERT;
                    g_vkeyMenu = 0;
                    Config_SaveKeybinds();
                } else {
                    ImGui::Text("Menu key: set in keybinds.cfg (default INSERT)");
                }
                for (int i = 0; i < FEAT_COUNT; i++) {
                    char label[64];
                    snprintf(label, sizeof(label), "%-16s: %s", g_featNames[i],
                             g_cfg.keyFeat[i] ? "bound" : "-");
                    if (ImGui::Selectable(label)) {
                        // quick toggle bind: use F1..F6 for convenience
                        g_cfg.keyFeat[i] = g_cfg.keyFeat[i] ? 0 : (VK_F1 + i);
                        Config_SaveKeybinds();
                    }
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Settings")) {
                g_tab = 2;
                ImGui::Text("Key file:");
                ImGui::TextWrapped("%s", g_cfg.keyFile);
                ImGui::Text("Key: %s", g_cfg.key[0] ? g_cfg.key : "(empty)");
                ImGui::Text("Key check: %s",
                    g_keyValid ? "OK" : (g_cfg.keyCheckUrl[0] ? "FAILED / not run" : "disabled (local mode)"));
                ImGui::Text("Lua payload: %s", g_luaReady ? "injected" : "waiting...");
                ImGui::Text("Present hook: %s", g_hookInstalled ? "installed" : "waiting...");
                ImGui::Separator();
                ImGui::Text("keybinds.cfg / settings.cfg are saved in:");
                ImGui::TextWrapped("%s", ArchDir());
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("About")) {
                g_tab = 3;
                ImGui::Text("Archangel v%s for MTA:SA", ARCHANGEL_VERSION);
                ImGui::Text("Author: %s", ARCHANGEL_AUTHOR);
                ImGui::Text("ImGui %s", IMGUI_VERSION);
                ImGui::Separator();
                ImGui::TextWrapped("Rebuilt from decompiled original (DLL.dll.c).");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None))
            g_cfg.menuOpen = 1; // keep while interacting (close via X)
    }
    ImGui::End();
}
