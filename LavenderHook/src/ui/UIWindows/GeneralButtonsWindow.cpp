#include "GeneralButtonsWindow.h"
#include "functions/GeneralButtonActions.h"
#include "functions/FunctionRegistry.h"
#include "../../misc/logmonitor/LogMonitor.h"
#include "../../misc/Globals.h"

#include "../../ui/UIWindowBuilder.h"
#include "../../ui/ActionsOverlay.h"
#include "../../ui/UIState.h"
#include "../../config/ConfigManager.h"
#include "../../ui/components/LavenderHotkey.h"
#include "../../assets/UITextures.h"
#include <chrono>

namespace LavenderHook::UI::Windows {

    namespace {
        StateTable g_state;
        UIWindowBuilder g_window("General");
        bool g_inited = false;

        // UI owned settings
        int g_autoGHoldMs = 2000;
        int g_autoGDelayMs = 3000;
        int g_autoGPressKey = 0;
        int g_autoCtrlGCtrlKey = 0;
        int g_autoCtrlGGKey = 0;
        int g_skipPressKey = 0;
        int g_autoCtrlGHoldMs = 5000;
        int g_autoCtrlGDelayMs = 10000;
        int g_skipHoldMs = 2000;
        int g_skipDelayMs = 4000;
        int g_autoClickIntervalMs = 100;

        int g_autoGHotkey = 0;
        int g_autoCtrlGHotkey = 0;
        int g_skipHotkey = 0;
        int g_autoClickHotkey = 0;

        // Runtime hotkeys
        LavenderHook::UI::Lavender::Hotkey g_rtAutoG;
        LavenderHook::UI::Lavender::Hotkey g_rtAutoCtrlG;
        LavenderHook::UI::Lavender::Hotkey g_rtSkipCut;

        bool g_loadedOnce = false;
    }

