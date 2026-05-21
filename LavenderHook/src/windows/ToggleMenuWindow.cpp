#include "ToggleMenuWindow.h"
#include "../misc/Globals.h"
#include "../sound/SoundPlayer.h"
#include "../imgui/imgui.h"
#include "../ui/components/console.h"
#include "../ui/components/LavenderFadeOut.h"
#include "../ui/components/LavenderWindowHeader.h"
#include "../assets/UITextures.h"
#include "../webhook/WebhookManager.h"

#include <fstream>
#include <cstring>

// Lies of Pi
static constexpr float kPi = 3.14159265358979323846f;

// Theme Colors
extern ImVec4 MAIN_RED;
extern ImVec4 MID_RED;
extern ImVec4 DARK_RED;
extern float WINDOW_BORDER_SIZE;


// Defaults
static const ImVec4 DEF_MAIN_RED = ImVec4(0.6310878396034241f, 0.5130504965782166f, 0.7424892783164978f, 1.0f);
static const ImVec4 DEF_MID_RED = ImVec4(0.7018406391143799f, 0.544309139251709f, 0.8454935550689697f, 1.0f);
static const ImVec4 DEF_DARK_RED = ImVec4(0.7300597429275513f, 0.4847022593021393f, 0.9570815563201904f, 1.0f);

// UI
static bool  expand_performance = false;

static float s_perfAnim = 0.0f;

static bool  s_headerOpen = true;
static float s_headerAnim = 1.0f;
static float s_arrowAnim = 1.0f;

static float s_perfArrowAnim = 0.0f;

static bool  expand_windows = false;
static float s_windowsAnim = 0.0f;
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

    // animate
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
    char* app = nullptr; size_t len = 0;
    std::string dir = ".";
    if (_dupenv_s(&app, &len, "APPDATA") == 0 && app) {
        dir = app;
        free(app);
    }
    dir += "\\LavenderHook";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

static std::string GetThemePath() {
    return GetConfigDir() + "\\theme.ini";
}

static std::string GetSettingsPath() {
    return GetConfigDir() + "\\menu_settings.ini";
}

static void ApplyThemeToImGui()
{
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowBorderSize = WINDOW_BORDER_SIZE;

    style.Colors[ImGuiCol_Border] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.92f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.56f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.56f);

    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.56f);
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

    style.Colors[ImGuiCol_TableHeaderBg] = MAIN_RED;
    style.Colors[ImGuiCol_TableBorderStrong] = MAIN_RED;
    style.Colors[ImGuiCol_TableBorderLight] = MAIN_RED;

    style.Colors[ImGuiCol_TextSelectedBg] = MID_RED;
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


// Save checkbox states

static std::string BoolToStr(bool v) { return v ? "1" : "0"; }

