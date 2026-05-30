#include "LavenderGradient.h"
#include "../../imgui/imgui.h"
#include <cmath>

namespace LavenderHook::UI::Lavender {

    static GradientStyle gStyle{
        ImGuiCol_ButtonActive,
        ImGuiCol_ButtonActive,
        0.50f
    };

    GradientStyle& Gradient() { return gStyle; }

    static inline const ImVec4& C(ImGuiCol idx) {
        return ImGui::GetStyle().Colors[idx];
    }

    // Local fast clamp
    static inline float ClampF(float v, float lo, float hi) {
        return (v < lo) ? lo : (v > hi) ? hi : v;
    }

    void GradientText(const std::string& text, float alpha,
                      float shadowOffX, float shadowOffY) {

        if (text.empty()) {
            ImGui::TextUnformatted("");
            return;
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos0 = ImGui::GetCursorScreenPos();
        ImVec2 sz = ImGui::CalcTextSize(text.c_str());
        const float lineH = ImGui::GetTextLineHeight();

        const float invLen = 1.0f / (sz.x + lineH + 1e-6f);
        const float t = ImGui::GetTime() * gStyle.speed;
        const float TWO_PI = 6.28318530718f;

        const ImVec4 base = C(gStyle.colorA);
        const bool doShadow = (shadowOffX != 0 || shadowOffY != 0);

        float x = pos0.x;
        for (char c : text) {

            char buf[2] = { c, 0 };
            ImVec2 chSz = ImGui::CalcTextSize(buf);

            float along = ((x - pos0.x) + 0.5f * chSz.x + 0.5f * lineH) * invLen;
            float wave = 0.5f * (sinf(TWO_PI * (along - t)) + 1.0f);

            // Pulsing brightness
            float brightness = 0.25f + wave * 1.50f;

            if (doShadow) {
                float normBright = (brightness - 0.25f) / 1.50f;
                int shadowA = (int)(normBright * 90 * alpha);
                if (shadowA > 0)
                    dl->AddText(ImVec2(x + shadowOffX, pos0.y + shadowOffY),
                                IM_COL32(0, 0, 0, shadowA), buf);
            }

            ImVec4 col = {
                ClampF(base.x * brightness, 0.0f, 1.0f),
                ClampF(base.y * brightness, 0.0f, 1.0f),
                ClampF(base.z * brightness, 0.0f, 1.0f),
                base.w * alpha
            };

            dl->AddText(ImVec2(x, pos0.y), ImGui::GetColorU32(col), buf);
            x += chSz.x;
        }

        ImGui::Dummy(ImVec2(sz.x, lineH));
    }
}
