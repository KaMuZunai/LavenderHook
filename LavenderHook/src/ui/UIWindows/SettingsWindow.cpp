#include "SettingsWindow.h"
#include "../../misc/Globals.h"
#include "../../misc/FileLog.h"
#include "../../config/ConfigManager.h"
#include "../../sound/SoundPlayer.h"
#include "../../imgui/imgui.h"
#include "console.h"
#include "../components/LavenderFadeOut.h"
#include "../components/LavenderUI.h"
#include "../components/LavenderWindowHeader.h"
#include "../../assets/UITextures.h"

#include <fstream>
#include <cstring>
#include <vector>

static constexpr float kPi = 3.14159265358979323846f;

extern ImVec4 MAIN_RED;
extern ImVec4 MID_RED;
extern ImVec4 DARK_RED;
extern float WINDOW_BORDER_SIZE;

// Old theme (previously "Default")
static const ImVec4 OLD_MAIN_RED = ImVec4(0.6310878396034241f, 0.5130504965782166f, 0.7424892783164978f, 1.0f);
static const ImVec4 OLD_MID_RED = ImVec4(0.7018406391143799f, 0.544309139251709f, 0.8454935550689697f, 1.0f);
static const ImVec4 OLD_DARK_RED = ImVec4(0.7300597429275513f, 0.4847022593021393f, 0.9570815563201904f, 1.0f);

// Polished theme (now default)
static const ImVec4 POLISHED_MAIN_RED = ImVec4(0.35f, 0.50f, 0.55f, 1.0f);
static const ImVec4 POLISHED_MID_RED = ImVec4(0.40f, 0.60f, 0.70f, 1.0f);
static const ImVec4 POLISHED_DARK_RED = ImVec4(0.50f, 0.75f, 0.85f, 1.0f);

struct SettingsCheckbox {
    const char* label;
    bool* value;
};

static bool expand_performance = false;
static bool expand_info_overlay = false;
static bool expand_windows = false;

static float s_perfAnim = 0.0f;
static float s_infoOverlayAnim = 0.0f;
static float s_windowsAnim = 0.0f;

static bool s_headerOpen = true;
static float s_headerAnim = 1.0f;
static float s_arrowAnim = 1.0f;

static float s_perfArrowAnim = 0.0f;
static float s_infoArrowAnim = 0.0f;
static float s_windowsArrowAnim = 0.0f;

static float Clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static float RowH() {
    return ImGui::GetFrameHeightWithSpacing();
}

static void CollapsibleBegin(const char* id, float anim, float rowCount)
{
    float rowH = ImGui::GetFrameHeightWithSpacing();
    float fullHeight = rowH * rowCount;
    constexpr float closedBuf = 4.0f;
    float height = closedBuf + (fullHeight - closedBuf) * anim;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild(
        ImGui::GetID(id),
        ImVec2(0.0f, height),
        false,
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground
    );
}

static void CollapsibleEnd()
{
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static float ItemAlpha(float anim, float parentAlpha)
{
    return parentAlpha * Clamp01(anim * anim * (3.0f - 2.0f * anim));
}

static bool DropdownArrowCustom(
    const char* id,
    bool expanded,
    float& anim,
    float alpha)
{
    const float size = ImGui::GetFrameHeight();
    const float arrowArea = size;

    ImGui::SameLine();
    ImGui::SetCursorPosX(
        ImGui::GetCursorPosX() +
        ImGui::GetContentRegionAvail().x -
        arrowArea
    );

    ImGui::PushID(id);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##arrow", ImVec2(arrowArea, size));

    bool clicked = ImGui::IsItemClicked();

    float target = expanded ? 1.0f : 0.0f;
    anim += (target - anim) * ImGui::GetIO().DeltaTime * 10.0f;
    anim = Clamp01(anim);

    if (g_dropLeftTex)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImVec2 center(
            pos.x + arrowArea * 0.5f,
            pos.y + size * 0.5f
        );

        float iconSize = size * 0.6f;
        float angle = -anim * kPi * 0.5f;

        float s = sinf(angle);
        float c = cosf(angle);
        ImVec2 h(iconSize * 0.5f, iconSize * 0.5f);

        ImVec2 v[4] = {
            {-h.x,-h.y},{h.x,-h.y},{h.x,h.y},{-h.x,h.y}
        };

        for (auto& p : v)
            p = ImVec2(
                center.x + p.x * c - p.y * s,
                center.y + p.x * s + p.y * c
            );

        dl->AddImageQuad(
            g_dropLeftTex,
            v[0], v[1], v[2], v[3],
            ImVec2(0, 0), ImVec2(1, 0),
            ImVec2(1, 1), ImVec2(0, 1),
            ImGui::GetColorU32(ImVec4(1, 1, 1, alpha))
        );
    }

    ImGui::PopID();
    return clicked;
}


