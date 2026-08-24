#pragma once
#include "../../imgui/imgui.h"
#include <string>

namespace LavenderHook::UI::Lavender {

    struct GradientStyle {
        ImGuiCol colorA;
        ImGuiCol colorB;
        float speed = 0.30f;
    };

    // Global style
    GradientStyle& Gradient();

    // Draw text with optional shadow offset (pixels, e.g. -1,1).
    // The shadow opacity follows the wave so it fades where the text is dim.
    void GradientText(const std::string& text, float alpha = 1.0f,
                      float shadowOffX = 0, float shadowOffY = 0);
}