static void SaveMenuSettings()
{
    std::ofstream f(GetSettingsPath(), std::ios::trunc);
    if (!f) return;

    f << "show_info_overlay=" << BoolToStr(LavenderHook::Globals::show_info_overlay) << "\n";
    f << "show_ping=" << BoolToStr(LavenderHook::Globals::show_ping) << "\n";
    f << "show_server=" << BoolToStr(LavenderHook::Globals::show_server) << "\n";
    f << "show_general_window=" << BoolToStr(LavenderHook::Globals::show_general_window) << "\n";
    f << "show_misc_window=" << BoolToStr(LavenderHook::Globals::show_misc_window) << "\n";
    f << "show_buffing_window=" << BoolToStr(LavenderHook::Globals::show_buffing_window) << "\n";
    f << "show_profiles_window=" << BoolToStr(LavenderHook::Globals::show_profiles_window) << "\n";
    f << "show_macro_window=" << BoolToStr(LavenderHook::Globals::show_macro_window) << "\n";
    f << "show_gamepad_window=" << BoolToStr(LavenderHook::Globals::show_gamepad_window) << "\n";
    f << "show_paragon_level_window=" << BoolToStr(LavenderHook::Globals::show_paragon_level_window) << "\n";
    f << "show_console=" << BoolToStr(LavenderHook::Globals::show_console) << "\n";
    f << "show_menu_logo=" << BoolToStr(LavenderHook::Globals::show_menu_logo) << "\n";
    f << "stop_on_fail=" << BoolToStr(LavenderHook::Globals::stop_on_fail) << "\n";
    f << "mute_buttons=" << BoolToStr(LavenderHook::Globals::mute_buttons) << "\n";
    f << "mute_fail=" << BoolToStr(LavenderHook::Globals::mute_fail) << "\n";
    f << "show_process_overlay_on_hide=" << BoolToStr(LavenderHook::Globals::show_process_overlay_on_hide) << "\n";
    f << "show_wave_window=" << BoolToStr(LavenderHook::Globals::show_wave_window) << "\n";
    f << "show_wiki_window=" << BoolToStr(LavenderHook::Globals::show_wiki_window) << "\n";
    f << "sound_volume=" << LavenderHook::Globals::sound_volume << "\n";
    f << "menu_scale=" << LavenderHook::Globals::menu_scale << "\n";

    f << "webhook_url=" << LavenderHook::Webhook::url << "\n";
    f << "webhook_user_id=" << LavenderHook::Webhook::user_id << "\n";
    f << "webhook_map_finished_enabled=" << BoolToStr(LavenderHook::Webhook::map_finished_enabled) << "\n";
    f << "webhook_core_destroyed_enabled=" << BoolToStr(LavenderHook::Webhook::core_destroyed_enabled) << "\n";
    f << "webhook_map_finished_msg=" << LavenderHook::Webhook::map_finished_msg << "\n";
    f << "webhook_core_destroyed_msg=" << LavenderHook::Webhook::core_destroyed_msg << "\n";
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

        if (line.rfind("stop_on_fail", 0) == 0)       ReadBool(line, LavenderHook::Globals::stop_on_fail);

        if (line.rfind("mute_buttons", 0) == 0)       ReadBool(line, LavenderHook::Globals::mute_buttons);
        if (line.rfind("mute_fail", 0) == 0)          ReadBool(line, LavenderHook::Globals::mute_fail);

        if (line.rfind("show_info_overlay", 0) == 0)         ReadBool(line, LavenderHook::Globals::show_info_overlay);
        if (line.rfind("show_ping", 0) == 0)                 ReadBool(line, LavenderHook::Globals::show_ping);
        if (line.rfind("show_server", 0) == 0)               ReadBool(line, LavenderHook::Globals::show_server);
        if (line.rfind("show_general_window", 0) == 0)       ReadBool(line, LavenderHook::Globals::show_general_window);
        if (line.rfind("show_misc_window", 0) == 0)          ReadBool(line, LavenderHook::Globals::show_misc_window);
        if (line.rfind("show_buffing_window", 0) == 0)       ReadBool(line, LavenderHook::Globals::show_buffing_window);
        if (line.rfind("show_profiles_window", 0) == 0)          ReadBool(line, LavenderHook::Globals::show_profiles_window);
        if (line.rfind("show_macro_window", 0) == 0)             ReadBool(line, LavenderHook::Globals::show_macro_window);
        if (line.rfind("show_gamepad_window", 0) == 0)       ReadBool(line, LavenderHook::Globals::show_gamepad_window);
        if (line.rfind("show_paragon_level_window", 0) == 0) ReadBool(line, LavenderHook::Globals::show_paragon_level_window);
        if (line.rfind("show_console", 0) == 0)              ReadBool(line, LavenderHook::Globals::show_console);
        if (line.rfind("show_menu_logo", 0) == 0)            ReadBool(line, LavenderHook::Globals::show_menu_logo);
        if (line.rfind("show_process_overlay_on_hide", 0) == 0) ReadBool(line, LavenderHook::Globals::show_process_overlay_on_hide);
        if (line.rfind("show_wave_window", 0) == 0)              ReadBool(line, LavenderHook::Globals::show_wave_window);
        if (line.rfind("show_wiki_window", 0) == 0)              ReadBool(line, LavenderHook::Globals::show_wiki_window);
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

        auto ReadStr = [&](const std::string& key, std::string& out) {
            size_t eq = line.find('=');
            if (eq == std::string::npos) return;
            if (line.size() > eq + 1)
                out = line.substr(eq + 1);
            else
                out.clear();
        };
        if (line.rfind("webhook_url", 0) == 0)               ReadStr(line, LavenderHook::Webhook::url);
        if (line.rfind("webhook_user_id", 0) == 0)           ReadStr(line, LavenderHook::Webhook::user_id);
        if (line.rfind("webhook_map_finished_enabled", 0) == 0) ReadBool(line, LavenderHook::Webhook::map_finished_enabled);
        if (line.rfind("webhook_core_destroyed_enabled", 0) == 0) ReadBool(line, LavenderHook::Webhook::core_destroyed_enabled);
        if (line.rfind("webhook_map_finished_msg", 0) == 0)  ReadStr(line, LavenderHook::Webhook::map_finished_msg);
        if (line.rfind("webhook_core_destroyed_msg", 0) == 0) ReadStr(line, LavenderHook::Webhook::core_destroyed_msg);
    }
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
        if (eq != std::string::npos)
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