static std::string GetConfigDir() {
    std::string dir = LavenderHook::Config::GetBaseDir();
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

static std::string GetThemePath() {
    return GetConfigDir() + "\\theme.ini";
}

static std::string GetSettingsPath() {
    return GetConfigDir() + "\\menu_settings.ini";
}

void ApplyThemeToImGui()
{
    ImGuiStyle& style = ImGui::GetStyle();

    bool polished = LavenderHook::Globals::use_polished_overlay;

    // Style properties
    if (polished) {
        style.Alpha = 1.0f;
        style.DisabledAlpha = 0.6f;
        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.WindowRounding = 10.0f;
        style.WindowBorderSize = 1.0f;
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.ChildRounding = 8.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupRounding = 8.0f;
        style.PopupBorderSize = 1.0f;
        style.FramePadding = ImVec2(6.0f, 4.0f);
        style.FrameRounding = 8.0f;
        style.FrameBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.IndentSpacing = 21.0f;
        style.ScrollbarSize = 12.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabMinSize = 10.0f;
        style.GrabRounding = 8.0f;
        style.TabRounding = 8.0f;
        style.TabBorderSize = 0.0f;
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

        // Dark frosted backgrounds matching the polished overlay
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.086f, 0.078f, 0.110f, 0.90f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.086f, 0.078f, 0.110f, 0.70f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.086f, 0.078f, 0.110f, 0.95f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.050f, 0.045f, 0.065f, 0.85f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.75f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.85f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.60f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.065f, 0.060f, 0.085f, 0.80f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.050f, 0.045f, 0.065f, 0.60f);
        style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.086f, 0.078f, 0.110f, 0.92f);
        style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.050f, 0.045f, 0.065f, 0.50f);
        style.Colors[ImGuiCol_Separator] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.40f);
        style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.60f);
        style.Colors[ImGuiCol_SeparatorActive] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.80f);
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.40f);
    } else {
        // Default theme — match GUI.cpp constructor defaults
        style.Alpha = 1.0f;
        style.DisabledAlpha = 0.6000000238418579f;
        style.WindowPadding = ImVec2(8.0f, 8.0f);
        style.WindowRounding = 10.0f;
        style.WindowBorderSize = WINDOW_BORDER_SIZE;
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.ChildRounding = 5.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupRounding = 5.0f;
        style.PopupBorderSize = 1.0f;
        style.FramePadding = ImVec2(4.0f, 3.0f);
        style.FrameRounding = 5.0f;
        style.FrameBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(8.0f, 4.0f);
        style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
        style.IndentSpacing = 21.0f;
        style.ScrollbarSize = 14.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabMinSize = 10.0f;
        style.GrabRounding = 5.0f;
        style.TabRounding = 5.0f;
        style.TabBorderSize = 0.0f;
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

        // Default backgrounds
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.100f, 0.100f, 0.100f, 0.75f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.054902f, 0.054902f, 0.054902f, 0.478431f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.478431f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0392157f, 0.0392157f, 0.0392157f, 0.75f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.75);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 85);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 85);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.0784314f, 0.0784314f, 0.0784314f, 0.784314f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.564706f);
        style.Colors[ImGuiCol_TableHeaderBg] = MAIN_RED;
        style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.921569f);
        style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.0980392f, 0.0980392f, 0.0980392f, 0.921569f);
        style.Colors[ImGuiCol_Separator] = ImVec4(0.0784314f, 0.0784314f, 0.0784314f, 0.501961f);
        style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.0784314f, 0.0784314f, 0.0784314f, 0.669528f);
        style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.0784314f, 0.0784314f, 0.0784314f, 0.957082f);
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.101961f, 0.113725f, 0.129412f, 0.2f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.203922f, 0.207843f, 0.215686f, 0.2f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.301961f, 0.301961f, 0.301961f, 0.2f);
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.705882f);
    }

    // Theme accent colors
    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 0.9215686f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.498039f, 0.498039f, 0.498039f, 0.9215686f);
    style.Colors[ImGuiCol_Border] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.92f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.56f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.56f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.78f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.78f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.78f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.78f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.81f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.81f);
    style.Colors[ImGuiCol_Button] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.50f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.92f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.75f);
    style.Colors[ImGuiCol_Header] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.65f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.80f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.92f);
    style.Colors[ImGuiCol_Tab] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.44f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.92f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.88f);
    style.Colors[ImGuiCol_PlotHistogram] = DARK_RED;
    style.Colors[ImGuiCol_PlotHistogramHovered] = DARK_RED;
    style.Colors[ImGuiCol_TableBorderStrong] = MAIN_RED;
    style.Colors[ImGuiCol_TableBorderLight] = MAIN_RED;
    style.Colors[ImGuiCol_TextSelectedBg] = MID_RED;

    // Switch font: Fonts[0] = regular Segoe UI, Fonts[1] = Semibold (if loaded)
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->Fonts.Size >= 2) {
        io.FontDefault = polished ? io.Fonts->Fonts[1] : io.Fonts->Fonts[0];
    }
}

