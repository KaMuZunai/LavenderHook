#include "MenuLogoWindow.h"
#include "../../imgui/imgui.h"
#include "../../imgui/imgui_internal.h"
#include "../components/LavenderFadeOut.h"
#include "../../misc/Globals.h"
#include <cstdlib>

// girl icons
extern ImTextureID g_halloweenGirlTex;
extern ImTextureID g_necroGirlTex;
extern ImTextureID g_moonGirlTex;
extern ImTextureID g_snowGirlTex;
extern ImTextureID g_cloverGirlTex;
extern ImTextureID g_loveGirlTex;
extern ImTextureID g_orbGirlTex;
extern ImTextureID g_owlGirlTex;

namespace LavenderHook::UI::Windows {

    static LavenderHook::UI::LavenderFadeOut s_fade;
    static bool s_posInitialized = false;

    static ImTextureID s_currentIcon = 0;
    static bool s_lastVisible = false;
    static bool s_canRandomize = true;

    ImVec2 ImageDragWindow::s_pos = ImVec2(0.f, 0.f);
    ImVec2 ImageDragWindow::s_size = ImVec2(128.f, 128.f);

    void ImageDragWindow::Init()
    {
    }

    void ImageDragWindow::Render(bool visible)
    {
        // allow re-randomization only after full fade-out
        if (!visible && s_fade.Alpha() <= 0.001f)
            s_canRandomize = true;

        // randomize only when menu opens AND fade was fully completed
        if (visible && !s_lastVisible && s_canRandomize)
        {
            ImTextureID pool[9];
            int count = 0;

            auto push = [&](ImTextureID id) {
                if (id) pool[count++] = id;
                };

            push(g_halloweenGirlTex);
            push(g_necroGirlTex);
            push(g_moonGirlTex);
            push(g_snowGirlTex);
            push(g_cloverGirlTex);
            push(g_loveGirlTex);
            push(g_orbGirlTex);
            push(g_owlGirlTex);

            if (count > 0)
                s_currentIcon = pool[rand() % count];
            else
                s_currentIcon = 0;

            s_canRandomize = false;
        }

        s_lastVisible = visible;

        s_fade.Tick(visible);

        if (!s_fade.ShouldRender() || !s_currentIcon)
            return;

        const float fadeAlpha = s_fade.Alpha();

        // initialize bottom-left position once
        if (!s_posInitialized) {
            ImGuiIO& io = ImGui::GetIO();
            const float margin = 20.0f;

            s_pos.x = margin;
            s_pos.y = io.DisplaySize.y - s_size.y - margin;

            s_posInitialized = true;
        }

        ImGui::SetNextWindowPos(s_pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(s_size, ImGuiCond_Always);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
        ImGui::Begin("##MenuLogoDrag", nullptr, flags);

        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();

        ImDrawList* dl = ImGui::GetWindowDrawList();

        bool polished = LavenderHook::Globals::use_polished_overlay;

        // Shadow image (polished only) — background layer so it's not clipped by the window
        if (polished) {
            ImDrawList* bg = ImGui::GetBackgroundDrawList();
            bg->AddImage(
                s_currentIcon,
                ImVec2(winPos.x - 4, winPos.y + 2),
                ImVec2(winPos.x + winSize.x - 4, winPos.y + winSize.y + 2),
                ImVec2(0, 0),
                ImVec2(1, 1),
                IM_COL32(0, 0, 0, (int)(85 * fadeAlpha))
            );
        }

        ImU32 tint = ImGui::GetColorU32(
            ImVec4(1.f, 1.f, 1.f, fadeAlpha)
        );

        dl->AddImage(
            s_currentIcon,
            winPos,
            ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
            ImVec2(0, 0),
            ImVec2(1, 1),
            tint
        );

        // full drag area
        ImGui::SetCursorScreenPos(winPos);
        ImGui::InvisibleButton("##drag", winSize);

        if (ImGui::IsItemActive() &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            s_pos.x += ImGui::GetIO().MouseDelta.x;
            s_pos.y += ImGui::GetIO().MouseDelta.y;
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

}
