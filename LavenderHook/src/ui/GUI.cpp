#include "GUI.h"
#include "components/LavenderGradient.h"
#include "../windows/ToggleMenuWindow.h"
#include "components/LavenderFadeOut.h"
#include "../assets/TextureLoader.h"
#include "../assets/resources/resource.h"
#include "../sound/SoundPlayer.h"
#include <windows.h>
#include <winver.h>

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cmath>

Texture g_dropLeft;
Texture g_dropDown;
Texture g_menuLogo;

Texture g_emptyIco;

Texture g_arrowIco;
Texture g_dotsIco;
Texture g_infoIco;
Texture g_menuIco;
Texture g_sparkleIco;
Texture g_speedIco;
Texture g_starIco;
Texture g_swordIco;
Texture g_wrenchIco;
Texture g_zapIco;

Texture g_halloweenGirl;
Texture g_necroGirl;
Texture g_moonGirl;
Texture g_snowGirl;
Texture g_cloverGirl;
Texture g_loveGirl;
Texture g_orbGirl;
Texture g_owlGirl;

ImTextureID g_dropLeftTex = 0;
ImTextureID g_dropDownTex = 0;
ImTextureID g_menuLogoTex = 0;

ImTextureID g_emptyIcoTex = 0;

ImTextureID g_arrowIcoTex = 0;
ImTextureID g_dotsIcoTex = 0;
ImTextureID g_infoIcoTex = 0;
ImTextureID g_menuIcoTex = 0;
ImTextureID g_sparkleIcoTex = 0;
ImTextureID g_speedIcoTex = 0;
ImTextureID g_starIcoTex = 0;
ImTextureID g_swordIcoTex = 0;
ImTextureID g_wrenchIcoTex = 0;
ImTextureID g_zapIcoTex = 0;

ImTextureID g_halloweenGirlTex = 0;
ImTextureID g_necroGirlTex = 0;
ImTextureID g_moonGirlTex = 0;
ImTextureID g_snowGirlTex = 0;
ImTextureID g_cloverGirlTex = 0;
ImTextureID g_loveGirlTex = 0;
ImTextureID g_orbGirlTex = 0;
ImTextureID g_owlGirlTex = 0;



static double g_lastTextureTry = 0.0;

// Startup tooltip fade state
static LavenderHook::UI::LavenderFadeOut g_startup_fade;
static double g_startup_show_start = -1.0;

// Show the startup tooltip.
void DisplayStartupToolTip()
{
    g_startup_show_start = ImGui::GetTime();
    g_startup_fade.SetSpeed(4.0f);
    g_startup_fade.SetVisible(false);
}

static std::string GetFileVersionString()
{
    static std::string s;
    if (!s.empty()) return s;

    HMODULE mod = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&GetFileVersionString,
        &mod
    );

    char path[MAX_PATH] = {0};
    if (GetModuleFileNameA(mod, path, MAX_PATH) == 0)
        return s;

    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeA(path, &dummy);
    if (size == 0) return s;

    std::vector<char> data(size);
    if (!GetFileVersionInfoA(path, 0, size, data.data())) return s;

    struct LANGANDCODEPAGE { WORD wLanguage; WORD wCodePage; } *trans = nullptr;
    UINT transSize = 0;
    if (VerQueryValueA(data.data(), "\\VarFileInfo\\Translation", (LPVOID*)&trans, &transSize) && transSize >= sizeof(LANGANDCODEPAGE)) {
        char subblock[64] = {0};
        sprintf_s(subblock, "\\StringFileInfo\\%04x%04x\\FileVersion", trans->wLanguage, trans->wCodePage);

        LPSTR verBuf = nullptr;
        UINT verSize = 0;
        if (VerQueryValueA(data.data(), subblock, (LPVOID*)&verBuf, &verSize) && verBuf && verSize > 0) {
            s.assign(verBuf, verSize);
            // strip trailing nulls/spaces
            while (!s.empty() && (s.back() == '\0' || s.back() == '\n' || s.back() == '\r')) s.pop_back();
            return s;
        }
    }

    return s;
}