void SaveTheme()
{
    std::ofstream f(GetThemePath(), std::ios::trunc);
    if (!f) return;

    auto W = [&](const char* k, const ImVec4& c) {
        f << k << "=" << c.x << "," << c.y << "," << c.z << "," << c.w << "\n";
        };
    W("MAIN_RED", MAIN_RED);
    W("MID_RED", MID_RED);
    W("DARK_RED", DARK_RED);
}

bool LoadTheme()
{
    std::ifstream f(GetThemePath());
    if (!f) return false;

    auto R = [&](const std::string& s, ImVec4& out) {
        size_t eq = s.find('=');
        if (eq == std::string::npos) return;
        float r, g, b, a;
        std::string v = s.substr(eq + 1);
        if (sscanf_s(v.c_str(), "%f,%f,%f,%f", &r, &g, &b, &a) == 4)
            out = ImVec4(r, g, b, a);
        };

    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("MAIN_RED", 0) == 0)   R(line, MAIN_RED);
        if (line.rfind("MID_RED", 0) == 0)    R(line, MID_RED);
        if (line.rfind("DARK_RED", 0) == 0)   R(line, DARK_RED);
    }

    return true;
}


static std::string BoolToStr(bool v) { return v ? "1" : "0"; }

static void SaveMenuSettings()
{
    std::ofstream f(GetSettingsPath(), std::ios::trunc);
    if (!f) return;

    f << "show_info_overlay=" << BoolToStr(LavenderHook::Globals::show_info_overlay) << "\n";
    f << "show_ping=" << BoolToStr(LavenderHook::Globals::show_ping) << "\n";
    f << "show_server=" << BoolToStr(LavenderHook::Globals::show_server) << "\n";
    f << "show_debug_window=" << BoolToStr(LavenderHook::Globals::show_debug_window) << "\n";
    f << "show_console=" << BoolToStr(LavenderHook::Globals::show_console) << "\n";
    f << "show_menu_logo=" << BoolToStr(LavenderHook::Globals::show_menu_logo) << "\n";
    f << "show_profiles_window=" << BoolToStr(LavenderHook::Globals::show_profiles_window) << "\n";
    f << "mute_buttons=" << BoolToStr(LavenderHook::Globals::mute_buttons) << "\n";
    f << "sound_volume=" << LavenderHook::Globals::sound_volume << "\n";
    f << "menu_scale=" << LavenderHook::Globals::menu_scale << "\n";
    f << "use_polished_overlay=" << BoolToStr(LavenderHook::Globals::use_polished_overlay) << "\n";
}

