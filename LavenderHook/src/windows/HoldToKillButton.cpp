#include "HoldToKillButton.h"
#include "../misc/Globals.h"
#include "../imgui/imgui.h"
#include "../ui/components/LavenderFadeOut.h"
#include "../ui/GUI.h"

#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>

static void KillProcess(DWORD pid)
{
    if (!pid) {
        return;
    }

    // If target is current process we can terminate directly.
    DWORD selfPid = GetCurrentProcessId();
    if (pid == selfPid) {
        TerminateProcess(GetCurrentProcess(), 0);
        return;
    }

    // open process and call TerminateProcess.
    HANDLE hProc = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc) {
        if (TerminateProcess(hProc, 0)) {
            CloseHandle(hProc);
            return;
        }
        else {
            CloseHandle(hProc);
        }
    }
    else {
    }

    // Fallback
    std::string cmd = "cmd.exe /C taskkill /PID " + std::to_string(pid) + " /T /F";

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> buf(cmd.begin(), cmd.end());
    buf.push_back('\0');

    if (CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        // Wait a short time for taskkill to finish.
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

static float EaseInOut(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

namespace LavenderHook {
    namespace UI {
        namespace Widgets {

            // Fade controller
            static LavenderHook::UI::LavenderFadeOut g_fade;

            void RenderHoldToKillButton(bool wantVisible)
            {
                static float rawProgress = 0.0f;
                static float easedProgress = 0.0f;
                static bool armed = false;
                static DWORD cached_pid = 0;

                // Tick fade every frame
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
                const ImVec2 size(180.f * s, 28.f * s);
                const float margin = 14.f;
                const float round = 6.f * s;

                const float chargeTime = 2.f;
                const float drainTime = 1.f;

                // Refresh cached_pid when unknown or when window handle available
                if (LavenderHook::Globals::window_handle)
                {
                    DWORD pid = 0;
                    GetWindowThreadProcessId(LavenderHook::Globals::window_handle, &pid);
                    if (pid != 0)
                    {
                        cached_pid = pid;
                    }
                }

                ImGuiIO& io = ImGui::GetIO();
                ImVec2 anchor(
                    io.DisplaySize.x - margin,
                    io.DisplaySize.y - margin
                );

                ImGui::SetNextWindowPos(anchor, ImGuiCond_Always, ImVec2(1.f, 1.f));
                ImGui::SetNextWindowSize(size, ImGuiCond_Always);

                const ImGuiWindowFlags flags =
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                    ImGuiWindowFlags_NoBackground;

                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

                if (!ImGui::Begin("##AH_KillSwitch", nullptr, flags)) {
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

                ImGui::InvisibleButton("##hold_kill_btn", winSize);

                bool hovered = ImGui::IsItemHovered();
                bool held = hovered && ImGui::IsMouseDown(0);

                float dt = io.DeltaTime;

                rawProgress += held ? (dt / chargeTime) : -(dt / drainTime);
                rawProgress = std::clamp(rawProgress, 0.f, 1.f);

                easedProgress = EaseInOut(rawProgress);

                // Kill immediately when fully charged.
                if (rawProgress >= 1.f && !armed) {
                    armed = true;
                    KillProcess(cached_pid);
                }

                if (rawProgress <= 0.001f && !held)
                    armed = false;

                if (alpha < 0.98f)
                    ImGui::EndDisabled();

                ImU32 col_border = ImGui::GetColorU32(ImGuiCol_Border);

                // Scale background alpha
                ImVec4 bg = ImVec4(0, 0, 0, 0.65f * alpha);
                ImU32 col_bg = ImGui::GetColorU32(bg);

                ImU32 col_fill = ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
                ImU32 col_text = IM_COL32(255, 255, 255, (int)(255.0f * alpha));

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


                if (easedProgress > 0.f)
                {
                    float center = p0.x + winSize.x * 0.5f;
                    float halfFill = (winSize.x * 0.5f) * easedProgress;

                    ImVec2 f0(center - halfFill, p0.y);
                    ImVec2 f1(center + halfFill, p1.y);

                    dl->AddRectFilled(f0, f1, col_fill, round);
                }

                const char* label = held ? "EXITING..." : "EXIT";
                ImVec2 ts = ImGui::CalcTextSize(label);
                ImVec2 tc(p0.x + (winSize.x - ts.x) * 0.5f, p0.y + (winSize.y - ts.y) * 0.5f);
                dl->AddText(tc, col_text, label);

                ImGui::End();
                ImGui::PopStyleVar(4);
            }

        }
    }
}