extern bool LoadTheme();
extern void ApplyThemeToImGui();
extern void LoadMenuSettings();
extern void InitMenuScale();
namespace LavenderHook::UI::Windows {
    void LoadPerfSettings();
}

namespace LavenderHook {
    namespace UI {
        namespace Actions {

            static std::unordered_set<std::string> gActive;

            void SetActive(const std::string& label, bool on) {
                if (on) gActive.insert(label);
                else    gActive.erase(label);
            }

            void ClearAll() { gActive.clear(); }

            std::vector<std::string> GetActiveList() {
                return std::vector<std::string>(gActive.begin(), gActive.end());
            }

            void ClearByPrefix(const std::string& prefix) {
                std::vector<std::string> toErase;
                toErase.reserve(gActive.size());
                for (const auto& s : gActive) {
                    if (s.rfind(prefix, 0) == 0) toErase.emplace_back(s);
                }
                for (const auto& s : toErase) gActive.erase(s);
            }

        }
    }
} // namespace LavenderHook::UI::Actions

ImVec4 MAIN_RED = ImVec4(0.6310878396034241f, 0.5130504965782166f, 0.7424892783164978f, 1.0f);
ImVec4 MID_RED = ImVec4(0.7018406391143799f, 0.544309139251709f, 0.8454935550689697f, 1.0f);
ImVec4 DARK_RED = ImVec4(0.7300597429275513f, 0.4847022593021393f, 0.9570815563201904f, 1.0f);

float WINDOW_BORDER_SIZE = 0.0f;


void ApplyThemeToImGui()
{
    ImGuiStyle& style = ImGui::GetStyle();

    style.Colors[ImGuiCol_Border] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.92f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.56f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.56f);

    style.Colors[ImGuiCol_Button] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.50f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.75f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.92f);

    style.Colors[ImGuiCol_Header] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.65f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.80f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.92f);

    style.Colors[ImGuiCol_Tab] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.44f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.92f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.88f);

    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.92f);

    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(DARK_RED.x, DARK_RED.y, DARK_RED.z, 0.92f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(DARK_RED.x, DARK_RED.y, DARK_RED.z, 0.92f);

    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.56f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.78f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.78f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.78f);
}

GUI::GUI()
{
    LoadTheme();
    ApplyThemeToImGui();
    LoadMenuSettings();
    LavenderHook::Audio::SetVolumePercent(LavenderHook::Globals::sound_volume);
    LoadPerfSettings();
	DisplayStartupToolTip();

    // LavenderHookTheme
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.6000000238418579f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.WindowRounding = 10.0f;
    style.WindowBorderSize = WINDOW_BORDER_SIZE;
    style.WindowMinSize = ImVec2(32.0f, 32.0f);
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.ChildRounding = 5.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 5.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(4.0f, 3.0f);
    style.FrameRounding = 5.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.CellPadding = ImVec2(4.0f, 2.0f);
    style.IndentSpacing = 21.0f;
    style.ColumnsMinSpacing = 6.0f;
    style.ScrollbarSize = 14.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 5.0f;
    style.TabBorderSize = 0.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 0.9215686f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.498039f, 0.498039f, 0.498039f, 0.9215686f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.100f, 0.100f, 0.100f, 0.75f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.054902f, 0.054902f, 0.054902f, 0.478431f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.478431f);
    style.Colors[ImGuiCol_Border] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.9215686f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.705882f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0392157f, 0.0392157f, 0.0392157f, 0.75f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.564706f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.564706f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.75);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 85);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 85);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.0784314f, 0.0784314f, 0.0784314f, 0.784314f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.564706f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.784314f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.784314f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.784314f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.784314f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.815686f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.815686f);
    style.Colors[ImGuiCol_Button] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.501961f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.745098f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.921569f);
    style.Colors[ImGuiCol_Header] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.654902f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.803922f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.921569f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.0784314f, 0.0784314f, 0.0784314f, 0.501961f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.0784314f, 0.0784314f, 0.0784314f, 0.669528f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.0784314f, 0.0784314f, 0.0784314f, 0.957082f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.101961f, 0.113725f, 0.129412f, 0.2f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.203922f, 0.207843f, 0.215686f, 0.2f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.301961f, 0.301961f, 0.301961f, 0.2f);
    style.Colors[ImGuiCol_Tab] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.439216f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.882353f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.921569f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.0666667f, 0.0666667f, 0.0666667f, 0.901961f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.0666667f, 0.0666667f, 0.0666667f, 0.921569f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.607843f, 0.607843f, 0.607843f, 0.921569f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.935622f, 0.313213f, 0.503746f, 0.921569f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(DARK_RED.x, DARK_RED.y, DARK_RED.z, 0.921569f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(DARK_RED.x, DARK_RED.y, DARK_RED.z, 0.921569f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.921569f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.921569f);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(MAIN_RED.x, MAIN_RED.y, MAIN_RED.z, 0.921569f);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.921569f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.0980392f, 0.0980392f, 0.0980392f, 0.921569f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(MID_RED.x, MID_RED.y, MID_RED.z, 0.921569f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.258824f, 0.270588f, 0.380392f, 0.921569f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.180392f, 0.227451f, 0.278431f, 0.921569f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.35f);


    // font
    static bool fontLoaded = false;
    if (!fontLoaded) {
        ImGuiIO& io = ImGui::GetIO();
        char winDir[MAX_PATH]; size_t outlen = 0;
        getenv_s(&outlen, winDir, sizeof(winDir), "WINDIR");
        std::string fontPath = outlen > 0 ? std::string(winDir) + "\\Fonts\\segoeui.ttf" : "C:\\Windows\\Fonts\\segoeui.ttf";
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 24.0f);
        fontLoaded = true;
    }

    InitMenuScale();
}