    static void InitOnce()
    {
        if (g_inited)
            return;
        g_inited = true;

        auto& cfg = Config::Store::Instance("general_buttons.ini");
        cfg.Load("general_buttons.ini");

        auto& autoG = g_state.Toggle("Auto G");
        auto& autoCtrlG = g_state.Toggle("Auto Ctrl+G");
        auto& skipCut = g_state.Toggle("Skip Cutscene");
        auto& autoClick = g_state.Toggle("Auto Clicker");

        // Ensure defaults
        g_autoGHotkey = cfg.EnsureInt("auto_g_hotkey", 0);
        g_autoGPressKey = cfg.EnsureInt("auto_g_press_key", 'G');
        g_autoCtrlGCtrlKey = cfg.EnsureInt("auto_ctrlg_ctrl_key", VK_CONTROL);
        g_autoCtrlGGKey = cfg.EnsureInt("auto_ctrlg_g_key", 'G');
        g_skipPressKey = cfg.EnsureInt("skip_cutscene_key", VK_SPACE);
        g_autoCtrlGHotkey = cfg.EnsureInt("auto_ctrlg_hotkey", 0);
        g_skipHotkey = cfg.EnsureInt("skip_cutscene_hotkey", 0);

        g_autoGHoldMs = cfg.EnsureInt("auto_g_hold_ms", 2000);
        g_autoGDelayMs = cfg.EnsureInt("auto_g_delay_ms", 3000);
        g_autoCtrlGHoldMs = cfg.EnsureInt("auto_ctrlg_hold_ms", 5000);
        g_autoCtrlGDelayMs = cfg.EnsureInt("auto_ctrlg_delay_ms", 10000);
        g_skipHoldMs = cfg.EnsureInt("skip_hold_ms", 2000);
        g_skipDelayMs = cfg.EnsureInt("skip_delay_ms", 4000);
        g_autoClickIntervalMs = cfg.EnsureInt("auto_click_interval_ms", 100);
        g_autoClickHotkey = cfg.EnsureInt("auto_click_hotkey", 0);

        LavenderHook::Globals::stop_at_wave_5 = cfg.EnsureInt("stop_at_wave_5", 0) != 0;

        // Bind runtime hotkeys
        g_rtAutoG.keyVK = &g_autoGHotkey;
        g_rtAutoCtrlG.keyVK = &g_autoCtrlGHotkey;
        g_rtSkipCut.keyVK = &g_skipHotkey;

        // configure action press keys
        LavenderHook::UI::Functions::SetAutoGKey(g_autoGPressKey);
        LavenderHook::UI::Functions::SetAutoCtrlGCtrlKey(g_autoCtrlGCtrlKey);
        LavenderHook::UI::Functions::SetAutoCtrlGGKey(g_autoCtrlGGKey);
        LavenderHook::UI::Functions::SetSkipCutsceneKey(g_skipPressKey);
        
        cfg.Save(); // only writes if defaults were injected
        g_loadedOnce = true;

        // Register toggles with the global function registry
        {
            auto& reg = LavenderHook::UI::FunctionRegistry::Instance();
            reg.Register("Ready Up",      &autoG.enabled);
            reg.Register("Force Ready Up", &autoCtrlG.enabled);
            reg.Register("Skip Cutscene",  &skipCut.enabled);
            reg.Register("Auto Clicker",   &autoClick.enabled);
        }

        // Build UI
        g_window
            .SetHeaderIcon(g_menuIcoTex)
            .AddToggleDropdown("Ready Up", &autoG.enabled, &g_autoGHotkey)
            .AddItemDescription(R"(Presses the Ready Up Button by holding the configured button.
additional configurations:
Hotkey - Select Hotkey to toggle the button. (Esc binds to None)
Key - Key that will be pressed when button is enabled (Default: G).
Hold - Time the Key is being held down before released.
Delay - Delay before it repeats after the last action has finished.
Stop at 5 - Disables the button 10 seconds after a wave multiple of 5 is reached.)")
            .AddDropdownButton("Key:", &g_autoGPressKey)
            .AddDropdownTimingSeconds("Hold:", &g_autoGHoldMs, 100, 20000)
            .AddDropdownTimingSeconds("Delay:", &g_autoGDelayMs, 100, 20000)
            .AddDropdownCheckbox("Stop at 5", &LavenderHook::Globals::stop_at_wave_5)

            .AddToggleDropdown("Force Ready Up", &autoCtrlG.enabled, &g_autoCtrlGHotkey)
            .AddItemDescription(R"(Presses the Force Ready Up Combo by Pressing the configured buttons.
additional configurations:
Hotkey - Select Hotkey to toggle the button. (Esc binds to None)
CTRL - Key that will be held down alongside the other button. (Default: CTRL)
G - Key that will be held down to force the start. (Default: G)
Hold - Time the Key is being held down before released.
Delay - Delay before it repeats after the last action has finished.
Stop at 5 - Disables the button 10 seconds after a wave multiple of 5 is reached.)")
            .AddDropdownButton("CTRL:", &g_autoCtrlGCtrlKey)
            .AddDropdownButton("G:", &g_autoCtrlGGKey)
            .AddDropdownTimingSeconds("Hold:", &g_autoCtrlGHoldMs, 5000, 20000)
            .AddDropdownTimingSeconds("Delay:", &g_autoCtrlGDelayMs, 100, 60000)
            .AddDropdownCheckbox("Stop at 5", &LavenderHook::Globals::stop_at_wave_5)

            .AddToggleDropdown("Skip Cutscene", &skipCut.enabled, &g_skipHotkey)
            .AddItemDescription(R"(Skips Cutscenes by holding the configured button.
additional configurations:
Hotkey - Select Hotkey to toggle the button. (Esc binds to None)
Key - Key that will be pressed when button is enabled. (Default: SPACE)
Hold - Time the Key is being held down before released.
Delay - Delay before it repeats after the last action has finished.)")
            .AddDropdownButton("Key:", &g_skipPressKey)
            .AddDropdownTimingSeconds("Hold:", &g_skipHoldMs, 2000, 20000)
            .AddDropdownTimingSeconds("Delay:", &g_skipDelayMs, 100, 20000)

            .AddToggleDropdown("Auto Clicker", &autoClick.enabled, &g_autoClickHotkey)
            .AddItemDescription(R"(Spams Left Click for relaxed tower upgrading.
additional configurations:
Hotkey - Select Hotkey to toggle the button. (Esc binds to None)
Interval - Time it takes for the action to repeat.)")
            .AddDropdownTiming("Interval:", &g_autoClickIntervalMs, 50, 200);
    }

    // Public
    void GeneralButtonsWindow::Init()
    {
        InitOnce();
    }