// Window
namespace LavenderHook {
    namespace UI {
        namespace Windows {

            void RenderMenuSelectorWindow(bool wantVisible)
            {
                // Tick fade every frame
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
                const float rowH = ImGui::GetFrameHeightWithSpacing();

                // compute dynamic content height (collapsible sections scale with anim)
                float contentHeight = 0.0f;

                // always-visible items (15 + bottom pad)
                contentHeight += 15 * rowH;
                contentHeight += 3.5f * rowH;  // bottom padding

                // collapsible sub-items
                contentHeight += 4 * rowH * s_perfAnim;   // perf sub: FPS, RAM, CPU, GPU
                contentHeight += 11 * rowH * s_windowsAnim; // windows sub: all toggles

                // drive header animation 
                float target = s_headerOpen ? 1.0f : 0.0f;
                s_headerAnim += (target - s_headerAnim) * ImGui::GetIO().DeltaTime * 8.0f;
                s_headerAnim = Clamp01(s_headerAnim);

                // drive section animations 
                auto DriveAnim = [](float& v, bool open, float speed = 10.0f)
                    {
                        float t = open ? 1.0f : 0.0f;
                        v += (t - v) * ImGui::GetIO().DeltaTime * speed;
                        v = Clamp01(v);
                    };

                DriveAnim(s_perfAnim, expand_performance);
                DriveAnim(s_windowsAnim, expand_windows);

                // animated window height
                float targetHeight =
                    headerHeight + contentHeight * s_headerAnim;
                static float s_smoothHeight = 0.0f;
                if (s_smoothHeight == 0.0f)
                    s_smoothHeight = targetHeight;
                s_smoothHeight += (targetHeight - s_smoothHeight) * ImGui::GetIO().DeltaTime * 20.0f;
                if (fabsf(s_smoothHeight - targetHeight) < 0.5f)
                    s_smoothHeight = targetHeight;

                ImGui::SetNextWindowSize(
                    ImVec2(351.0f * s, s_smoothHeight),
                    ImGuiCond_Always
                );

                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

                ImGuiWindowFlags flags =
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoCollapse;

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

                if (s_headerAnim > 0.001f)
                {
                    float ha = alpha * s_headerAnim;

                    // Info Overlay
                    {
                        bool b = LavenderHook::Globals::show_info_overlay;
                        if (ImGui::Checkbox("Info Overlay", &b)) {
                            LavenderHook::Globals::show_info_overlay = b;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(b);
                        }
                    }

                    // Process Overlay on Hide
                    {
                        bool b = LavenderHook::Globals::show_process_overlay_on_hide;
                        if (ImGui::Checkbox("Process Overlay on Hide", &b)) {
                            LavenderHook::Globals::show_process_overlay_on_hide = b;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(b);
                        }
                    }

                    // Stop on Fail
                    {
                        if (ImGui::Checkbox("Stop on Fail", &LavenderHook::Globals::stop_on_fail)) {
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(LavenderHook::Globals::stop_on_fail);
                        }
                    }

                    // Performance Overlay
                    {
                        bool b = LavenderHook::Globals::show_performance_overlay;
                        if (ImGui::Checkbox("Performance Overlay", &b)) {
                            LavenderHook::Globals::show_performance_overlay = b;
                            SavePerfSettings();
                            LavenderHook::Audio::PlayToggleSound(b);
                        }
                        if (DropdownArrowCustom("perf", expand_performance, s_perfArrowAnim, alpha))
                            expand_performance = !expand_performance;

                        CollapsibleBegin("##perf_section", s_perfAnim, 4);
                        ImGui::Indent(18.f);

                        float pa = ItemAlpha(s_perfAnim, ha);
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, pa);
                        {
                            bool bP = LavenderHook::Globals::show_perf_fps;
                            if (ImGui::Checkbox("FPS", &bP)) {
                                LavenderHook::Globals::show_perf_fps = bP;
                                SavePerfSettings();
                                LavenderHook::Audio::PlayToggleSound(bP);
                            }
                            bP = LavenderHook::Globals::show_perf_ram;
                            if (ImGui::Checkbox("RAM Usage", &bP)) {
                                LavenderHook::Globals::show_perf_ram = bP;
                                SavePerfSettings();
                                LavenderHook::Audio::PlayToggleSound(bP);
                            }
                            bP = LavenderHook::Globals::show_perf_cpu;
                            if (ImGui::Checkbox("CPU Usage", &bP)) {
                                LavenderHook::Globals::show_perf_cpu = bP;
                                SavePerfSettings();
                                LavenderHook::Audio::PlayToggleSound(bP);
                            }
                            bP = LavenderHook::Globals::show_perf_gpu;
                            if (ImGui::Checkbox("GPU Usage", &bP)) {
                                LavenderHook::Globals::show_perf_gpu = bP;
                                SavePerfSettings();
                                LavenderHook::Audio::PlayToggleSound(bP);
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

                        bool mb = LavenderHook::Globals::mute_buttons;
                        if (ImGui::Checkbox("Mute Button Clicks", &mb)) {
                            LavenderHook::Globals::mute_buttons = mb;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(mb);
                        }

                        bool mf = LavenderHook::Globals::mute_fail;
                        if (ImGui::Checkbox("Mute Stop on Fail", &mf)) {
                            LavenderHook::Globals::mute_fail = mf;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(mf);
                        }
                    }
                    ImGui::Unindent(8.0f);

                    // Discord Webhook button
                    {
                        if (ImGui::Button("Discord Webhook", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                            ImGui::OpenPopup("Discord Webhook Settings");
                    }

                    // Discord Webhook Settings popup
                    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.65f));
                    if (ImGui::BeginPopupModal("Discord Webhook Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                    {
                        ImGui::TextUnformatted("Discord Webhook Settings");
                        ImGui::Separator();

                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted("Webhook URL:");
                        ImGui::SameLine();
                        char urlBuf[1024];
                        std::strncpy(urlBuf, LavenderHook::Webhook::url.c_str(), sizeof(urlBuf));
                        urlBuf[sizeof(urlBuf) - 1] = 0;
                        ImGui::SetNextItemWidth(400.0f);
                        if (ImGui::InputText("##webhook_url", urlBuf, sizeof(urlBuf)))
                        {
                            LavenderHook::Webhook::url = urlBuf;
                            SaveMenuSettings();
                        }

                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted("User ID:");
                        ImGui::SameLine();
                        char uidBuf[64];
                        std::strncpy(uidBuf, LavenderHook::Webhook::user_id.c_str(), sizeof(uidBuf));
                        uidBuf[sizeof(uidBuf) - 1] = 0;
                        ImGui::SetNextItemWidth(200.0f);
                        if (ImGui::InputText("##webhook_uid", uidBuf, sizeof(uidBuf)))
                        {
                            LavenderHook::Webhook::user_id = uidBuf;
                            SaveMenuSettings();
                        }

                        ImGui::Separator();
                        bool mf = LavenderHook::Webhook::map_finished_enabled;
                        if (ImGui::Checkbox("Map Finished", &mf))
                        {
                            LavenderHook::Webhook::map_finished_enabled = mf;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(mf);
                        }
                        if (LavenderHook::Webhook::map_finished_enabled)
                        {
                            char msgBuf[512];
                            std::strncpy(msgBuf, LavenderHook::Webhook::map_finished_msg.c_str(), sizeof(msgBuf));
                            msgBuf[sizeof(msgBuf) - 1] = 0;
                            ImGui::Indent(16.0f);
                            ImGui::SetNextItemWidth(400.0f);
                            if (ImGui::InputText("##map_finished_msg", msgBuf, sizeof(msgBuf)))
                            {
                                LavenderHook::Webhook::map_finished_msg = msgBuf;
                                SaveMenuSettings();
                            }
                            ImGui::Unindent(16.0f);
                        }

                        bool cd = LavenderHook::Webhook::core_destroyed_enabled;
                        if (ImGui::Checkbox("Core Destroyed", &cd))
                        {
                            LavenderHook::Webhook::core_destroyed_enabled = cd;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(cd);
                        }
                        if (LavenderHook::Webhook::core_destroyed_enabled)
                        {
                            char msgBuf[512];
                            std::strncpy(msgBuf, LavenderHook::Webhook::core_destroyed_msg.c_str(), sizeof(msgBuf));
                            msgBuf[sizeof(msgBuf) - 1] = 0;
                            ImGui::Indent(16.0f);
                            ImGui::SetNextItemWidth(400.0f);
                            if (ImGui::InputText("##core_destroyed_msg", msgBuf, sizeof(msgBuf)))
                            {
                                LavenderHook::Webhook::core_destroyed_msg = msgBuf;
                                SaveMenuSettings();
                            }
                            ImGui::Unindent(16.0f);
                        }

                        ImGui::Separator();
                        {
                            ImGui::AlignTextToFramePadding();
                            ImGui::TextUnformatted("Placeholders:");
                            ImGui::SameLine();
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.8f, 1.0f));
                            if (ImGui::SmallButton("(?)"))
                            {
                                // open on click handled by IsItemHovered tooltip
                            }
                            ImGui::PopStyleColor();
                            if (ImGui::IsItemHovered())
                            {
                                ImGui::BeginTooltip();
                                ImGui::TextUnformatted("@User       - Mentions the configured user");
                                ImGui::TextUnformatted("@Wave       - Current wave number");
                                ImGui::TextUnformatted("@Gamemode   - Current game mode");
                                ImGui::TextUnformatted("@Difficulty - Difficulty level");
                                ImGui::TextUnformatted("@Modifiers  - Active modifiers (Hardcore/Rifted)");
                                ImGui::TextUnformatted("@Bonus      - Bonus wave type");
                                ImGui::TextUnformatted("@Map        - Map name");
                                ImGui::EndTooltip();
                            }
                        }

                        ImGui::Separator();
                        if (ImGui::Button("Close", ImVec2(120, 0)))
                            ImGui::CloseCurrentPopup();

                        ImGui::EndPopup();
                    }
                    ImGui::PopStyleColor();

                    // Windows section
                    ImGui::Separator();
                    ImGui::TextDisabled("Windows:");
                    if (DropdownArrowCustom("windows", expand_windows, s_windowsArrowAnim, alpha))
                        expand_windows = !expand_windows;

                    CollapsibleBegin("##windows_section", s_windowsAnim, 11);
                    ImGui::Indent(18.f);

                    {
                        float wa = ItemAlpha(s_windowsAnim, ha);
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, wa);

                        auto WinCb = [&](const char* label, bool& var, const char* saveAfter = nullptr) {
                            bool b = var;
                            if (ImGui::Checkbox(label, &b)) {
                                var = b;
                                SaveMenuSettings();
                                LavenderHook::Audio::PlayToggleSound(b);
                            }
                        };

                        WinCb("General Window", LavenderHook::Globals::show_general_window);
                        WinCb("Misc Window", LavenderHook::Globals::show_misc_window);
                        WinCb("Buffing Window", LavenderHook::Globals::show_buffing_window);
                        WinCb("Virtual Controller", LavenderHook::Globals::show_gamepad_window);
                        WinCb("Profiles Window", LavenderHook::Globals::show_profiles_window);
                        WinCb("Macro Manager", LavenderHook::Globals::show_macro_window);
                        WinCb("Mastery Level", LavenderHook::Globals::show_paragon_level_window);
                        WinCb("Wave Overlay", LavenderHook::Globals::show_wave_window);
                        WinCb("Lavender Wiki", LavenderHook::Globals::show_wiki_window);
                        WinCb("Console", LavenderHook::Globals::show_console);
                        WinCb("Menu Logo", LavenderHook::Globals::show_menu_logo);

                        ImGui::PopStyleVar();
                    }
                    ImGui::Unindent(18.f);
                    CollapsibleEnd();

                    // Theme Colors
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
                            MAIN_RED = DEF_MAIN_RED;
                            MID_RED = DEF_MID_RED;
                            DARK_RED = DEF_DARK_RED;
                            SaveTheme();
                            ApplyThemeToImGui();
                        }
                    }
                }
                ImGui::End();
                ImGui::PopStyleVar();
            }
        }
    }
}
