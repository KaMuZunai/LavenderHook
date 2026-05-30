#include "console.h"
#include "../components/LavenderFadeOut.h"
#include "../components/LavenderUI.h"
#include "../../misc/Globals.h"
#include "../../imgui/imgui_internal.h"

static LavenderHook::UI::LavenderFadeOut g_console_fade;

static bool s_autoScroll = true;
static bool s_newContent = false;

static int ConsoleInputCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && s_newContent && s_autoScroll)
        data->CursorPos = data->BufTextLen;
    return 0;
}

void LavenderConsole::Render(bool wantVisible)
{
    // Tick fade every frame
    g_console_fade.Tick(wantVisible);

    if (!g_console_fade.ShouldRender())
        return;

    float alpha = g_console_fade.Alpha();
    bool polished = LavenderHook::Globals::use_polished_overlay;

    float s = LavenderHook::Globals::menu_scale;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

    ImGui::SetNextWindowSize(ImVec2(500 * s, 300 * s), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;
    if (polished)
        flags |= ImGuiWindowFlags_NoTitleBar;

    ImGui::Begin("Lavender Console", nullptr, flags);

    if (polished) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wPos = ImGui::GetWindowPos();
        float w = ImGui::GetWindowWidth();
        float headerH = 30.0f * s;

        float rr = ImGui::GetStyle().WindowRounding;
        dl->AddRectFilled(wPos, ImVec2(wPos.x + w, wPos.y + headerH),
            IM_COL32(22, 20, 28, (int)(235.f * alpha)), rr, ImDrawFlags_RoundCornersTop);
        dl->AddRectFilled(wPos, ImVec2(wPos.x + w, wPos.y + headerH * 0.50f),
            IM_COL32(255, 255, 255, (int)(9.f * alpha)),
            rr, ImDrawFlags_RoundCornersTop);

        dl->AddLine(
            ImVec2(wPos.x + 1.0f * s, wPos.y + headerH),
            ImVec2(wPos.x + w - 1.0f * s, wPos.y + headerH),
            LavenderHook::UI::Lavender::PolishedAccent(0.55f * alpha), 1.0f * s);

        const char* title = "Console";
        ImVec2 ts = ImGui::CalcTextSize(title);
        dl->AddText(ImVec2(wPos.x + (w - ts.x) * 0.5f,
            wPos.y + (headerH - ts.y) * 0.5f),
            ImGui::GetColorU32(ImVec4(1, 1, 1, alpha)), title);

        ImGui::SetCursorPosY(headerH + 4.0f * s);
    }

    static const size_t kConsoleBufSize = 65536;
    static char textBuffer[kConsoleBufSize] = {};
    static size_t lastCount = 0;

    if (this->buffer.size() != lastCount)
    {
        size_t pos = 0;
        for (size_t i = 0; i < this->buffer.size() && pos < kConsoleBufSize - 1; ++i)
        {
            if (i && pos < kConsoleBufSize - 1)
                textBuffer[pos++] = '\n';
            for (char c : this->buffer[i])
            {
                if (pos >= kConsoleBufSize - 1) break;
                textBuffer[pos++] = c;
            }
        }
        textBuffer[pos] = '\0';
        lastCount = this->buffer.size();
        s_autoScroll = true;
        s_newContent = true;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && io.MouseWheel != 0.0f)
        s_autoScroll = (io.MouseWheel < 0.0f);

    ImGui::InputTextMultiline("##log", textBuffer, kConsoleBufSize,
        ImVec2(-FLT_MIN, -FLT_MIN),
        ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_CallbackAlways,
        ConsoleInputCallback, nullptr);

    s_newContent = false;

    ImVec2 sbPos = ImGui::GetWindowPos();
    ImVec2 sbSize = ImGui::GetWindowSize();
    ImGui::End();
    if (polished)
        LavenderHook::UI::Lavender::DrawWindowShadow(sbPos, sbSize, alpha);

    ImGui::PopStyleVar();
}

void LavenderConsole::Log(std::string line)
{
    std::cout << line << std::endl;
    this->buffer.push_back(std::move(line));
}
