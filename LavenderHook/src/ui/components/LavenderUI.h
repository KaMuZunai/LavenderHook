#pragma once
#include "../../imgui/imgui.h"
#include <string>

namespace LavenderHook::UI::Lavender {

    // Buttons
    bool Button(const char* label, const ImVec2& size = ImVec2(0, 0));
    bool ToggleButton(const char* label, bool* v,
        const ImVec2& size = ImVec2(0, 0));
    bool HotkeyButton(const char* shownText,
        const ImVec2& size = ImVec2(0, 0));

    // Core Input Widgets
    bool Checkbox(const char* label, bool* v);
    bool SliderFloat(const char* label, float* v,
        float min, float max, const char* fmt = "%.3f",
        ImGuiSliderFlags flags = 0);
    bool SliderInt(const char* label, int* v,
        int min, int max, const char* fmt = "%d",
        ImGuiSliderFlags flags = 0);

    // Layout Helpers
    void Separator(float thickness = 1.0f);

    ImU32 PolishedAccent(float alpha = 1.0f);

    void PolishedPanel(ImDrawList* dl, const ImVec2& p0, const ImVec2& p1,
                       float rounding, float alpha,
                       bool leftAccentBar = true, float sheenHeight = 0.0f);

    void DrawWindowShadow(const ImVec2& pos, const ImVec2& size, float alpha = 1.0f);
}
