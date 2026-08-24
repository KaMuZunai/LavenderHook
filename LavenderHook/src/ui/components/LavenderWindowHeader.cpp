#include "LavenderWindowHeader.h"
#include "LavenderUI.h"
#include "../../imgui/imgui_internal.h"
#include "../../misc/Globals.h"
#include <cmath>

// Theme color
extern ImVec4 MAIN_RED;

namespace LavenderHook::UI::Lavender {

    static constexpr float kPi = 3.14159265358979323846f;

    static float Clamp(float v, float mn, float mx)
    {
        return v < mn ? mn : (v > mx ? mx : v);
    }

    static float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    static ImU32 LerpColor(ImU32 a, ImU32 b, float t)
    {
        ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
        ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);

        ImVec4 c(
            Lerp(ca.x, cb.x, t),
            Lerp(ca.y, cb.y, t),
            Lerp(ca.z, cb.z, t),
            Lerp(ca.w, cb.w, t)
        );

        return ImGui::ColorConvertFloat4ToU32(c);
    }

    bool RenderWindowHeader(
        const char* title,
        ImTextureID headerIcon,
        ImTextureID arrowIcon,
        float width,
        float fadeAlpha,
        bool& headerOpen,
        float& headerAnim,
        float& arrowAnim)
    {
    float sc = LavenderHook::Globals::menu_scale;
    const float headerHeight = 32.0f * sc;
    const float arrowWidth = 26.0f * sc;
    const float iconSize = 18.0f * sc;
    const float padding = 8.0f * sc;

        ImVec2 winPos = ImGui::GetWindowPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        bool polished = LavenderHook::Globals::use_polished_overlay;
        float r = ImGui::GetStyle().WindowRounding;

        if (polished) {
            // In polished mode the window background is already frosted dark.
            // Just add a subtle top sheen and an accent divider under the header.
            dl->AddRectFilled(
                ImVec2(winPos.x, winPos.y),
                ImVec2(winPos.x + width, winPos.y + headerHeight),
                IM_COL32(255, 255, 255, (int)(9.f * fadeAlpha)),
                r, ImDrawFlags_RoundCornersTop);

            dl->AddLine(
                ImVec2(winPos.x + 1.0f, winPos.y + headerHeight),
                ImVec2(winPos.x + width - 1.0f, winPos.y + headerHeight),
                PolishedAccent(0.55f * fadeAlpha), 1.0f);
        }
        else {
            // blended background
            ImU32 titleBg = ImGui::GetColorU32(ImGuiCol_TitleBg);
            ImU32 frameBg = ImGui::GetColorU32(ImGuiCol_FrameBg);
            ImU32 bg = LerpColor(titleBg, frameBg, 0.55f);

            dl->AddRectFilled(
                ImVec2(winPos.x, winPos.y),
                ImVec2(winPos.x + width, winPos.y + headerHeight),
                bg,
                r,
                ImDrawFlags_RoundCornersTop
            );
        }


        // interaction zone
        ImGui::SetCursorScreenPos(winPos);
        ImGui::InvisibleButton("##header", ImVec2(width, headerHeight));

        bool clicked = ImGui::IsItemClicked();
        bool overArrow =
            ImGui::GetIO().MousePos.x >= (winPos.x + width - arrowWidth);

        if (clicked && overArrow)
            headerOpen = !headerOpen;

        // drag window
        if (ImGui::IsItemActive() && !overArrow) {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            ImGui::SetWindowPos(ImVec2(winPos.x + d.x, winPos.y + d.y));
        }

        // icon
        if (headerIcon) {
            ImVec2 p0(
                winPos.x + padding,
                winPos.y + (headerHeight - iconSize) * 0.5f
            );
            ImVec2 p1(p0.x + iconSize, p0.y + iconSize);

            // white plate
            const float inset = 1.0f;
            dl->AddRectFilled(
                ImVec2(p0.x + inset, p0.y + inset),
                ImVec2(p1.x - inset, p1.y - inset),
                ImGui::GetColorU32(ImVec4(1, 1, 1, 0.95f * fadeAlpha)),
                4.0f
            );

            // tinted icon
            ImVec4 tint(
                MAIN_RED.x,
                MAIN_RED.y,
                MAIN_RED.z,
                fadeAlpha
            );

            dl->AddImage(
                headerIcon,
                p0, p1,
                ImVec2(0, 0), ImVec2(1, 1),
                ImGui::GetColorU32(tint)
            );
        }

        // title
        ImVec2 ts = ImGui::CalcTextSize(title);
        ImVec2 tp(
            winPos.x + (width - ts.x) * 0.5f,
            winPos.y + (headerHeight - ts.y) * 0.5f
        );

        ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);

        if (polished) {
            dl->AddText(tp, textCol, title);
        } else {
            dl->AddText(ImVec2(tp.x + 1, tp.y), textCol, title);
            dl->AddText(ImVec2(tp.x - 1, tp.y), textCol, title);
            dl->AddText(ImVec2(tp.x, tp.y + 1), textCol, title);
            dl->AddText(ImVec2(tp.x, tp.y - 1), textCol, title);
            dl->AddText(tp, textCol, title);
        }

        // arrow
        float target = headerOpen ? 1.0f : 0.0f;
        arrowAnim += (target - arrowAnim) * ImGui::GetIO().DeltaTime * 10.0f;
        arrowAnim = Clamp(arrowAnim, 0.0f, 1.0f);

        if (arrowIcon) {
            float icon = headerHeight * 0.55f;
            constexpr float kHeaderArrowInset = 8.0f; // offset

            ImVec2 c(
                winPos.x + width - arrowWidth * 0.5f - kHeaderArrowInset,
                winPos.y + headerHeight * 0.5f
            );

            float a = -arrowAnim * kPi * 0.5f;
            float s = std::sinf(a);
            float c2 = std::cosf(a);

            ImVec2 h(icon * 0.5f, icon * 0.5f);
            ImVec2 v[4] = {
                {-h.x,-h.y},{h.x,-h.y},{h.x,h.y},{-h.x,h.y}
            };

            for (auto& p : v)
                p = ImVec2(
                    c.x + p.x * c2 - p.y * s,
                    c.y + p.x * s + p.y * c2
                );

            dl->AddImageQuad(
                arrowIcon,
                v[0], v[1], v[2], v[3],
                ImVec2(0, 0), ImVec2(1, 0),
                ImVec2(1, 1), ImVec2(0, 1),
                ImGui::GetColorU32(ImVec4(1, 1, 1, fadeAlpha))
            );
        }

        ImGui::Dummy(ImVec2(0.0f, 4.0f * sc));
        return headerOpen;
    }

}
