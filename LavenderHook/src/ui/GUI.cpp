#include "GUI.h"
#include "UIWindows/SettingsWindow.h"
#include "components/LavenderFadeOut.h"
#include "components/LavenderUI.h"
#include "../misc/Globals.h"
#include "../misc/FileLog.h"
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

static ImFont* LoadFontFromResource(int resId, float sizePixels);

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

ImVec4 MAIN_RED = ImVec4(0.35f, 0.50f, 0.55f, 1.0f);
ImVec4 MID_RED = ImVec4(0.40f, 0.60f, 0.70f, 1.0f);
ImVec4 DARK_RED = ImVec4(0.50f, 0.75f, 0.85f, 1.0f);

float WINDOW_BORDER_SIZE = 0.0f;

GUI::GUI()
{
    LoadTheme();
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

    // Apply full theme (overrides style properties based on use_polished_overlay)
    ApplyThemeToImGui();

    // font
    static bool fontLoaded = false;
    if (!fontLoaded) {
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;

        // Fonts are embedded as DLL resources so the overlay renders identically
        // on any system (including Linux/Proton) with no hardcoded path.
        ImFont* regularFont = LoadFontFromResource(FONT_OPEN_SANS_REGULAR, 24.0f);
        ImFont* semiboldFont = LoadFontFromResource(FONT_OPEN_SANS_SEMIBOLD, 24.0f);

        // Set default font based on theme
        if (semiboldFont && LavenderHook::Globals::use_polished_overlay)
            io.FontDefault = semiboldFont;
        else if (regularFont)
            io.FontDefault = regularFont;
        else if (io.Fonts->Fonts.Size >= 1)
            io.FontDefault = io.Fonts->Fonts[0];

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

static ImFont* LoadFontFromResource(int resId, float sizePixels)
{
    if (!ImGui::GetCurrentContext())
        return nullptr;

    HMODULE mod = nullptr;
    GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCTSTR)&LoadFontFromResource,
        &mod
    );

    HRSRC res = FindResource(mod, MAKEINTRESOURCE(resId), RT_RCDATA);
    if (!res)
        return nullptr;

    HGLOBAL data = LoadResource(mod, res);
    if (!data)
        return nullptr;

    void* ptr = LockResource(data);
    DWORD size = SizeofResource(mod, res);
    if (!ptr || size == 0)
        return nullptr;

    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(ptr, (int)size, sizePixels);
}

static void TryLoadTextures()
{
	static bool all_loaded = false;
	if (all_loaded)
		return;

	struct TexEntry
	{
		int resId;
		Texture* tex;
		ImTextureID* id;
	};

	static const TexEntry kTextures[] = {
		{ DROP_LEFT, &g_dropLeft, &g_dropLeftTex },
		{ DROP_DOWN, &g_dropDown, &g_dropDownTex },
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

	bool any_missing = false;
	for (const auto& e : kTextures)
	{
		if (*e.id == 0)
		{
			any_missing = true;
			LoadTextureFromResource(e.resId, *e.tex, *e.id);
		}
	}
	if (!any_missing)
		all_loaded = true;
}


void GUI::Render()
{
}

void GUI::RenderOverlay()
{
    TryLoadTextures();

    // Startup tooltip: skip entirely after it has expired
    static bool startup_done = false;
    if (!startup_done && g_startup_show_start >= 0.0)
    {
        double now = ImGui::GetTime();
        double elapsed = now - g_startup_show_start;
        const double kDelay = 2.0;
        const double kHold = 6.0;

        bool wantVisible = (elapsed >= kDelay && elapsed < (kDelay + kHold));

        g_startup_fade.Tick(wantVisible);

        if (g_startup_fade.ShouldRender())
        {
            float a = g_startup_fade.Alpha();
            ImDrawList* fdl = ImGui::GetForegroundDrawList();
            ImVec2 ds = ImGui::GetIO().DisplaySize;

            bool polished = LavenderHook::Globals::use_polished_overlay;

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

            if (polished)
            {
                const float r = 8.0f;
                ImU32 shadow = IM_COL32(0, 0, 0, (int)(85 * a));
                fdl->AddRectFilled(ImVec2(p0.x + 1, p0.y + 3), ImVec2(p1.x + 1, p1.y + 4), shadow, r);
                fdl->AddRectFilled(p0, p1, IM_COL32(22, 20, 28, (int)(196 * a)), r);
                fdl->AddRectFilled(p0, ImVec2(p1.x, p0.y + (p1.y - p0.y) * 0.50f),
                    IM_COL32(255, 255, 255, (int)(10 * a)), r, ImDrawFlags_RoundCornersTop);
                fdl->AddRect(p0, p1, LavenderHook::UI::Lavender::PolishedAccent(0.50f * a), r, 0, 1.f);
            }
            else
            {
                ImU32 bg = IM_COL32(20, 20, 20, (int)(200.0f * a));
                ImU32 border = IM_COL32(80, 80, 80, (int)(200.0f * a));
                fdl->AddRectFilled(p0, p1, bg, 8.0f);
                fdl->AddRect(p0, p1, border, 8.0f);
            }

            ImVec2 textPos = ImVec2(pos.x + pad, pos.y + pad);
            ImU32 textCol = IM_COL32(255, 255, 255, (int)(230.0f * a));
            fdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, textCol, line0.c_str());
            fdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(textPos.x, textPos.y + s0.y + spacing), textCol, line1.c_str());
            fdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(textPos.x, textPos.y + s0.y + s1.y + spacing * 2.0f), textCol, line2.c_str());
            fdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(textPos.x, textPos.y + s0.y + s1.y + s2.y + spacing * 3.0f), textCol, line3.c_str());
        }

        if (elapsed > kDelay + kHold + 2.0)
            startup_done = true;
    }

    // Info overlay rendering is handled by InfoOverlayWindow
}
