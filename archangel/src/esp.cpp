// Wallhack / ESP: boxes + names + distances.
// Screen coords & distance come from the MTA Lua side (ar_pushPlayer),
// so projection math stays in Lua (getScreenFromWorldPosition).
#include "archangel.h"
#include <stdio.h>

#include "imgui.h"

static const ImU32 c_box    = IM_COL32(255, 80,  80, 255);
static const ImU32 c_boxMe  = IM_COL32(120, 220,120, 200);
static const ImU32 c_text   = IM_COL32(255, 255, 255, 255);
static const ImU32 c_hpBg   = IM_COL32( 40,  40,  40, 180);
static const ImU32 c_hpFg   = IM_COL32( 80, 255, 80, 255);

void ESP_Frame(void) {
    if (g_espCount == 0) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float fontH = ImGui::GetFontSize();
    for (int i = 0; i < g_espCount; i++) {
        EspPlayer* e = &g_esp[i];
        if (!e->valid) continue;
        if (e->sx < -50 || e->sy < -50) continue;            // off-screen (Lua may send -1,-1)
        if (e->dist < 1.0f) e->dist = 1.0f;
        if (e->dist > 600.0f) continue;                      // too far

        float w = 900.0f / e->dist;   if (w < 10.0f) w = 10.0f; if (w > 140.0f) w = 140.0f;
        float h = 2200.0f / e->dist;  if (h < 24.0f) h = 24.0f; if (h > 320.0f) h = 320.0f;
        ImU32 col = e->isLocal ? c_boxMe : c_box;

        float x0 = e->sx - w * 0.5f, y0 = e->sy - h * 0.5f;
        dl->AddRect(ImVec2(x0, y0), ImVec2(x0 + w, y0 + h), col, 0.0f, 0, 1.5f);
        // head marker
        dl->AddLine(ImVec2(e->sx - 4, y0), ImVec2(e->sx + 4, y0), col, 2.0f);

        // name
        char txt[64];
        snprintf(txt, sizeof(txt), "%s  [%d m]", e->name[0] ? e->name : "?", (int)(e->dist * 3.0f));
        float tw = ImGui::CalcTextSize(txt).x;
        dl->AddText(ImVec2(e->sx - tw * 0.5f, y0 - fontH * 1.2f), c_text, txt);

        // health bar
        float barH = h * (e->health / 100.0f);
        if (barH < 0) barH = 0;
        dl->AddRectFilled(ImVec2(x0 - 6, y0 + h - barH), ImVec2(x0 - 2, y0 + h), c_hpBg, 0.0f);
        dl->AddRectFilled(ImVec2(x0 - 6, y0 + h - barH), ImVec2(x0 - 2, y0 + h), c_hpFg, 0.0f);
    }
}
