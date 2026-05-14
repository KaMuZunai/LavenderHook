#include "PerformanceOverlayWindow.h"
#include "../misc/Globals.h"
#include "../imgui/imgui.h"
#include "../ui/components/LavenderGradient.h"
#include "../ui/components/LavenderFadeOut.h"

#include <windows.h>
#include <psapi.h>
#include <string>
#include <algorithm>
#include <cmath>
#include <unordered_map>

#include <pdh.h>
#include <pdhmsg.h>

#pragma comment(lib, "pdh.lib")


// CPU, RAM and GPU helpers 
static float GetCPUPercentage()
{
    static ULARGE_INTEGER lastIdle{}, lastKernel{}, lastUser{};

    FILETIME idleFT, kernelFT, userFT;
    if (!GetSystemTimes(&idleFT, &kernelFT, &userFT))
        return 0.0f;

    ULARGE_INTEGER idle, kernel, user;
    idle.LowPart = idleFT.dwLowDateTime;     idle.HighPart = idleFT.dwHighDateTime;
    kernel.LowPart = kernelFT.dwLowDateTime; kernel.HighPart = kernelFT.dwHighDateTime;
    user.LowPart = userFT.dwLowDateTime;     user.HighPart = userFT.dwHighDateTime;

    ULONGLONG idleDiff = idle.QuadPart - lastIdle.QuadPart;
    ULONGLONG kernelDiff = kernel.QuadPart - lastKernel.QuadPart;
    ULONGLONG userDiff = user.QuadPart - lastUser.QuadPart;

    lastIdle = idle;
    lastKernel = kernel;
    lastUser = user;

    ULONGLONG total = kernelDiff + userDiff;
    if (total == 0) return 0.0f;

    return float((total - idleDiff) * 100.0 / total);
}

static void GetRAM_MB(size_t& usedMB, size_t& totalMB)
{
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);

    totalMB = size_t(ms.ullTotalPhys / (1024 * 1024));
    usedMB = size_t((ms.ullTotalPhys - ms.ullAvailPhys) / (1024 * 1024));
}

static std::string FormatRAM(size_t usedMB, size_t totalMB)
{
    char buf[64];
    if (totalMB >= 4096)
        sprintf_s(buf, "%.1fGB / %.1fGB", usedMB / 1024.0, totalMB / 1024.0);
    else
        sprintf_s(buf, "%zuMB / %zuMB", usedMB, totalMB);
    return buf;
}

static int GetGPUPercentage()
{
    static bool initialized = false;
    static PDH_HQUERY query = nullptr;
    static PDH_HCOUNTER counter = nullptr;

    if (!initialized)
    {
        if (PdhOpenQuery(NULL, NULL, &query) != ERROR_SUCCESS)
            return 0;

        if (PdhAddEnglishCounter(
            query,
            "\\GPU Engine(*)\\Utilization Percentage",
            NULL,
            &counter) != ERROR_SUCCESS)
            return 0;

        initialized = true;
        PdhCollectQueryData(query);
        Sleep(10);
    }

    if (PdhCollectQueryData(query) != ERROR_SUCCESS)
        return 0;

    DWORD bufferSize = 0, itemCount = 0;
    if (PdhGetFormattedCounterArray(
        counter, PDH_FMT_DOUBLE,
        &bufferSize, &itemCount, nullptr) != PDH_MORE_DATA)
        return 0;

    auto* values = (PDH_FMT_COUNTERVALUE_ITEM*)malloc(bufferSize);
    if (!values)
        return 0;

    PdhGetFormattedCounterArray(
        counter, PDH_FMT_DOUBLE,
        &bufferSize, &itemCount, values);

    double total = 0.0;
    for (DWORD i = 0; i < itemCount; i++)
        if (strstr(values[i].szName, "engtype_3D"))
            total += values[i].FmtValue.doubleValue;

    free(values);
    return (int)std::clamp(total, 0.0, 100.0);
}

namespace LavenderHook::UI::Windows {

    static LavenderHook::UI::LavenderFadeOut g_perf_overlay_fade;

    struct LineAnim {
        float alpha = 0.0f;
        float y = 10.0f;
        bool target = false;
    };

    static std::unordered_map<std::string, LineAnim> anim;

    void PerformanceOverlayWindow::Render()
    {
        // Fade window
        g_perf_overlay_fade.Tick(LavenderHook::Globals::show_performance_overlay);
        if (!g_perf_overlay_fade.ShouldRender())
            return;

        float winAlpha = g_perf_overlay_fade.Alpha();

        // Cached update every 0.5s
        static double lastUpdate = 0.0;
        static float cachedFPS = 0;
        static float cachedCPU = 0;
        static int   cachedGPU = 0;
        static size_t cachedUsed = 0, cachedTotal = 0;

        double now = ImGui::GetTime();
        if (now - lastUpdate >= 0.5)
        {
            cachedFPS = ImGui::GetIO().Framerate;
            GetRAM_MB(cachedUsed, cachedTotal);
            cachedCPU = GetCPUPercentage();
            cachedGPU = GetGPUPercentage();
            lastUpdate = now;
        }

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_AlwaysAutoResize;

        float s = LavenderHook::Globals::menu_scale;
        ImGui::SetNextWindowPos(ImVec2(10 * s, 10 * s), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6 * s, 6 * s));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, winAlpha);

        if (!ImGui::Begin("PerformanceOverlay", nullptr, flags))
        {
            ImGui::PopStyleVar(2);
            ImGui::End();
            return;
        }

        using namespace LavenderHook::UI::Lavender;

        struct Entry {
            const char* key;
            bool enabled;
            std::string text;
        };

        Entry entries[] = {
            { "fps", LavenderHook::Globals::show_perf_fps, "FPS: " + std::to_string((int)cachedFPS) },
            { "cpu", LavenderHook::Globals::show_perf_cpu, "CPU: " + std::to_string((int)cachedCPU) + "%" },
            { "gpu", LavenderHook::Globals::show_perf_gpu, "GPU: " + std::to_string((int)cachedGPU) + "%" },
            { "ram", LavenderHook::Globals::show_perf_ram, "RAM: " + FormatRAM(cachedUsed, cachedTotal) },
        };

        float dt = ImGui::GetIO().DeltaTime;
        float cursorY = ImGui::GetCursorPosY();

        for (auto& e : entries)
        {
            LineAnim& a = anim[e.key];
            a.target = e.enabled;

            a.alpha += ((a.target ? 1.0f : 0.0f) - a.alpha) * std::clamp(dt * 10.0f, 0.f, 1.f);
            a.y += ((a.target ? 0.0f : 10.0f) - a.y) * std::clamp(dt * 10.0f, 0.f, 1.f);

            if (a.alpha < 0.01f)
                continue;

            ImGui::SetCursorPosY(cursorY + a.y);
            GradientText(e.text, a.alpha * winAlpha);
            cursorY += ImGui::GetTextLineHeightWithSpacing();
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

}
