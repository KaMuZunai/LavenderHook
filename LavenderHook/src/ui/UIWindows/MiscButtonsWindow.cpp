#include "MiscButtonsWindow.h"
#include "functions/MiscButtonActions.h"
#include "functions/FunctionRegistry.h"
#include "../../misc/logmonitor/LogMonitor.h"
#include "../../misc/Globals.h"
#include "../../sound/SoundPlayer.h"

#include "../../ui/UIWindowBuilder.h"
#include "../../ui/UIState.h"
#include "../../ui/ActionsOverlay.h"
#include "../../config/ConfigManager.h"
#include "../../ui/components/LavenderHotkey.h"
#include "../../assets/UITextures.h"

namespace LavenderHook::UI::Windows {

    namespace {
        StateTable g_state;
        UIWindowBuilder g_window("Misc");
        bool g_inited = false;


        int g_flashHealHotkey = 0;
        int g_keepTriangleHotkey = 0;
        int g_minimizeHotkey = 0;

        LavenderHook::UI::Lavender::Hotkey g_rtFlashHeal;
        LavenderHook::UI::Lavender::Hotkey g_rtKeepTriangle;
        LavenderHook::UI::Lavender::Hotkey g_rtMinimize;

        bool g_loadedOnce = false;
    }

    static void InitOnce()
    {
        if (g_inited)
            return;
        g_inited = true;

        g_window.SetHeaderIcon(g_starIcoTex);

        auto& cfg = Config::Store::Instance("misc.ini");
        cfg.Load("misc.ini");

        auto& flashHeal = g_state.Toggle("Flash Heal Fix");

        g_flashHealHotkey = cfg.EnsureInt("flash_heal_hotkey", 0);
        g_minimizeHotkey = cfg.EnsureInt("minimize_hotkey", 0);
        g_keepTriangleHotkey = cfg.EnsureInt("keep_triangle_hotkey", 0);

        auto& keepTri = g_state.Toggle("Always Cursor");
        keepTri.enabled = false;

        g_rtFlashHeal.keyVK = &g_flashHealHotkey;
        g_rtKeepTriangle.keyVK = &g_keepTriangleHotkey;
        g_rtMinimize.keyVK = &g_minimizeHotkey;

        g_loadedOnce = true;

        g_window.AddToggleDropdown(
            "Flash Heal Fix",
            &flashHeal.enabled,
            &g_flashHealHotkey
        ).AddItemDescription(R"(Centers the Cursor in the middle of the screen to fix bugged flash heals.
additional configurations:
Hotkey - Select Hotkey to toggle the button. (Esc binds to None))");

        g_window.AddToggleDropdown(
            "Always Cursor",
            &keepTri.enabled,
            &g_keepTriangleHotkey
        ).AddItemDescription(R"(Will display a custom cursor at all times.
additional configurations:
Hotkey - Select Hotkey to toggle the button. (Esc binds to None))");

		auto& minimize = g_state.Toggle("Hide Window");
		minimize.enabled = false;

		g_window.AddToggleDropdown(
			"Hide Window",
			&minimize.enabled,
			&g_minimizeHotkey
		).AddItemDescription(R"(Hides the game window but keeps it active. (Restore with System Tray icon)
additional configurations:
Hotkey - Select Hotkey to trigger minimizing the window. (Esc binds to None))");

		// Register toggles with the global function registry
		{
			auto& reg = LavenderHook::UI::FunctionRegistry::Instance();
			reg.Register("Flash Heal Fix", &flashHeal.enabled);
			reg.Register("Always Cursor",  &keepTri.enabled);
			reg.Register("Hide Window",    &minimize.enabled);
		}
	}

    void MiscButtonsWindow::Init()
    {
        InitOnce();
    }

    void MiscButtonsWindow::Render(bool wantVisible)
    {
        InitOnce();

        static int prevFlashHK = g_flashHealHotkey;
        static int prevKeepTriangleHK = g_keepTriangleHotkey;
        static int prevMinimizeHK = g_minimizeHotkey;

        g_window.Render(wantVisible);


        // persist hotkey changes
        if (prevFlashHK != g_flashHealHotkey)
        {
            auto& cfg = Config::Store::Instance("misc.ini");
            cfg.SetInt("flash_heal_hotkey", g_flashHealHotkey);
            cfg.Save();

            prevFlashHK = g_flashHealHotkey;
        }

        if (prevKeepTriangleHK != g_keepTriangleHotkey)
        {
            auto& cfg = Config::Store::Instance("misc.ini");
            cfg.SetInt("keep_triangle_hotkey", g_keepTriangleHotkey);
            cfg.Save();
            prevKeepTriangleHK = g_keepTriangleHotkey;
        }

        if (prevMinimizeHK != g_minimizeHotkey)
        {
            auto& cfg = Config::Store::Instance("misc.ini");
            cfg.SetInt("minimize_hotkey", g_minimizeHotkey);
            cfg.Save();
            prevMinimizeHK = g_minimizeHotkey;
        }
    }

} // namespace LavenderHook::UI::Windows

void LavenderHook::UI::Windows::MiscButtonsWindow::UpdateActions()
{
    if (!g_loadedOnce)
        return;

    auto& flashHeal = g_state.Toggle("Flash Heal Fix");

    g_rtFlashHeal.UpdateToggle(flashHeal.enabled);

    auto& keepTri = g_state.Toggle("Always Cursor");
    g_rtKeepTriangle.UpdateToggle(keepTri.enabled);

    auto& minimize = g_state.Toggle("Hide Window");
    g_rtMinimize.UpdateToggle(minimize.enabled);

    // If stop_on_fail is enabled and the log indicates an abort, force disable
    if (LavenderHook::Globals::stop_on_fail && LavenderHook::LogMonitor::LatestLineHasAbort())
    {
        flashHeal.enabled = false;
        LavenderHook::Input::TickButton11(false);
        LavenderHook::UI::Actions::SetActive("Flash Heal Fix", false);
        // Play stop-on-fail sound implementation
        LavenderHook::Audio::PlayFailSound();
        return;
    }

    LavenderHook::Input::TickButton11(flashHeal.enabled);

    LavenderHook::Input::TickButton13(minimize.enabled);

    if (LavenderHook::Globals::minimize_auto_clear_requested) {
        minimize.enabled = false;
        g_rtMinimize.UpdateToggle(minimize.enabled);
        LavenderHook::Globals::minimize_auto_clear_requested = false;
    }

    LavenderHook::UI::Actions::SetActive(
        "Flash Heal Fix",
        flashHeal.enabled
    );

    LavenderHook::UI::Actions::SetActive(
        "Hide Window",
        minimize.enabled
    );

    LavenderHook::Globals::show_triangle_when_menu_hidden = keepTri.enabled;
    LavenderHook::UI::Actions::SetActive(
        "Always Cursor",
        keepTri.enabled
    );
}