void LoadMenuSettings()
{
    std::ifstream f(GetSettingsPath());
    if (!f) return;

    auto ReadBool = [&](const std::string& s, bool& out) {
        size_t eq = s.find('=');
        if (eq == std::string::npos) return;
        out = (std::stoi(s.substr(eq + 1)) != 0);
        };

    std::string line;
    while (std::getline(f, line)) {

        if (line.rfind("window_border_size", 0) == 0) {
            size_t eq = line.find('=');
            if (eq != std::string::npos)
                WINDOW_BORDER_SIZE = std::stof(line.substr(eq + 1));
        }

        if (line.rfind("mute_buttons", 0) == 0)       ReadBool(line, LavenderHook::Globals::mute_buttons);

        if (line.rfind("show_info_overlay", 0) == 0)         ReadBool(line, LavenderHook::Globals::show_info_overlay);
        if (line.rfind("show_ping", 0) == 0)                 ReadBool(line, LavenderHook::Globals::show_ping);
        if (line.rfind("show_server", 0) == 0)               ReadBool(line, LavenderHook::Globals::show_server);
        if (line.rfind("show_debug_window", 0) == 0)         ReadBool(line, LavenderHook::Globals::show_debug_window);
        if (line.rfind("show_console", 0) == 0)              ReadBool(line, LavenderHook::Globals::show_console);
        if (line.rfind("show_menu_logo", 0) == 0)            ReadBool(line, LavenderHook::Globals::show_menu_logo);
        if (line.rfind("show_profiles_window", 0) == 0)      ReadBool(line, LavenderHook::Globals::show_profiles_window);
        if (line.rfind("sound_volume", 0) == 0) {
            size_t eq = line.find('=');
            if (eq != std::string::npos)
                LavenderHook::Globals::sound_volume = std::stoi(line.substr(eq + 1));
        }
        if (line.rfind("menu_scale", 0) == 0) {
            size_t eq = line.find('=');
            if (eq != std::string::npos)
                LavenderHook::Globals::menu_scale = std::stof(line.substr(eq + 1));
        }
        if (line.rfind("use_polished_overlay", 0) == 0)  ReadBool(line, LavenderHook::Globals::use_polished_overlay);
    }

    if (LavenderHook::Globals::use_polished_overlay) {
        MAIN_RED = POLISHED_MAIN_RED;
        MID_RED = POLISHED_MID_RED;
        DARK_RED = POLISHED_DARK_RED;
    }
    ApplyThemeToImGui();
}

static std::string GetPerfSettingsPath() {
    return GetConfigDir() + "\\performance_settings.ini";
}

static void SavePerfSettings()
{
    std::ofstream f(GetPerfSettingsPath(), std::ios::trunc);
    if (!f) return;

    f << "show_performance_overlay=" << BoolToStr(LavenderHook::Globals::show_performance_overlay) << "\n";
    f << "show_perf_fps=" << BoolToStr(LavenderHook::Globals::show_perf_fps) << "\n";
    f << "show_perf_ram=" << BoolToStr(LavenderHook::Globals::show_perf_ram) << "\n";
    f << "show_perf_cpu=" << BoolToStr(LavenderHook::Globals::show_perf_cpu) << "\n";
    f << "show_perf_gpu=" << BoolToStr(LavenderHook::Globals::show_perf_gpu) << "\n";
}

void LoadPerfSettings()
{
    std::ifstream f(GetPerfSettingsPath());
    if (!f) return;

    auto ReadBool = [&](const std::string& s, bool& out) {
        size_t eq = s.find('=');
        if (eq == std::string::npos) return;
        out = (std::stoi(s.substr(eq + 1)) != 0);
        };

    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("show_performance_overlay", 0) == 0) ReadBool(line, LavenderHook::Globals::show_performance_overlay);
        if (line.rfind("show_perf_fps", 0) == 0)            ReadBool(line, LavenderHook::Globals::show_perf_fps);
        if (line.rfind("show_perf_ram", 0) == 0)            ReadBool(line, LavenderHook::Globals::show_perf_ram);
        if (line.rfind("show_perf_cpu", 0) == 0)            ReadBool(line, LavenderHook::Globals::show_perf_cpu);
        if (line.rfind("show_perf_gpu", 0) == 0)            ReadBool(line, LavenderHook::Globals::show_perf_gpu);
    }
}

static bool initialized_once = false;
static bool s_styleCaptured = false;
static ImGuiStyle s_baseStyle;
static float s_prevScale = 1.0f;
static float s_targetScale = 1.0f;
static LavenderHook::UI::LavenderFadeOut g_menu_selector_fade;

void InitMenuScale()
{
    s_baseStyle = ImGui::GetStyle();
    s_styleCaptured = true;
    s_targetScale = LavenderHook::Globals::menu_scale;
    s_prevScale = s_targetScale;
    ImGui::GetStyle().ScaleAllSizes(s_targetScale);
    ImGui::GetIO().FontGlobalScale = s_targetScale;
    ApplyThemeToImGui();
}