    void GeneralButtonsWindow::Render(bool wantVisible)
    {
        InitOnce();

        // Snapshot previous values
        static int prevVals[15] = {
            g_autoGHotkey, g_autoCtrlGHotkey, g_skipHotkey, g_autoClickHotkey,
            g_autoGPressKey, g_autoCtrlGCtrlKey, g_autoCtrlGGKey, g_skipPressKey,
            g_autoGHoldMs, g_autoGDelayMs,
            g_autoCtrlGHoldMs, g_autoCtrlGDelayMs,
            g_skipHoldMs, g_skipDelayMs, g_autoClickIntervalMs
        };

        g_window.Render(wantVisible);

        // If the press-key was changed in the UI, propagate immediately to runtime actions
        if (LavenderHook::UI::Functions::GetAutoGKey() != g_autoGPressKey) {
            LavenderHook::UI::Functions::SetAutoGKey(g_autoGPressKey);
        }
        if (LavenderHook::UI::Functions::GetAutoCtrlGCtrlKey() != g_autoCtrlGCtrlKey) {
            LavenderHook::UI::Functions::SetAutoCtrlGCtrlKey(g_autoCtrlGCtrlKey);
        }
        if (LavenderHook::UI::Functions::GetAutoCtrlGGKey() != g_autoCtrlGGKey) {
            LavenderHook::UI::Functions::SetAutoCtrlGGKey(g_autoCtrlGGKey);
        }
        if (LavenderHook::UI::Functions::GetSkipCutsceneKey() != g_skipPressKey) {
            LavenderHook::UI::Functions::SetSkipCutsceneKey(g_skipPressKey);
        }

        // Detect changes
        int currVals[15] = {
            g_autoGHotkey, g_autoCtrlGHotkey, g_skipHotkey, g_autoClickHotkey,
            g_autoGPressKey, g_autoCtrlGCtrlKey, g_autoCtrlGGKey, g_skipPressKey,
            g_autoGHoldMs, g_autoGDelayMs,
            g_autoCtrlGHoldMs, g_autoCtrlGDelayMs,
            g_skipHoldMs, g_skipDelayMs, g_autoClickIntervalMs
        };

        bool changed = false;
        for (int i = 0; i < 15; ++i)
        {
            if (prevVals[i] != currVals[i])
            {
                changed = true;
                break;
            }
        }

        if (changed)
        {
            auto& cfg = Config::Store::Instance("general_buttons.ini");

            cfg.SetInt("auto_g_hotkey",          g_autoGHotkey);
            cfg.SetInt("auto_g_press_key",         g_autoGPressKey);
            cfg.SetInt("skip_cutscene_key",         g_skipPressKey);
            cfg.SetInt("auto_ctrlg_ctrl_key",       g_autoCtrlGCtrlKey);
            cfg.SetInt("auto_ctrlg_g_key",          g_autoCtrlGGKey);
            cfg.SetInt("auto_ctrlg_hotkey",         g_autoCtrlGHotkey);
            cfg.SetInt("skip_cutscene_hotkey",      g_skipHotkey);
            cfg.SetInt("auto_click_hotkey",         g_autoClickHotkey);

            // propagate press keys to runtime actions
            LavenderHook::UI::Functions::SetAutoGKey(g_autoGPressKey);
            LavenderHook::UI::Functions::SetAutoCtrlGCtrlKey(g_autoCtrlGCtrlKey);
            LavenderHook::UI::Functions::SetAutoCtrlGGKey(g_autoCtrlGGKey);
            LavenderHook::UI::Functions::SetSkipCutsceneKey(g_skipPressKey);

            cfg.SetInt("auto_g_hold_ms",            g_autoGHoldMs);
            cfg.SetInt("auto_g_delay_ms",           g_autoGDelayMs);
            cfg.SetInt("auto_ctrlg_hold_ms",        g_autoCtrlGHoldMs);
            cfg.SetInt("auto_ctrlg_delay_ms",       g_autoCtrlGDelayMs);
            cfg.SetInt("skip_hold_ms",              g_skipHoldMs);
            cfg.SetInt("skip_delay_ms",             g_skipDelayMs);
            cfg.SetInt("auto_click_interval_ms",    g_autoClickIntervalMs);

            cfg.Save();

            for (int i = 0; i < 15; ++i)
                prevVals[i] = currVals[i];
        }

        // Save stop_at_wave_5 if changed
        static int lastStopAtWave5 = -1;
        int curStopAtWave5 = LavenderHook::Globals::stop_at_wave_5 ? 1 : 0;
        if (curStopAtWave5 != lastStopAtWave5)
        {
            auto& cfg = Config::Store::Instance("general_buttons.ini");
            cfg.SetInt("stop_at_wave_5", curStopAtWave5);
            cfg.Save();
            lastStopAtWave5 = curStopAtWave5;
        }
    }

} // namespace LavenderHook::UI::Windows

