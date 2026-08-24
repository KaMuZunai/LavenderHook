#include "DebugWindow.h"
#include "../../misc/Globals.h"

#include "../UIWindowBuilder.h"
#include "../ActionsOverlay.h"
#include "../UIState.h"
#include "../functions/FunctionRegistry.h"
#include "../../config/ConfigManager.h"
#include "../components/LavenderHotkey.h"
#include "../../assets/UITextures.h"
#include "../../assets/TextureCapture.h"
#include <chrono>

namespace LavenderHook::UI::Windows {

    namespace {
        UIWindowBuilder g_window("Debug");
        bool g_inited = false;

        int g_fullscreenBorderlessHotkey = 0;
        int g_textureCaptureHotkey = 0;

        int g_debugWireframeHotkey = 0;
        int g_debugNoFogHotkey = 0;
        int g_debugFreezeHotkey = 0;
        int g_gameSpeedHotkey = 0;

        LavenderHook::UI::Lavender::Hotkey g_rtFullscreenBorderless;
        LavenderHook::UI::Lavender::Hotkey g_rtTextureCapture;

        LavenderHook::UI::Lavender::Hotkey g_rtDebugWireframe;
        LavenderHook::UI::Lavender::Hotkey g_rtDebugNoFog;
        LavenderHook::UI::Lavender::Hotkey g_rtDebugFreeze;
        LavenderHook::UI::Lavender::Hotkey g_rtGameSpeed;
    }

    static void InitOnce()
    {
        if (g_inited)
            return;
        g_inited = true;

        auto& cfg = Config::Store::Instance("debug_window.ini");
        cfg.Load("debug_window.ini");

        g_fullscreenBorderlessHotkey = cfg.EnsureInt("fullscreen_borderless_hotkey", 0);
        g_textureCaptureHotkey = cfg.EnsureInt("texture_capture_hotkey", 0);

        g_debugWireframeHotkey = cfg.EnsureInt("debug_wireframe_hotkey", 0);
        g_debugNoFogHotkey = cfg.EnsureInt("debug_no_fog_hotkey", 0);
        g_debugFreezeHotkey = cfg.EnsureInt("debug_freeze_hotkey", 0);
        g_gameSpeedHotkey = cfg.EnsureInt("game_speed_hotkey", 0);

        g_rtFullscreenBorderless.keyVK = &g_fullscreenBorderlessHotkey;
        g_rtTextureCapture.keyVK = &g_textureCaptureHotkey;

        g_rtDebugWireframe.keyVK = &g_debugWireframeHotkey;
        g_rtDebugNoFog.keyVK = &g_debugNoFogHotkey;
        g_rtDebugFreeze.keyVK = &g_debugFreezeHotkey;
        g_rtGameSpeed.keyVK = &g_gameSpeedHotkey;

        cfg.Save();

        g_window
            .SetHeaderIcon(g_zapIcoTex)

            // Game speed
            .AddToggleDropdown("Game Speed", &LavenderHook::Globals::game_speed_enabled, &g_gameSpeedHotkey)
            .AddItemDescription(R"(Overrides the game's speed.
Hotkey - Select Hotkey to toggle game speed. (Esc binds to None))")
            .AddDropdownSliderFloat("Speed", &LavenderHook::Globals::game_speed, 0.10f, 10.0f, "%.2fx")

            // Utility actions
            .AddToggleDropdown("Fullscreen Borderless", &LavenderHook::Globals::fullscreen_borderless, &g_fullscreenBorderlessHotkey)
            .AddItemDescription(R"(Switches the game to fullscreen borderless mode by removing window borders and maximizing to the monitor.
Hotkey - Select Hotkey to trigger the action. (Esc binds to None))")

            .AddToggleDropdown("Texture Capture", &LavenderHook::Globals::texture_capture_enabled, &g_textureCaptureHotkey)
            .AddItemDescription(R"(Captures and dumps game textures to the DumpedTextures folder.
Hotkey - Select Hotkey to toggle texture capture. (Esc binds to None))")

            // Debug renderer toggles
            .AddToggleDropdown("Wireframe Mode", &LavenderHook::Globals::debug_wireframe, &g_debugWireframeHotkey)
            .AddItemDescription(R"(Forces the game to render in wireframe mode.
Works with DX9 and DX11 renderers.
Hotkey - Select Hotkey to toggle wireframe. (Esc binds to None))")

            .AddToggleDropdown("No Fog", &LavenderHook::Globals::debug_no_fog, &g_debugNoFogHotkey)
            .AddItemDescription(R"(Disables fog rendering.
Works with DX9 renderer.
Hotkey - Select Hotkey to toggle no fog. (Esc binds to None))")

            .AddToggleDropdown("Freeze Frame", &LavenderHook::Globals::debug_freeze, &g_debugFreezeHotkey)
            .AddItemDescription(R"(Freezes the current frame, preventing the game from rendering new frames.
Works with DX9 and DX11 renderers.
Hotkey - Select Hotkey to toggle freeze. (Esc binds to None))");

        // Register functions for profiles
        auto& reg = FunctionRegistry::Instance();
        reg.Register("Game Speed", &LavenderHook::Globals::game_speed_enabled);
        reg.Register("Fullscreen Borderless", &LavenderHook::Globals::fullscreen_borderless);
        reg.Register("Texture Capture", &LavenderHook::Globals::texture_capture_enabled);
        reg.Register("Wireframe Mode", &LavenderHook::Globals::debug_wireframe);
        reg.Register("No Fog", &LavenderHook::Globals::debug_no_fog);
        reg.Register("Freeze Frame", &LavenderHook::Globals::debug_freeze);
    }