static void UpdateScaleAnimation()
{
    if (!s_styleCaptured)
        return;
    float& current = LavenderHook::Globals::menu_scale;
    if (fabsf(current - s_targetScale) > 0.0005f)
    {
        current += (s_targetScale - current) * ImGui::GetIO().DeltaTime * 10.0f;
        if (fabsf(current - s_targetScale) < 0.0005f)
            current = s_targetScale;
        if (fabsf(current - s_prevScale) > 0.001f)
        {
            s_prevScale = current;
            ImGui::GetStyle() = s_baseStyle;
            ImGui::GetStyle().ScaleAllSizes(current);
            ImGui::GetIO().FontGlobalScale = current;
            ApplyThemeToImGui();
        }
    }
}

// Checkbox data for collapsible sections (add items here to auto-extend)
static const SettingsCheckbox kPerfCheckboxes[] = {
    {"FPS", &LavenderHook::Globals::show_perf_fps},
    {"RAM Usage", &LavenderHook::Globals::show_perf_ram},
    {"CPU Usage", &LavenderHook::Globals::show_perf_cpu},
    {"GPU Usage", &LavenderHook::Globals::show_perf_gpu},
};
static const int kPerfCount = sizeof(kPerfCheckboxes) / sizeof(kPerfCheckboxes[0]);

static const SettingsCheckbox kInfoCheckboxes[] = {
    {"Server", &LavenderHook::Globals::show_server},
    {"Ping", &LavenderHook::Globals::show_ping},
};
static const int kInfoCount = sizeof(kInfoCheckboxes) / sizeof(kInfoCheckboxes[0]);

static const SettingsCheckbox kWindowCheckboxes[] = {
    {"Debug Window", &LavenderHook::Globals::show_debug_window},
    {"Console", &LavenderHook::Globals::show_console},
    {"Menu Logo", &LavenderHook::Globals::show_menu_logo},
    {"Profiles", &LavenderHook::Globals::show_profiles_window},
};
static const int kWindowCount = sizeof(kWindowCheckboxes) / sizeof(kWindowCheckboxes[0]);

namespace LavenderHook {
    namespace UI {
        namespace Windows {

            void RenderSettingsWindow(bool wantVisible)
            {
                g_menu_selector_fade.Tick(wantVisible);

                if (!g_menu_selector_fade.ShouldRender())
                    return;

                UpdateScaleAnimation();

                if (!initialized_once) {
                    initialized_once = true;
                    LoadTheme();
                    LoadMenuSettings();
                    LoadPerfSettings();
                    LavenderHook::Audio::SetVolumePercent(LavenderHook::Globals::sound_volume);
                    if (!s_styleCaptured)
                        InitMenuScale();
                }


                float alpha = g_menu_selector_fade.Alpha();
                float s = LavenderHook::Globals::menu_scale;

                const float headerHeight = (32.0f + 4.0f) * s;

                float target = s_headerOpen ? 1.0f : 0.0f;
                s_headerAnim += (target - s_headerAnim) * ImGui::GetIO().DeltaTime * 8.0f;
                s_headerAnim = Clamp01(s_headerAnim);

                auto DriveAnim = [](float& v, bool open, float speed = 10.0f)
                    {
                        float t = open ? 1.0f : 0.0f;
                        v += (t - v) * ImGui::GetIO().DeltaTime * speed;
                        v = Clamp01(v);
                    };

                DriveAnim(s_perfAnim, expand_performance);
                DriveAnim(s_infoOverlayAnim, expand_info_overlay);
                DriveAnim(s_windowsAnim, expand_windows);

                ImGui::SetNextWindowSize(
                    ImVec2(351.0f * s, headerHeight + 400.0f),
                    ImGuiCond_FirstUseEver
                );

                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

                ImGuiWindowFlags flags =
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize;

                if (!ImGui::Begin("##Settings", nullptr, flags))
                {
                    ImGui::End();
                    ImGui::PopStyleVar();
                    return;
                }

                LavenderHook::UI::Lavender::RenderWindowHeader(
                    "Settings",
                    g_wrenchIcoTex,
                    g_dropLeftTex,
                    ImGui::GetWindowWidth(),
                    alpha,
                    s_headerOpen,
                    s_headerAnim,
                    s_arrowAnim
                );

                // Record start of content area (right after header)
                float contentStartY = ImGui::GetCursorPosY();

                if (s_headerAnim > 0.001f)
                {
                    float ha = alpha * s_headerAnim;

                    // Info Overlay — expandable with Server/Ping sub-options
                    {
                        bool b = LavenderHook::Globals::show_info_overlay;
                        if (ImGui::Checkbox("Info Overlay", &b)) {
                            LavenderHook::Globals::show_info_overlay = b;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(b);
                        }
                        if (DropdownArrowCustom("info", expand_info_overlay, s_infoArrowAnim, alpha))
                            expand_info_overlay = !expand_info_overlay;

                        CollapsibleBegin("##info_section", s_infoOverlayAnim, kInfoCount);
                        ImGui::Indent(18.f);

                        float ia = ItemAlpha(s_infoOverlayAnim, ha);
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ia);
                        for (const auto& item : kInfoCheckboxes)
                        {
                            bool val = *item.value;
                            if (ImGui::Checkbox(item.label, &val)) {
                                *item.value = val;
                                SaveMenuSettings();
                                LavenderHook::Audio::PlayToggleSound(val);
                            }
                        }
                        ImGui::PopStyleVar();
                        ImGui::Unindent(18.f);
                        CollapsibleEnd();
                    }

                    // Performance Overlay — expandable with sub-options
                    {
                        bool b = LavenderHook::Globals::show_performance_overlay;
                        if (ImGui::Checkbox("Performance Overlay", &b)) {
                            LavenderHook::Globals::show_performance_overlay = b;
                            SavePerfSettings();
                            LavenderHook::Audio::PlayToggleSound(b);
                        }
                        if (DropdownArrowCustom("perf", expand_performance, s_perfArrowAnim, alpha))
                            expand_performance = !expand_performance;

                        CollapsibleBegin("##perf_section", s_perfAnim, kPerfCount);
                        ImGui::Indent(18.f);

                        float pa = ItemAlpha(s_perfAnim, ha);
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, pa);
                        for (const auto& item : kPerfCheckboxes)
                        {
                            bool val = *item.value;
                            if (ImGui::Checkbox(item.label, &val)) {
                                *item.value = val;
                                SavePerfSettings();
                                LavenderHook::Audio::PlayToggleSound(val);
                            }
                        }
                        ImGui::PopStyleVar();
                        ImGui::Unindent(18.f);
                        CollapsibleEnd();
                    }

