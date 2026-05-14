#include "ToggleMenuButton.h"
#include "../misc/Globals.h"
#include "../imgui/imgui.h"
#include "../ui/components/LavenderFadeOut.h"
#include "../ui/GUI.h"

namespace LavenderHook {
    namespace UI {
        namespace Widgets {

            static float AnimateEase(float current, float target, float dt)
            {
                float speed = 10.0f;
                return current + (target - current) * dt * speed;
            }

            static LavenderHook::UI::LavenderFadeOut g_fade;

            void RenderMenuSelectorButton(bool wantVisible)
            {
                // Tick fade every frame
                g_fade.Tick(wantVisible);

                if (!g_fade.ShouldRender())
                    return;

                const float alpha = g_fade.Alpha();

                float s = LavenderHook::Globals::menu_scale;
                const ImVec2 size(180.0f * s, 28.0f * s);
                const float margin = 14.0f;
                const float gap_up = 8.0f;
                const float round = 6.0f * s;

                ImGuiIO& io = ImGui::GetIO();
                const ImVec2 pos(
                    io.DisplaySize.x - margin,
                    io.DisplaySize.y - margin - (size.y + gap_up) * 2
                );

                ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
                ImGui::SetNextWindowSize(size, ImGuiCond_Always);

                ImGuiWindowFlags flags =
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoFocusOnAppearing |
                    ImGuiWindowFlags_NoNav |
                    ImGuiWindowFlags_NoBackground |
                    ImGuiWindowFlags_NoBringToFrontOnFocus;

                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

                if (!ImGui::Begin("##AH_MenuSelectorBtn", nullptr, flags)) {
                    ImGui::End();
                    ImGui::PopStyleVar(4);
                    return;
                }

                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 winSize = ImGui::GetWindowSize();
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 p1 = ImVec2(p0.x + winSize.x, p0.y + winSize.y);

                if (alpha < 0.98f)
                    ImGui::BeginDisabled();

                ImGui::InvisibleButton("##menusel_btn", winSize);
                if (ImGui::IsItemClicked(0))
                    LavenderHook::Globals::show_menu_selector_window =
                    !LavenderHook::Globals::show_menu_selector_window;

                if (alpha < 0.98f)
                    ImGui::EndDisabled();

                bool active = LavenderHook::Globals::show_menu_selector_window;
                float dt = ImGui::GetIO().DeltaTime;

                static float anim = 0.0f;
                float target = active ? 1.0f : 0.0f;
                anim = AnimateEase(anim, target, dt);

                anim = (anim < 0.0f) ? 0.0f : (anim > 1.0f ? 1.0f : anim);

                const ImU32 col_border = ImGui::GetColorU32(ImGuiCol_Border);

                // Scale bg alpha
                const ImU32 col_bg = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.65f * alpha));

                const ImU32 col_fill = active
                    ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
                    : ImGui::GetColorU32(ImGuiCol_Button);

                dl->AddRectFilled(p0, p1, col_bg, round);
                if (WINDOW_BORDER_SIZE > 0.0f)
                {
                    dl->AddRect(
                        p0,
                        p1,
                        col_border,
                        round,
                        0,
                        WINDOW_BORDER_SIZE
                    );
                }


                float innerW = winSize.x - 2.0f;
                float fillW = innerW * anim;

                float cx = p0.x + winSize.x * 0.5f;
                float left = cx - fillW * 0.5f;
                float right = cx + fillW * 0.5f;

                if (anim > 0.001f) {
                    dl->AddRectFilled(
                        ImVec2(left, p0.y + 1),
                        ImVec2(right, p1.y - 1),
                        col_fill,
                        round - 1.0f
                    );
                }

                const char* label = "Settings";
                ImVec2 ts = ImGui::CalcTextSize(label);
                ImVec2 tc(cx - ts.x * 0.5f, p0.y + (winSize.y - ts.y) * 0.5f);

                dl->AddText(tc, IM_COL32(255, 255, 255, (int)(255.0f * alpha)), label);

                ImGui::End();
                ImGui::PopStyleVar(4);
            }

        }
    }
}
