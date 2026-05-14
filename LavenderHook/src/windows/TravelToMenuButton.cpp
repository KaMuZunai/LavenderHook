#include "TravelToMenuButton.h"
#include "../misc/Globals.h"
#include "../imgui/imgui.h"
#include "../ui/GUI.h"
#include "../ui/components/LavenderFadeOut.h"
#include "../windows/TravelWindow.h"
#include <algorithm>

namespace LavenderHook {
    namespace UI {
        namespace Widgets {

            static float EaseInOut(float t)
            {
                t = std::clamp(t, 0.0f, 1.0f);
                return t * t * (3.0f - 2.0f * t);
            }

            static LavenderHook::UI::LavenderFadeOut g_fade;

            void RenderTravelToMenuButton(bool wantVisible)
            {
                static float rawProgress = 0.0f;
                static float easedProgress = 0.0f;
                static bool armed = false;

                g_fade.Tick(wantVisible);

                if (!g_fade.ShouldRender())
                {
                    rawProgress = 0.f;
                    easedProgress = 0.f;
                    armed = false;
                    return;
                }

                const float alpha = g_fade.Alpha();

                float s = LavenderHook::Globals::menu_scale;
                const ImVec2 size(180.0f * s, 28.0f * s);
                const float margin = 14.0f;
                const float gap = 8.0f;
                const float round = 6.0f * s;

                const float chargeTime = 1.5f;
                const float drainTime = 0.8f;

                ImGuiIO& io = ImGui::GetIO();

                // Positioned above EXIT button, below Settings button
                const ImVec2 pos(
                    io.DisplaySize.x - margin,
                    io.DisplaySize.y - margin - (size.y + gap)
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

                if (!ImGui::Begin("##AH_TravelMenuBtn", nullptr, flags)) {
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

                ImGui::InvisibleButton("##travel_btn", winSize);

                bool hovered = ImGui::IsItemHovered();
                bool held = hovered && ImGui::IsMouseDown(0);

                float dt = ImGui::GetIO().DeltaTime;

                rawProgress += held ? (dt / chargeTime) : -(dt / drainTime);
                rawProgress = std::clamp(rawProgress, 0.f, 1.f);

                easedProgress = EaseInOut(rawProgress);

                // Trigger travel when fully charged
                if (rawProgress >= 1.f && !armed) {
                    armed = true;
                    HWND hwnd = LavenderHook::Globals::window_handle;
                    if (hwnd)
                        PostMessageW(hwnd, LavenderHook::UI::Windows::TravelWindow::GetTravelMessageId(), 0, 0);
                }

                if (rawProgress <= 0.001f && !held)
                    armed = false;

                if (alpha < 0.98f)
                    ImGui::EndDisabled();

                const ImU32 col_border = ImGui::GetColorU32(ImGuiCol_Border);
                const ImU32 col_bg = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.65f * alpha));
                const ImU32 col_fill = ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
                const ImU32 col_text = IM_COL32(255, 255, 255, (int)(255.0f * alpha));

                dl->AddRectFilled(p0, p1, col_bg, round);
                if (WINDOW_BORDER_SIZE > 0.0f)
                {
                    dl->AddRect(p0, p1, col_border, round, 0, WINDOW_BORDER_SIZE);
                }

                if (easedProgress > 0.f)
                {
                    float center = p0.x + winSize.x * 0.5f;
                    float halfFill = (winSize.x * 0.5f) * easedProgress;

                    ImVec2 f0(center - halfFill, p0.y);
                    ImVec2 f1(center + halfFill, p1.y);

                    dl->AddRectFilled(f0, f1, col_fill, round);
                }

                const char* label = held ? "TRAVELING..." : "Main Menu";
                ImVec2 ts = ImGui::CalcTextSize(label);
                ImVec2 tc(p0.x + (winSize.x - ts.x) * 0.5f, p0.y + (winSize.y - ts.y) * 0.5f);
                dl->AddText(tc, col_text, label);

                ImGui::End();
                ImGui::PopStyleVar(4);
            }

        }
    }
}