    void DebugWindow::Init()
    {
        InitOnce();
    }

    static void DoFullscreenBorderless()
    {
        HWND hwnd = LavenderHook::Globals::window_handle;
        if (!hwnd)
            return;

        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

        style &= ~(WS_OVERLAPPEDWINDOW | WS_CAPTION |
            WS_THICKFRAME | WS_MINIMIZEBOX |
            WS_MAXIMIZEBOX | WS_SYSMENU);

        exStyle &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE);

        SetWindowLong(hwnd, GWL_STYLE, style);
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);

        if (GetMonitorInfo(
            MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
            &mi))
        {
            SetWindowPos(
                hwnd,
                HWND_TOP,
                mi.rcMonitor.left,
                mi.rcMonitor.top,
                mi.rcMonitor.right - mi.rcMonitor.left,
                mi.rcMonitor.bottom - mi.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_NOZORDER
            );
        }

        LavenderHook::Globals::fullscreen_borderless = false;
    }

    void DebugWindow::Render(bool wantVisible)
    {
        InitOnce();

        static int prevVals[6] = {
            g_fullscreenBorderlessHotkey, g_textureCaptureHotkey,
            g_debugWireframeHotkey, g_debugNoFogHotkey, g_debugFreezeHotkey,
            g_gameSpeedHotkey
        };

        g_window.Render(wantVisible);

        int currVals[6] = {
            g_fullscreenBorderlessHotkey, g_textureCaptureHotkey,
            g_debugWireframeHotkey, g_debugNoFogHotkey, g_debugFreezeHotkey,
            g_gameSpeedHotkey
        };

        bool changed = false;
        for (int i = 0; i < 6; ++i)
        {
            if (prevVals[i] != currVals[i])
            {
                changed = true;
                break;
            }
        }

        if (changed)
        {
            auto& cfg = Config::Store::Instance("debug_window.ini");

            cfg.SetInt("fullscreen_borderless_hotkey", g_fullscreenBorderlessHotkey);
            cfg.SetInt("texture_capture_hotkey",       g_textureCaptureHotkey);
            cfg.SetInt("debug_wireframe_hotkey",       g_debugWireframeHotkey);
            cfg.SetInt("debug_no_fog_hotkey",          g_debugNoFogHotkey);
            cfg.SetInt("debug_freeze_hotkey",          g_debugFreezeHotkey);
            cfg.SetInt("game_speed_hotkey",            g_gameSpeedHotkey);

            cfg.Save();

            for (int i = 0; i < 6; ++i)
                prevVals[i] = currVals[i];
        }

        // Apply fullscreen borderless toggle (one-shot, self-disabling)
        if (LavenderHook::Globals::fullscreen_borderless)
        {
            DoFullscreenBorderless();
        }

        // Sync texture capture enabled state
        TextureCapture::SetEnabled(LavenderHook::Globals::texture_capture_enabled);
    }

} // namespace LavenderHook::UI::Windows

void LavenderHook::UI::Windows::DebugWindow::UpdateActions()
{
    using LavenderHook::UI::Actions::SetActive;

    g_rtFullscreenBorderless.UpdateToggle(LavenderHook::Globals::fullscreen_borderless);
    g_rtTextureCapture.UpdateToggle(LavenderHook::Globals::texture_capture_enabled);

    g_rtDebugWireframe.UpdateToggle(LavenderHook::Globals::debug_wireframe);
    g_rtDebugNoFog.UpdateToggle(LavenderHook::Globals::debug_no_fog);
    g_rtDebugFreeze.UpdateToggle(LavenderHook::Globals::debug_freeze);
    g_rtGameSpeed.UpdateToggle(LavenderHook::Globals::game_speed_enabled);

    SetActive("Texture Capture", LavenderHook::Globals::texture_capture_enabled);
    SetActive("Wireframe", LavenderHook::Globals::debug_wireframe);
    SetActive("No Fog", LavenderHook::Globals::debug_no_fog);
    SetActive("Freeze Frame", LavenderHook::Globals::debug_freeze);
    SetActive("Game Speed", LavenderHook::Globals::game_speed_enabled);
}