static bool LoadTextureFromResource(int resId, Texture& outTex, ImTextureID& outId)
{
    if (!TextureLoader::IsInitialized())
        return false;

    HMODULE mod = nullptr;
    GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCTSTR)&LoadTextureFromResource,
        &mod
    );

    HRSRC res = FindResource(mod, MAKEINTRESOURCE(resId), RT_RCDATA);
    if (!res)
        return false;

    HGLOBAL data = LoadResource(mod, res);
    if (!data)
        return false;

    void* ptr = LockResource(data);
    DWORD size = SizeofResource(mod, res);
    if (!ptr || size == 0)
        return false;

    outTex = TextureLoader::LoadFromMemory(ptr, size);
    outId = outTex.id;

    return outId != 0;
}

static void TryLoadTextures()
{
	struct TexEntry
	{
		int resId;
		Texture* tex;
		ImTextureID* id;
	};

	static const TexEntry kTextures[] = {
		{ DROP_LEFT, &g_dropLeft, &g_dropLeftTex },
		{ DROP_DOWN, &g_dropDown, &g_dropDownTex },
		{ MENU_LOGO, &g_menuLogo, &g_menuLogoTex },
		{ EMPTY_ICO, &g_emptyIco, &g_emptyIcoTex },
		{ ARROW_ICO, &g_arrowIco, &g_arrowIcoTex },
		{ DOTS_ICO, &g_dotsIco, &g_dotsIcoTex },
		{ INFO_ICO, &g_infoIco, &g_infoIcoTex },
		{ MENU_ICO, &g_menuIco, &g_menuIcoTex },
		{ SPARKLE_ICO, &g_sparkleIco, &g_sparkleIcoTex },
		{ SPEED_ICO, &g_speedIco, &g_speedIcoTex },
		{ STAR_ICO, &g_starIco, &g_starIcoTex },
		{ SWORD_ICO, &g_swordIco, &g_swordIcoTex },
		{ WRENCH_ICO, &g_wrenchIco, &g_wrenchIcoTex },
		{ ZAP_ICO, &g_zapIco, &g_zapIcoTex },
		{ HALLOWEEN_GIRL, &g_halloweenGirl, &g_halloweenGirlTex },
		{ NECRO_GIRL, &g_necroGirl, &g_necroGirlTex },
		{ MOON_GIRL, &g_moonGirl, &g_moonGirlTex },
		{ SNOW_GIRL, &g_snowGirl, &g_snowGirlTex },
		{ CLOVER_GIRL, &g_cloverGirl, &g_cloverGirlTex },
		{ LOVE_GIRL, &g_loveGirl, &g_loveGirlTex },
		{ ORB_GIRL, &g_orbGirl, &g_orbGirlTex },
		{ OWL_GIRL, &g_owlGirl, &g_owlGirlTex },
	};

	for (const auto& e : kTextures)
	{
		if (*e.id == 0)
			LoadTextureFromResource(e.resId, *e.tex, *e.id);
	}
}