                    // Menu Scale
                    ImGui::Separator();
                    ImGui::TextDisabled("Menu Size:");
                    ImGui::Indent(8.0f);
                    ImGui::Spacing();
                    {
                        float cur = LavenderHook::Globals::menu_scale;
                        float scaleWid = ImGui::GetContentRegionAvail().x * 0.9f;
                        ImGui::SetNextItemWidth(scaleWid);
                        float displayPct = cur * 100.0f;
                        if (ImGui::SliderFloat("##menu_scale", &displayPct, 50.0f, 200.0f, "%.0f%%")) {
                            s_targetScale = displayPct / 100.0f;
                            float temp = LavenderHook::Globals::menu_scale;
                            LavenderHook::Globals::menu_scale = s_targetScale;
                            SaveMenuSettings();
                            LavenderHook::Globals::menu_scale = temp;
                        }
                        {
                            float wheel = ImGui::GetIO().MouseWheel;
                            if (wheel != 0.0f && ImGui::IsItemHovered()) {
                                displayPct += wheel * 1.5f;
                                if (displayPct < 50.0f) displayPct = 50.0f;
                                if (displayPct > 200.0f) displayPct = 200.0f;
                                s_targetScale = displayPct / 100.0f;
                                float temp = LavenderHook::Globals::menu_scale;
                                LavenderHook::Globals::menu_scale = s_targetScale;
                                SaveMenuSettings();
                                LavenderHook::Globals::menu_scale = temp;
                            }
                        }
                    }
                    ImGui::Unindent(8.0f);

                    // Audio section
                    ImGui::Separator();
                    ImGui::TextDisabled("Audio:");
                    ImGui::Indent(8.0f);
                    ImGui::Spacing();
                    {
                        int vol = LavenderHook::Globals::sound_volume;
                        float wid = ImGui::GetContentRegionAvail().x * 0.9f;
                        ImGui::SetNextItemWidth(wid);
                        if (ImGui::SliderInt("##sound_volume", &vol, 0, 100, "%d%%")) {
                            LavenderHook::Globals::sound_volume = vol;
                            SaveMenuSettings();
                            LavenderHook::Audio::SetVolumePercent(vol);
                        }
                        {
                            float wheel = ImGui::GetIO().MouseWheel;
                            if (wheel != 0.0f && ImGui::IsItemHovered()) {
                                vol += (int)(wheel > 0 ? 1 : -1);
                                if (vol < 0) vol = 0;
                                if (vol > 100) vol = 100;
                                LavenderHook::Globals::sound_volume = vol;
                                SaveMenuSettings();
                                LavenderHook::Audio::SetVolumePercent(vol);
                            }
                        }

                        bool mb = LavenderHook::Globals::mute_buttons;
                        if (ImGui::Checkbox("Mute Button Clicks", &mb)) {
                            LavenderHook::Globals::mute_buttons = mb;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(mb);
                        }
                    }
                    ImGui::Unindent(8.0f);