// Runtime update
void LavenderHook::UI::Windows::GeneralButtonsWindow::UpdateActions()
{
    if (!g_loadedOnce)
        return;

    using namespace LavenderHook::UI::Functions;
    using LavenderHook::UI::Actions::SetActive;

    auto& autoG = g_state.Toggle("Auto G");
    auto& autoCtrlG = g_state.Toggle("Auto Ctrl+G");
    auto& skipCut = g_state.Toggle("Skip Cutscene");
    auto& autoClick = g_state.Toggle("Auto Clicker");

    // Poll runtime hotkeys
    g_rtAutoG.UpdateToggle(autoG.enabled);
    g_rtAutoCtrlG.UpdateToggle(autoCtrlG.enabled);
    g_rtSkipCut.UpdateToggle(skipCut.enabled);

    // Bind and poll auto clicker hotkey
    static LavenderHook::UI::Lavender::Hotkey g_rtAutoClick;
    g_rtAutoClick.keyVK = &g_autoClickHotkey;
    g_rtAutoClick.UpdateToggle(autoClick.enabled);

    // If stop_on_fail is enabled and the log indicates an abort, disable all toggles
    if (LavenderHook::Globals::stop_on_fail && LavenderHook::LogMonitor::LatestLineHasAbort())
    {
        autoG.enabled = false;
        autoCtrlG.enabled = false;
        skipCut.enabled = false;
        autoClick.enabled = false;

        // Update overlay
        SetActive("Ready Up", false);
        SetActive("Force Ready Up", false);
        SetActive("Skip Cutscene", false);
        SetActive("Auto Clicker", false);

        // Ensure actions are stopped by passing false to tickers
        TickButton1(false);
        TickButton3(false);
        TickButton4(false);
        TickButton5(false);

        return;
    }

    // If the final build phase has ended, disable Auto Ready and Force Ready Up for 2 seconds
    {
        static bool s_lockActive = false;
        static std::chrono::steady_clock::time_point s_lockStart{};

        const bool finalBuildEnded = LavenderHook::LogMonitor::IsFinalBuildPhaseEnded();
        if (finalBuildEnded && !s_lockActive)
        {
            s_lockActive = true;
            s_lockStart  = std::chrono::steady_clock::now();
        }
        else if (!finalBuildEnded)
        {
            s_lockActive = false;
        }

        if (s_lockActive)
        {
            const auto elapsed = std::chrono::steady_clock::now() - s_lockStart;
            if (elapsed < std::chrono::seconds(2))
            {
                autoG.enabled     = false;
                autoCtrlG.enabled = false;
            }
        }
    }

    // Stop at wave 5 interval (10-second grace, then disable)
    {
        static bool s_waiting = false;
        static std::chrono::steady_clock::time_point s_waveDetected{};
        static int s_lastTriggeredWave = 0;

        if (LavenderHook::Globals::stop_at_wave_5)
        {
            int currentWave = LavenderHook::LogMonitor::GetCurrentWave();
            bool onInterval = currentWave > 0 && currentWave % 5 == 0;

            if (onInterval && currentWave != s_lastTriggeredWave && !s_waiting)
            {
                s_lastTriggeredWave = currentWave;
                s_waveDetected = std::chrono::steady_clock::now();
                s_waiting = true;
            }

            if (!onInterval)
            {
                s_lastTriggeredWave = 0;
                s_waiting = false;
            }

            if (s_waiting)
            {
                const auto elapsed = std::chrono::steady_clock::now() - s_waveDetected;
                if (elapsed >= std::chrono::seconds(10))
                {
                    s_waiting = false;
                    autoG.enabled     = false;
                    autoCtrlG.enabled = false;
                }
            }
        }
        else
        {
            s_waiting = false;
            s_lastTriggeredWave = 0;
        }
    }

    // Apply timings
    SetAutoGTimings(g_autoGHoldMs, g_autoGDelayMs);
    SetAutoCtrlGTimings(g_autoCtrlGHoldMs, g_autoCtrlGDelayMs);
    SetSkipCutsceneTimings(g_skipHoldMs, g_skipDelayMs);

    // Drive actions
    TickButton1(autoG.enabled);
    TickButton3(autoCtrlG.enabled);
    TickButton4(skipCut.enabled);
    TickButton5(autoClick.enabled);

    // Overlay
    SetActive("Ready Up", autoG.enabled);
    SetActive("Force Ready Up", autoCtrlG.enabled);
    SetActive("Skip Cutscene", skipCut.enabled);
    SetActive("Auto Clicker", autoClick.enabled);
}
