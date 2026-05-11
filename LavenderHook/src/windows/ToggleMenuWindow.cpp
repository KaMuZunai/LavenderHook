#include "ToggleMenuWindow.h"
#include "../misc/Globals.h"
#include "../sound/SoundPlayer.h"
#include "../imgui/imgui.h"
#include "../ui/components/console.h"
#include "../ui/components/LavenderFadeOut.h"
#include "../ui/components/LavenderWindowHeader.h"
#include "../assets/UITextures.h"

#include <fstream>

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

constexpr float kHideThreshold = 0.06f;

static float Clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static float RowH() {
    return ImGui::GetFrameHeightWithSpacing();
}

static void AnimatedSectionBegin(
    const char* id,
    float anim,
    float rowCount,
    float parentAlpha)

{
    if (anim <= 0.001f)
        return;

    float rowH = ImGui::GetFrameHeightWithSpacing();
    float height = rowH * rowCount * anim;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, anim * parentAlpha);
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

static void AnimatedSectionEnd(float anim)
{
    if (anim <= 0.001f)
        return;

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
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
    f << "sound_volume=" << LavenderHook::Globals::sound_volume << "\n";
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
        if (line.rfind("sound_volume", 0) == 0) {
            size_t eq = line.find('=');
            if (eq != std::string::npos)
                LavenderHook::Globals::sound_volume = std::stoi(line.substr(eq + 1));
        }
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
static LavenderHook::UI::LavenderFadeOut g_menu_selector_fade;

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

                if (!initialized_once) {
                    initialized_once = true;
                    LoadTheme();
                    LoadMenuSettings();
                    LoadPerfSettings();
                    LavenderHook::Audio::SetVolumePercent(LavenderHook::Globals::sound_volume);
                    ApplyThemeToImGui();
                }


                float alpha = g_menu_selector_fade.Alpha();

                const float headerHeight = 32.0f + 4.0f;
                const float rowH = ImGui::GetFrameHeightWithSpacing();

                // compute dynamic content height 
                float contentHeight = 0.0f;

                // base options (Info Overlay, Stop on Fail, Process Overlay, Performance Overlay, Windows header)
                contentHeight += 7 * rowH;

                float perfLayoutT = (s_perfAnim > kHideThreshold) ? s_perfAnim : 0.0f;
                contentHeight += 4 * rowH * perfLayoutT;

                float windowsLayoutT = (s_windowsAnim > kHideThreshold) ? s_windowsAnim : 0.0f;
                contentHeight += 10 * rowH * windowsLayoutT;

                // audio section
                contentHeight += rowH; // separator/label
                contentHeight += rowH; // slider


                // theme section
                contentHeight += rowH;       // separator + label
                contentHeight += 3 * rowH;   // color pickers
                contentHeight += rowH;       // reset button
                contentHeight += 16.0f;      // padding

                //  drive header animation 
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
                float animatedHeight =
                    headerHeight + contentHeight * s_headerAnim;

                ImGui::SetNextWindowSize(
                    ImVec2(350.0f, animatedHeight),
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
                    ImGui::PushStyleVar(
                        ImGuiStyleVar_Alpha,
                        alpha * s_headerAnim
                    );

                    // Info Overlay
                    bool b = LavenderHook::Globals::show_info_overlay;
                    if (ImGui::Checkbox("Info Overlay", &b)) {
                        LavenderHook::Globals::show_info_overlay = b;
                        SaveMenuSettings();
                        LavenderHook::Audio::PlayToggleSound(b);
                    }

                    // Stop on Fail
                    if (ImGui::Checkbox("Stop on Fail", &LavenderHook::Globals::stop_on_fail)) {
                        SaveMenuSettings();
                        LavenderHook::Audio::PlayToggleSound(LavenderHook::Globals::stop_on_fail);
                    }

                    // Process Overlay on Hide
                    b = LavenderHook::Globals::show_process_overlay_on_hide;
                    if (ImGui::Checkbox("Process Overlay on Hide", &b)) {
                        LavenderHook::Globals::show_process_overlay_on_hide = b;
                        SaveMenuSettings();
                        LavenderHook::Audio::PlayToggleSound(b);
                    }

                    // Performance Overlay
                    b = LavenderHook::Globals::show_performance_overlay;
                    if (ImGui::Checkbox("Performance Overlay", &b)) {
                        LavenderHook::Globals::show_performance_overlay = b;
                        SavePerfSettings();
                        LavenderHook::Audio::PlayToggleSound(b);
                    }
                    if (DropdownArrowCustom("perf", expand_performance, s_perfArrowAnim, alpha))
                        expand_performance = !expand_performance;

                    if (perfLayoutT > kHideThreshold)
                    {
                        AnimatedSectionBegin("##perf_section", perfLayoutT, 4, alpha * s_headerAnim);
                        ImGui::Indent(18.f);

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

                        ImGui::Unindent(18.f);
                        AnimatedSectionEnd(perfLayoutT);
                    }

                    // Windows section
                    ImGui::Separator();
                    ImGui::TextDisabled("Windows:");
                    if (DropdownArrowCustom("windows", expand_windows, s_windowsArrowAnim, alpha))
                        expand_windows = !expand_windows;

                    if (windowsLayoutT > kHideThreshold)
                    {
                        AnimatedSectionBegin("##windows_section", windowsLayoutT, 10, alpha * s_headerAnim);
                        ImGui::Indent(18.f);

                        bool bW = LavenderHook::Globals::show_general_window;
                        if (ImGui::Checkbox("General Window", &bW)) {
                            LavenderHook::Globals::show_general_window = bW;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(bW);
                        }
                        bW = LavenderHook::Globals::show_misc_window;
                        if (ImGui::Checkbox("Misc Window", &bW)) {
                            LavenderHook::Globals::show_misc_window = bW;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(bW);
                        }
                        bW = LavenderHook::Globals::show_buffing_window;
                        if (ImGui::Checkbox("Buffing Window", &bW)) {
                            LavenderHook::Globals::show_buffing_window = bW;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(bW);
                        }
                        bW = LavenderHook::Globals::show_gamepad_window;
                        if (ImGui::Checkbox("Virtual Controller", &bW)) {
                            LavenderHook::Globals::show_gamepad_window = bW;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(bW);
                        }
                        bW = LavenderHook::Globals::show_profiles_window;
                        if (ImGui::Checkbox("Profiles Window", &bW)) {
                            LavenderHook::Globals::show_profiles_window = bW;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(bW);
                        }
                        bW = LavenderHook::Globals::show_macro_window;
                        if (ImGui::Checkbox("Macro Manager", &bW)) {
                            LavenderHook::Globals::show_macro_window = bW;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(bW);
                        }
                        bW = LavenderHook::Globals::show_paragon_level_window;
                        if (ImGui::Checkbox("Mastery Level", &bW)) {
                            LavenderHook::Globals::show_paragon_level_window = bW;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(bW);
                        }
                        bW = LavenderHook::Globals::show_wave_window;
                        if (ImGui::Checkbox("Wave Overlay", &bW)) {
                            LavenderHook::Globals::show_wave_window = bW;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(bW);
                        }
                        bW = LavenderHook::Globals::show_console;
                        if (ImGui::Checkbox("Console", &bW)) {
                            LavenderHook::Globals::show_console = bW;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(bW);
                        }
                        bW = LavenderHook::Globals::show_menu_logo;
                        if (ImGui::Checkbox("Menu Logo", &bW)) {
                            LavenderHook::Globals::show_menu_logo = bW;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(bW);
                        }
                        ImGui::Unindent(18.f);
                        AnimatedSectionEnd(windowsLayoutT);
                    }

                    // Audio Slider
                    ImGui::Separator();
                    ImGui::TextDisabled("Audio:");
                    ImGui::Indent(8.0f);
                    ImGui::Spacing();
                    int vol = LavenderHook::Globals::sound_volume;
                    float wid = ImGui::GetContentRegionAvail().x * 0.9f;
                    ImGui::SetNextItemWidth(wid);
                    if (ImGui::SliderInt("##sound_volume", &vol, 0, 100)) {
                        LavenderHook::Globals::sound_volume = vol;
                        SaveMenuSettings();
                        LavenderHook::Audio::SetVolumePercent(vol);
                    }

					// Mute Buttons
                    bool mb = LavenderHook::Globals::mute_buttons;
                    if (ImGui::Checkbox("Mute Button Clicks", &mb)) {
                        if (mb) {
                            LavenderHook::Audio::PlayToggleSound(mb);
                            LavenderHook::Globals::mute_buttons = mb;
                            SaveMenuSettings();
                        }
                        else {
                            LavenderHook::Globals::mute_buttons = mb;
                            SaveMenuSettings();
                            LavenderHook::Audio::PlayToggleSound(mb);
                        }
                    }

					// Mute Stop on Fail
                    bool mf = LavenderHook::Globals::mute_fail;
                    if (ImGui::Checkbox("Mute Stop on Fail", &mf)) {
                        LavenderHook::Globals::mute_fail = mf;
                        SaveMenuSettings();
                        LavenderHook::Audio::PlayToggleSound(mf);
                    }

                    ImGui::Unindent(8.0f);

                    ImGui::Separator();
                    ImGui::TextDisabled("Theme Colors:");

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
                    ImGui::PopStyleVar();
                }
                ImGui::End();
                ImGui::PopStyleVar();
            }
        }
    }
}