                    // Windows section
                    ImGui::Separator();
                    ImGui::TextDisabled("Windows:");
                    if (DropdownArrowCustom("windows", expand_windows, s_windowsArrowAnim, alpha))
                        expand_windows = !expand_windows;

                    CollapsibleBegin("##windows_section", s_windowsAnim, kWindowCount);
                    ImGui::Indent(18.f);

                    {
                        float wa = ItemAlpha(s_windowsAnim, ha);
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, wa);
                        for (const auto& item : kWindowCheckboxes)
                        {
                            bool val = *item.value;
                            if (ImGui::Checkbox(item.label, &val)) {
                                *item.value = val;
                                SaveMenuSettings();
                                LavenderHook::Audio::PlayToggleSound(val);
                            }
                        }
                        ImGui::PopStyleVar();
                    }
                    ImGui::Unindent(18.f);
                    CollapsibleEnd();

                    // Theme
                    ImGui::Separator();
                    ImGui::TextDisabled("Theme:");

                    {
                        const char* themes[] = { "Old", "Polished" };
                        int current = LavenderHook::Globals::use_polished_overlay ? 1 : 0;
                        if (ImGui::Combo("##theme_sel", &current, themes, 2))
                        {
                            LavenderHook::Globals::use_polished_overlay = (current == 1);
                            if (current == 1) {
                                MAIN_RED = POLISHED_MAIN_RED;
                                MID_RED = POLISHED_MID_RED;
                                DARK_RED = POLISHED_DARK_RED;
                            } else {
                                MAIN_RED = OLD_MAIN_RED;
                                MID_RED = OLD_MID_RED;
                                DARK_RED = OLD_DARK_RED;
                            }
                            SaveMenuSettings();
                            ApplyThemeToImGui();
                        }
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("Theme Colors:");

                    {
                        bool changed = false;
                        if (ImGui::ColorEdit3("Main", (float*)&MAIN_RED)) changed = true;
                        if (ImGui::ColorEdit3("Alt", (float*)&MID_RED))  changed = true;
                        if (ImGui::ColorEdit3("Bright", (float*)&DARK_RED)) changed = true;

                        if (changed) {
                            SaveTheme();
                            ApplyThemeToImGui();
                        }

                        if (ImGui::Button("Reset to Default"))
                        {
                            MAIN_RED = POLISHED_MAIN_RED;
                            MID_RED = POLISHED_MID_RED;
                            DARK_RED = POLISHED_DARK_RED;
                            LavenderHook::Globals::use_polished_overlay = true;
                            SaveMenuSettings();
                            SaveTheme();
                            ApplyThemeToImGui();
                        }
                    }
                }

                // Measure actual rendered content height, scaled by header anim so the window shrinks smoothly
                float contentEndY = ImGui::GetCursorPosY();
                float actualContent = (contentEndY - contentStartY) * s_headerAnim;

                static float s_animatedContent = 0.0f;
                if (s_animatedContent == 0.0f)
                    s_animatedContent = actualContent;
                s_animatedContent += (actualContent - s_animatedContent) * ImGui::GetIO().DeltaTime * 20.0f;
                if (fabsf(s_animatedContent - actualContent) < 0.5f)
                    s_animatedContent = actualContent;

                float bottomPad = ImGui::GetStyle().WindowPadding.y + 4.0f;
                ImGui::SetWindowSize(ImVec2(351.0f * s, headerHeight + s_animatedContent + bottomPad));
                ImVec2 sbPos = ImGui::GetWindowPos();
                ImVec2 sbSize = ImGui::GetWindowSize();
                ImGui::End();
                if (LavenderHook::Globals::use_polished_overlay)
                    LavenderHook::UI::Lavender::DrawWindowShadow(sbPos, sbSize, alpha);
                ImGui::PopStyleVar();
            }
        }
    }
}
