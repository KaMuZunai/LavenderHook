#include "lavenderui.h"
#include "../GUI.h"
#include "../../misc/Globals.h"
#include "../../imgui/imgui_internal.h"

static float Animate(float current, float target, float speed = 12.0f)
{
    return current + (target - current) * ImGui::GetIO().DeltaTime * speed;
}


namespace LavenderHook::UI::Lavender {

    static inline const ImVec4& C(ImGuiCol idx) {
        return ImGui::GetStyle().Colors[idx];
    }

    // use global
    static inline float Border() {
        return ::WINDOW_BORDER_SIZE;
    }


    struct ButtonScope
    {
        ButtonScope(ImGuiCol b, ImGuiCol h, ImGuiCol a, ImGuiCol t = ImGuiCol_Text)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, Border());
            ImGui::PushStyleColor(ImGuiCol_Button, C(b));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, C(h));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, C(a));
            ImGui::PushStyleColor(ImGuiCol_Text, C(t));
        }
        ~ButtonScope() {
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(1);
        }
    };

    bool Button(const char* label, const ImVec2& size) {
        ButtonScope s(ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive);
        return ImGui::Button(label, size);
    }

    bool ToggleButton(const char* label, bool* v, const ImVec2& size) {
        ImGuiCol b = *v ? ImGuiCol_Button : ImGuiCol_FrameBg;
        ImGuiCol h = *v ? ImGuiCol_ButtonHovered : ImGuiCol_FrameBgHovered;
        ImGuiCol a = *v ? ImGuiCol_ButtonActive : ImGuiCol_FrameBgActive;
        ButtonScope s(b, h, a);
        if (ImGui::Button(label, size))
            *v = !*v;
        return *v;
    }

    bool HotkeyButton(const char* shownText, const ImVec2& size)
    {
        // Hotkey Size expansion
        ImGuiStyle& style = ImGui::GetStyle();
        float defaultWidth = size.x > 0.0f ? size.x : 70.0f;
        ImVec2 textSize = ImGui::CalcTextSize(shownText, NULL, true);
        float contentWidth = textSize.x + style.FramePadding.x * 2.0f;
        float maxWidth = defaultWidth * 2.2f;
        float finalWidth = defaultWidth;
        if (contentWidth > defaultWidth) {
            finalWidth = contentWidth;
            if (finalWidth > maxWidth) finalWidth = maxWidth;
        }
        ImVec2 finalSize(finalWidth, size.y);

        ImGui::PushID(shownText);

        ImGuiStorage* storage = ImGui::GetStateStorage();
        ImGuiID anim_id = ImGui::GetID("anim");

        float anim = storage->GetFloat(anim_id, 0.0f);

        // colors
        ImVec4 base = C(ImGuiCol_FrameBg);
        ImVec4 hover = C(ImGuiCol_FrameBgHovered);
        ImVec4 active = C(ImGuiCol_FrameBgActive);

        ImVec4 blended = ImLerp(base, hover, anim);

        ImGui::PushStyleColor(ImGuiCol_Button, blended);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, blended);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, blended);

        bool pressed = ImGui::Button(shownText, finalSize);

        bool hovered = ImGui::IsItemHovered();
        bool held = ImGui::IsItemActive();

        float target =
            held ? 1.0f :
            hovered ? 0.6f :
            0.0f;

        anim = Animate(anim, target);
        storage->SetFloat(anim_id, anim);

        ImGui::PopStyleColor(3);
        ImGui::PopID();

        return pressed;
    }

    bool Checkbox(const char* label, bool* v) {
        ButtonScope s(*v ? ImGuiCol_Button : ImGuiCol_FrameBg,
            *v ? ImGuiCol_ButtonHovered : ImGuiCol_FrameBgHovered,
            *v ? ImGuiCol_ButtonActive : ImGuiCol_FrameBgActive);
        bool changed = ImGui::Checkbox(label, v);
        return changed;
    }

    bool SliderFloat(const char* label, float* v, float min, float max,
        const char* fmt, ImGuiSliderFlags flags) {
        ButtonScope s(ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive);
        return ImGui::SliderFloat(label, v, min, max, fmt, flags);
    }

    bool SliderInt(const char* label, int* v, int min, int max,
        const char* fmt, ImGuiSliderFlags flags) {
        ButtonScope s(ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive);
        return ImGui::SliderInt(label, v, min, max, fmt, flags);
    }

    void Separator(float thickness) {
        auto& s = ImGui::GetStyle();
        ImGui::GetWindowDrawList()->AddLine(
            ImGui::GetCursorScreenPos(),
            ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x,
                ImGui::GetCursorScreenPos().y),
            ImGui::ColorConvertFloat4ToU32(C(ImGuiCol_Separator)),
            thickness
        );
        ImGui::Dummy(ImVec2(0, thickness + s.ItemSpacing.y));
    }


    ImU32 PolishedAccent(float alpha) {
        const ImVec4& c = C(ImGuiCol_ButtonActive);
        return IM_COL32((int)(c.x * 255.f), (int)(c.y * 255.f),
                        (int)(c.z * 255.f), (int)(235.f * alpha));
    }

    void PolishedPanel(ImDrawList* dl, const ImVec2& p0, const ImVec2& p1,
                       float rounding, float alpha,
                       bool leftAccentBar, float sheenHeight)
    {
        if (!dl) return;

        // soft drop shadow
        dl->AddRectFilled(
            ImVec2(p0.x + 1.f, p0.y + 3.f),
            ImVec2(p1.x + 1.f, p1.y + 5.f),
            IM_COL32(0, 0, 0, (int)(90.f * alpha)), rounding);

        // frosted dark fill
        dl->AddRectFilled(p0, p1, IM_COL32(22, 20, 28, (int)(235.f * alpha)), rounding);

        // top sheen band
        if (sheenHeight > 0.f) {
            dl->AddRectFilled(
                p0, ImVec2(p1.x, p0.y + sheenHeight),
                IM_COL32(255, 255, 255, (int)(9.f * alpha)),
                rounding, ImDrawFlags_RoundCornersTop);
        }

        // faint accent border
        dl->AddRect(p0, p1, PolishedAccent(0.35f * alpha), rounding, 0, 1.f);

        // left accent bar
        if (leftAccentBar) {
            dl->AddRectFilled(
                p0, ImVec2(p0.x + 3.f, p1.y),
                PolishedAccent(0.90f * alpha),
                rounding, ImDrawFlags_RoundCornersLeft);
        }
    }

    void DrawWindowShadow(const ImVec2& pos, const ImVec2& size, float alpha)
    {
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        float r = ImGui::GetStyle().WindowRounding;
        float s = LavenderHook::Globals::menu_scale;

        bg->AddRectFilled(
            ImVec2(pos.x - 4.f * s, pos.y + 2.f * s),
            ImVec2(pos.x + size.x, pos.y + size.y + 5.f * s),
            IM_COL32(0, 0, 0, (int)(85.f * alpha)), r);
    }
}