void GUI::Render()
{
}

static LavenderHook::UI::LavenderFadeOut g_info_overlay_fade;

void GUI::RenderOverlay()
{
    TryLoadTextures();

    if (g_startup_show_start >= 0.0) {
        double now = ImGui::GetTime();
        double elapsed = now - g_startup_show_start;
        const double kDelay = 2.0; // seconds to wait before starting fade-in
        const double kHold = 6.0; // seconds to keep visible after fade-in
        bool wantVisible = false;

        if (elapsed >= kDelay && elapsed < (kDelay + kHold)) {
            wantVisible = true;
        }

        g_startup_fade.Tick(wantVisible);

        if (g_startup_fade.ShouldRender()) {
            float a = g_startup_fade.Alpha();
            ImDrawList* fdl = ImGui::GetForegroundDrawList();
            ImVec2 ds = ImGui::GetIO().DisplaySize;

            const std::string line0 = std::string("Version ") + (GetFileVersionString().empty() ? "Unknown" : GetFileVersionString());
            const std::string line1 = "Press \"Insert\" to Show/Hide menu.";
            const std::string line2 = "Press \"CTRL + F1\" to Show/Hide menu.";
            const std::string line3 = "Hold Right Click on buttons to see a Tooltip.";

            ImVec2 s0 = ImGui::CalcTextSize(line0.c_str());
            ImVec2 s1 = ImGui::CalcTextSize(line1.c_str());
            ImVec2 s2 = ImGui::CalcTextSize(line2.c_str());
            ImVec2 s3 = ImGui::CalcTextSize(line3.c_str());

            const float pad = 12.0f;
            const float spacing = 6.0f;
            float boxW = std::max(std::max(s0.x, s1.x), std::max(s2.x, s3.x)) + pad * 2.0f;
            float boxH = s0.y + s1.y + s2.y + s3.y + spacing * 3.0f + pad * 2.0f;

            ImVec2 pos = ImVec2((ds.x - boxW) * 0.5f, (ds.y - boxH) * 0.5f);
            ImVec2 p0 = pos;
            ImVec2 p1 = ImVec2(pos.x + boxW, pos.y + boxH);

            ImU32 bg = IM_COL32(20, 20, 20, (int)(200.0f * a));
            ImU32 border = IM_COL32(80, 80, 80, (int)(200.0f * a));
            fdl->AddRectFilled(p0, p1, bg, 8.0f);
            fdl->AddRect(p0, p1, border, 8.0f);

            ImVec2 textPos = ImVec2(pos.x + pad, pos.y + pad);
            ImU32 textCol = IM_COL32(255, 255, 255, (int)(230.0f * a));
            fdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, textCol, line0.c_str());
            fdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(textPos.x, textPos.y + s0.y + spacing), textCol, line1.c_str());
            fdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(textPos.x, textPos.y + s0.y + s1.y + spacing * 2.0f), textCol, line2.c_str());
            fdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(textPos.x, textPos.y + s0.y + s1.y + s2.y + spacing * 3.0f), textCol, line3.c_str());
        }
    }

    g_info_overlay_fade.Tick(LavenderHook::Globals::show_info_overlay);
    if (!g_info_overlay_fade.ShouldRender())
        return;

    float alpha = g_info_overlay_fade.Alpha();
    using namespace LavenderHook::UI::Lavender;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

    ImGui::Begin(
        "##overlay_root",
        nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBackground
    );

    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float screenW = ImGui::GetIO().DisplaySize.x;
    const float margin = 28.f;
    const float lineH = ImGui::GetTextLineHeight();
    const float padY = 6.f;
    const float itemH = lineH + padY * 2.f;
    const float itemSpacing = 6.f;

    float cursorY = margin;

    auto DrawTextBg = [&](const ImVec2& pos, const std::string& text, float a)
        {
            const float padX = 10.f;
            const float rounding = 7.f;

            ImVec2 sz = ImGui::CalcTextSize(text.c_str());

            ImVec2 p0 = pos;
            ImVec2 p1 = ImVec2(
                pos.x + sz.x + padX * 2.f,
                pos.y + sz.y + padY * 2.f
            );

            ImU32 bg = IM_COL32(20, 20, 20, (int)(120 * a));
            dl->AddRectFilled(p0, p1, bg, rounding);

            ImGui::SetCursorScreenPos(ImVec2(
                pos.x + padX,
                pos.y + padY
            ));

            GradientText(text, a);
        };

    static double prevTime = ImGui::GetTime();
    float dt = (float)(ImGui::GetTime() - prevTime);
    prevTime = ImGui::GetTime();

    cursorY += 12.f;
    float actionsBaseY = cursorY;

    // Actions
    auto activeRaw = LavenderHook::UI::Actions::GetActiveList();

    struct Measured { std::string label; float width; };
    std::vector<Measured> measured;

    for (auto& s : activeRaw)
        measured.push_back({ s, ImGui::CalcTextSize(s.c_str()).x });

    std::sort(measured.begin(), measured.end(),
        [](auto& a, auto& b) {
            if (a.width != b.width) return a.width > b.width;
            return a.label < b.label;
        });

    std::vector<std::string> active;
    for (auto& m : measured)
        active.push_back(m.label);

    struct AnimState {
        float pos;
        float alpha;
        float slide;
        int target;
        bool exiting;
    };

    static std::unordered_map<std::string, AnimState> anim;

    for (auto& kv : anim)
        kv.second.target = -1;

    for (int i = 0; i < (int)active.size(); ++i)
    {
        auto& label = active[i];
        auto it = anim.find(label);

        if (it == anim.end())
            anim[label] = { (float)active.size(), 0.f, 60.f, i, false };
        else {
            it->second.target = i;
            it->second.exiting = false;
        }
    }

    for (auto& kv : anim)
        if (kv.second.target == -1)
            kv.second.exiting = true;

    for (auto& kv : anim)
    {
        auto& a = kv.second;
        a.pos += (a.target - a.pos) * std::clamp(12.f * dt, 0.f, 1.f);

        if (!a.exiting) {
            a.alpha += (1.f - a.alpha) * std::clamp(10.f * dt, 0.f, 1.f);
            a.slide += (0.f - a.slide) * std::clamp(10.f * dt, 0.f, 1.f);
        }
        else {
            a.alpha += (0.f - a.alpha) * std::clamp(8.f * dt, 0.f, 1.f);
            a.slide += (80.f - a.slide) * std::clamp(8.f * dt, 0.f, 1.f);
        }
    }

    struct DrawItem { std::string label; float pos, alpha, slide; };
    std::vector<DrawItem> draw;

    for (auto it = anim.begin(); it != anim.end(); )
    {
        if (it->second.exiting && it->second.alpha < 0.02f)
            it = anim.erase(it);
        else {
            draw.push_back({ it->first, it->second.pos, it->second.alpha, it->second.slide });
            ++it;
        }
    }

    std::sort(draw.begin(), draw.end(),
        [](auto& a, auto& b) { return a.pos < b.pos; });

    for (auto& d : draw)
    {
        float w = ImGui::CalcTextSize(d.label.c_str()).x;
        DrawTextBg(
            ImVec2(
                screenW - w - margin - 18.f + d.slide,
                actionsBaseY + d.pos * (itemH + itemSpacing)
            ),
            d.label,
            alpha * d.alpha
        );
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
}
