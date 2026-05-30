#include "BuffingWindow.h"
#include "functions/BuffingActions.h"
#include "functions/FunctionRegistry.h"
#include "../../misc/logmonitor/LogMonitor.h"
#include "../../misc/Globals.h"

#include "../../ui/UIWindowBuilder.h"
#include "../../ui/ActionsOverlay.h"
#include "../../ui/UIState.h"
#include "../../config/ConfigManager.h"
#include "../../ui/components/LavenderHotkey.h"
#include "../../assets/UITextures.h"
#include "../../sound/SoundPlayer.h"
#include <chrono>

namespace LavenderHook::UI::Windows {

    namespace {
        StateTable g_bState;
        UIWindowBuilder g_bWindow("Buffing");
        bool g_bInited = false;

        // UI owned settings
        int g_autoFIntervalMs    = 8000;
        int g_autoFPressKey      = 'F';
        int g_autoFHotkey        = 0;

        int g_altAutoFIntervalMs = 8000;
        int g_altAutoFPressKey   = 0x0209; // GPVK_RB
        int g_altAutoFHotkey     = 0;

        int g_petBoostPressKey   = 'C';
        int g_petBoostCooldownMs = 8200;
        int g_petBoostMinMana    = 15;
        int g_petBoostMaxMana    = 70;
        int g_petBoostHotkey     = 0;

        int g_wrathFormPressKey   = 'C';
        int g_wrathFormCooldownMs = 8200;
        int g_wrathFormMinMana    = 15;
        int g_wrathFormMaxMana    = 70;
        int g_wrathFormHotkey     = 0;

        int g_overchargePressKey     = 'C';
        int g_overchargeSpamKey      = 'K';
        int g_overchargeSpamDelayMs  = 50;
        int g_overchargeCooldownMs   = 8200;
        int g_overchargeMinMana      = 15;
        int g_overchargeMaxMana      = 70;
        int g_overchargeHotkey       = 0;

        int g_defenseBoostPressKey   = 'F';
        int g_defenseBoostIntervalMs = 13000;
        int g_defenseBoostHotkey     = 0;

        // Runtime hotkeys
        LavenderHook::UI::Lavender::Hotkey g_rtAutoF;
        LavenderHook::UI::Lavender::Hotkey g_rtAltAutoF;
        LavenderHook::UI::Lavender::Hotkey g_rtPetBoost;
        LavenderHook::UI::Lavender::Hotkey g_rtWrathForm;
        LavenderHook::UI::Lavender::Hotkey g_rtOvercharge;
        LavenderHook::UI::Lavender::Hotkey g_rtDefenseBoost;

        bool g_bLoadedOnce = false;
    }

    static void BuffInitOnce()
    {
        if (g_bInited) return;
        g_bInited = true;

        auto& cfg = Config::Store::Instance("buffing.ini");
        cfg.Load("buffing.ini");

        auto& autoF    = g_bState.Toggle("Flash Buff");
        auto& altAutoF = g_bState.Toggle("Alt Flash Buff");
        auto& petBoost = g_bState.Toggle("Pet Boost");
        auto& wrathForm    = g_bState.Toggle("Wrath Form");
        auto& overcharge   = g_bState.Toggle("Overcharge");
        auto& defenseBoost = g_bState.Toggle("Defense Boost");

        g_autoFHotkey        = cfg.EnsureInt("auto_f_hotkey",        0);
        g_autoFPressKey      = cfg.EnsureInt("auto_f_press_key",     'F');
        g_autoFIntervalMs    = cfg.EnsureInt("auto_f_interval_ms",   8000);

        g_altAutoFHotkey     = cfg.EnsureInt("alt_auto_f_hotkey",        0);
        g_altAutoFPressKey   = cfg.EnsureInt("alt_auto_f_press_key",     0x0209);
        g_altAutoFIntervalMs = cfg.EnsureInt("alt_auto_f_interval_ms",   8000);

        g_petBoostHotkey     = cfg.EnsureInt("pet_boost_hotkey",      0);
        g_petBoostPressKey   = cfg.EnsureInt("pet_boost_press_key",   'C');
        g_petBoostCooldownMs = cfg.EnsureInt("pet_boost_cooldown_ms", 8200);
        g_petBoostMinMana    = cfg.EnsureInt("pet_boost_min_mana",    15);
        g_petBoostMaxMana    = cfg.EnsureInt("pet_boost_max_mana",    70);

        g_wrathFormHotkey     = cfg.EnsureInt("wrath_form_hotkey",      0);
        g_wrathFormPressKey   = cfg.EnsureInt("wrath_form_press_key",   'C');
        g_wrathFormCooldownMs = cfg.EnsureInt("wrath_form_cooldown_ms", 8200);
        g_wrathFormMinMana    = cfg.EnsureInt("wrath_form_min_mana",    15);
        g_wrathFormMaxMana    = cfg.EnsureInt("wrath_form_max_mana",    70);

        g_overchargeHotkey     = cfg.EnsureInt("overcharge_hotkey",       0);
        g_overchargePressKey   = cfg.EnsureInt("overcharge_press_key",    'C');
        g_overchargeSpamKey    = cfg.EnsureInt("overcharge_spam_key",     'K');
        g_overchargeSpamDelayMs = cfg.EnsureInt("overcharge_spam_delay_ms", 50);
        g_overchargeCooldownMs = cfg.EnsureInt("overcharge_cooldown_ms",  8200);
        g_overchargeMinMana    = cfg.EnsureInt("overcharge_min_mana",     15);
        g_overchargeMaxMana    = cfg.EnsureInt("overcharge_max_mana",     70);

        g_defenseBoostHotkey     = cfg.EnsureInt("defense_boost_hotkey",      0);
        g_defenseBoostPressKey   = cfg.EnsureInt("defense_boost_press_key",   'F');
        g_defenseBoostIntervalMs = cfg.EnsureInt("defense_boost_interval_ms", 13000);

        g_rtAutoF.keyVK    = &g_autoFHotkey;
        g_rtAltAutoF.keyVK = &g_altAutoFHotkey;
        g_rtPetBoost.keyVK = &g_petBoostHotkey;
        g_rtWrathForm.keyVK    = &g_wrathFormHotkey;
        g_rtOvercharge.keyVK   = &g_overchargeHotkey;
        g_rtDefenseBoost.keyVK = &g_defenseBoostHotkey;

        LavenderHook::UI::Functions::SetAutoFKey(g_autoFPressKey);
        LavenderHook::UI::Functions::SetAltAutoFKey(g_altAutoFPressKey);
        LavenderHook::UI::Functions::SetPetBoostKey(g_petBoostPressKey);
        LavenderHook::UI::Functions::SetPetBoostCooldownMs(g_petBoostCooldownMs);
        LavenderHook::UI::Functions::SetPetBoostMinMana(g_petBoostMinMana);
        LavenderHook::UI::Functions::SetPetBoostMaxMana(g_petBoostMaxMana);
        LavenderHook::UI::Functions::SetWrathFormKey(g_wrathFormPressKey);
        LavenderHook::UI::Functions::SetWrathFormCooldownMs(g_wrathFormCooldownMs);
        LavenderHook::UI::Functions::SetWrathFormMinMana(g_wrathFormMinMana);
        LavenderHook::UI::Functions::SetWrathFormMaxMana(g_wrathFormMaxMana);
        LavenderHook::UI::Functions::SetOverchargeKey(g_overchargePressKey);
        LavenderHook::UI::Functions::SetOverchargeSpamKey(g_overchargeSpamKey);
        LavenderHook::UI::Functions::SetOverchargeSpamDelayMs(g_overchargeSpamDelayMs);
        LavenderHook::UI::Functions::SetOverchargeCooldownMs(g_overchargeCooldownMs);
        LavenderHook::UI::Functions::SetOverchargeMinMana(g_overchargeMinMana);
        LavenderHook::UI::Functions::SetOverchargeMaxMana(g_overchargeMaxMana);
        LavenderHook::UI::Functions::SetDefenseBoostKey(g_defenseBoostPressKey);
        LavenderHook::UI::Functions::SetDefenseBoostIntervalMs(g_defenseBoostIntervalMs);

        cfg.Save();
        g_bLoadedOnce = true;

        // Register toggles with the global function registry
        {
            auto& reg = LavenderHook::UI::FunctionRegistry::Instance();
            reg.Register("Flash Buff",     &autoF.enabled);
            reg.Register("Alt Flash Buff", &altAutoF.enabled);
            reg.Register("Pet Boost",      &petBoost.enabled);
            reg.Register("Wrath Form",     &wrathForm.enabled);
            reg.Register("Overcharge",     &overcharge.enabled);
            reg.Register("Defense Boost",  &defenseBoost.enabled);
        }

        // Build UI
        g_bWindow
            .SetHeaderIcon(g_swordIcoTex)
            .AddToggleDropdown("Flash Buff", &autoF.enabled, &g_autoFHotkey)
            .AddItemDescription(R"(Presses the Flash Heal Button by pressing the configured button.
additional configurations:
Hotkey - Select Hotkey to toggle the button. (Esc binds to None)
Key - Key that will be pressed when button is enabled. (Default: F)
Interval - Time it takes for the action to repeat.)")
            .AddDropdownButton("Key:", &g_autoFPressKey)
            .AddDropdownTimingSeconds("Interval:", &g_autoFIntervalMs, 100, 20000)

            .AddToggleDropdown("Alt Flash Buff", &altAutoF.enabled, &g_altAutoFHotkey)
            .AddItemDescription(R"(Presses the Flash Heal Button by pressing the configured button (used for alt).
additional configurations:
Hotkey - Select Hotkey to toggle the button. (Esc binds to None)
Key - Key that will be pressed when button is enabled. (Default: Right Bumper)
Interval - Time it takes for the action to repeat.)")
            .AddDropdownButton("Key:", &g_altAutoFPressKey)
            .AddDropdownTimingSeconds("Interval:", &g_altAutoFIntervalMs, 100, 20000)

            .AddToggleDropdown("Pet Boost", &petBoost.enabled, &g_petBoostHotkey)
            .AddItemDescription(R"(Activates the pet buff by pressing the configured key.
dynamically swaps on or off based on mana thresholds and cooldown.
additional configurations:
Hotkey - Select Hotkey to toggle the button. (Esc binds to None)
Key - Key that will be pressed to activate Pet Boost. (Default: C)
CD - Time between consecutive presses.
Min - Deactivate when current mana drops below this value.
Max - Reactivate when current mana exceeds this value (after cooldown).)")   
            .AddDropdownButton("Key:", &g_petBoostPressKey)
            .AddDropdownTimingSeconds("CD:", &g_petBoostCooldownMs, 4000, 20000)
            .AddDropdownIntInput("Min:", &g_petBoostMinMana, 0, 100)
            .AddDropdownIntInput("Max:", &g_petBoostMaxMana, 0, 100)

            .AddToggleDropdown("Wrath Form", &wrathForm.enabled, &g_wrathFormHotkey)
            .AddItemDescription(R"(Activates Wrath Form by pressing the configured key.
dynamically swaps on or off based on mana thresholds and cooldown.
additional configurations:
Hotkey - Select Hotkey to toggle the button. (Esc binds to None)
Key - Key that will be pressed to activate Wrath Form. (Default: C)
CD - Time between consecutive presses.
Min - Deactivate when current mana drops below this value.
Max - Reactivate when current mana exceeds this value (after cooldown).)")   
            .AddDropdownButton("Key:", &g_wrathFormPressKey)
            .AddDropdownTimingSeconds("CD:", &g_wrathFormCooldownMs, 4000, 20000)
            .AddDropdownIntInput("Min:", &g_wrathFormMinMana, 0, 100)
            .AddDropdownIntInput("Max:", &g_wrathFormMaxMana, 0, 100)

            .AddToggleDropdown("Overcharge", &overcharge.enabled, &g_overchargeHotkey)
            .AddItemDescription(R"(Activates Overcharge by pressing the configured key.
while draining mana repeatedly spam right click to apply overcharge buff to nearby towers.
dynamically swaps on or off based on mana thresholds and cooldown.
Note: SET ALTERNATIVE RIGHT CLICK KEY OTHERWISE IT WONT OVERCHARGE!
additional configurations:
Hotkey - Select Hotkey to toggle the button. (Esc binds to None)
Key - Key pressed to toggle Overcharge. (Default: C)
OC Key - Key spammed while Overcharge is draining. (Default: K)
OC Delay - Delay between overcharge presses.
CD - Recovery cooldown before re-entering draining form.
Min - Deactivate when current mana drops below this value.
Max - Reactivate when current mana exceeds this value (after cooldown).)")   
            .AddDropdownButton("Key:", &g_overchargePressKey)
            .AddDropdownButton("OC Key:", &g_overchargeSpamKey)
            .AddDropdownTiming("OC Delay:", &g_overchargeSpamDelayMs, 50, 500)
            .AddDropdownTimingSeconds("CD:", &g_overchargeCooldownMs, 4000, 20000)
            .AddDropdownIntInput("Min:", &g_overchargeMinMana, 0, 100)
            .AddDropdownIntInput("Max:", &g_overchargeMaxMana, 0, 100)

            .AddToggleDropdown("Defense Boost", &defenseBoost.enabled, &g_defenseBoostHotkey)
            .AddItemDescription(R"(Presses the Defense Boost Button by pressing the configured button.
additional configurations:
Hotkey - Select Hotkey to toggle the button. (Esc binds to None)
Key - Key that will be pressed. (Default: F)
Interval - Time between presses.)")   
            .AddDropdownButton("Key:", &g_defenseBoostPressKey)
            .AddDropdownTimingSeconds("Interval:", &g_defenseBoostIntervalMs, 4000, 30000);
    }

    // Public
    void BuffingWindow::Init() { BuffInitOnce(); }

    void BuffingWindow::Render(bool wantVisible)
    {
        BuffInitOnce();

        // Snapshot previous values
        static int prevVals[27] = {
            g_autoFHotkey, g_autoFPressKey, g_autoFIntervalMs,
            g_altAutoFHotkey, g_altAutoFPressKey, g_altAutoFIntervalMs,
            g_petBoostHotkey, g_petBoostPressKey, g_petBoostCooldownMs, g_petBoostMinMana, g_petBoostMaxMana,
            g_wrathFormHotkey, g_wrathFormPressKey, g_wrathFormCooldownMs, g_wrathFormMinMana, g_wrathFormMaxMana,
            g_overchargeHotkey, g_overchargePressKey, g_overchargeSpamKey, g_overchargeSpamDelayMs, g_overchargeCooldownMs, g_overchargeMinMana, g_overchargeMaxMana,
            g_defenseBoostHotkey, g_defenseBoostPressKey, g_defenseBoostIntervalMs,
            0
        };

        g_bWindow.Render(wantVisible);

        // Propagate press-key changes immediately to runtime
        if (LavenderHook::UI::Functions::GetAutoFKey() != g_autoFPressKey)
            LavenderHook::UI::Functions::SetAutoFKey(g_autoFPressKey);
        if (LavenderHook::UI::Functions::GetAltAutoFKey() != g_altAutoFPressKey)
            LavenderHook::UI::Functions::SetAltAutoFKey(g_altAutoFPressKey);

        // Detect changes
        int currVals[27] = {
            g_autoFHotkey, g_autoFPressKey, g_autoFIntervalMs,
            g_altAutoFHotkey, g_altAutoFPressKey, g_altAutoFIntervalMs,
            g_petBoostHotkey, g_petBoostPressKey, g_petBoostCooldownMs, g_petBoostMinMana, g_petBoostMaxMana,
            g_wrathFormHotkey, g_wrathFormPressKey, g_wrathFormCooldownMs, g_wrathFormMinMana, g_wrathFormMaxMana,
            g_overchargeHotkey, g_overchargePressKey, g_overchargeSpamKey, g_overchargeSpamDelayMs, g_overchargeCooldownMs, g_overchargeMinMana, g_overchargeMaxMana,
            g_defenseBoostHotkey, g_defenseBoostPressKey, g_defenseBoostIntervalMs,
            0
        };

        bool changed = false;
        for (int i = 0; i < 27; ++i)
        {
            if (prevVals[i] != currVals[i]) { changed = true; break; }
        }

        if (changed)
        {
            auto& cfg = Config::Store::Instance("buffing.ini");

            cfg.SetInt("auto_f_hotkey",         g_autoFHotkey);
            cfg.SetInt("auto_f_press_key",       g_autoFPressKey);
            cfg.SetInt("auto_f_interval_ms",     g_autoFIntervalMs);
            cfg.SetInt("alt_auto_f_hotkey",      g_altAutoFHotkey);
            cfg.SetInt("alt_auto_f_press_key",   g_altAutoFPressKey);
            cfg.SetInt("alt_auto_f_interval_ms", g_altAutoFIntervalMs);
            cfg.SetInt("pet_boost_hotkey",       g_petBoostHotkey);
            cfg.SetInt("pet_boost_press_key",    g_petBoostPressKey);
            cfg.SetInt("pet_boost_cooldown_ms",  g_petBoostCooldownMs);
            cfg.SetInt("pet_boost_min_mana",     g_petBoostMinMana);
            cfg.SetInt("pet_boost_max_mana",     g_petBoostMaxMana);

            cfg.SetInt("wrath_form_hotkey",       g_wrathFormHotkey);
            cfg.SetInt("wrath_form_press_key",    g_wrathFormPressKey);
            cfg.SetInt("wrath_form_cooldown_ms",  g_wrathFormCooldownMs);
            cfg.SetInt("wrath_form_min_mana",     g_wrathFormMinMana);
            cfg.SetInt("wrath_form_max_mana",     g_wrathFormMaxMana);

            cfg.SetInt("overcharge_hotkey",        g_overchargeHotkey);
            cfg.SetInt("overcharge_press_key",      g_overchargePressKey);
            cfg.SetInt("overcharge_spam_key",       g_overchargeSpamKey);
            cfg.SetInt("overcharge_spam_delay_ms",  g_overchargeSpamDelayMs);
            cfg.SetInt("overcharge_cooldown_ms",    g_overchargeCooldownMs);
            cfg.SetInt("overcharge_min_mana",       g_overchargeMinMana);
            cfg.SetInt("overcharge_max_mana",       g_overchargeMaxMana);

            cfg.SetInt("defense_boost_hotkey",       g_defenseBoostHotkey);
            cfg.SetInt("defense_boost_press_key",    g_defenseBoostPressKey);
            cfg.SetInt("defense_boost_interval_ms",  g_defenseBoostIntervalMs);

            LavenderHook::UI::Functions::SetAutoFKey(g_autoFPressKey);
            LavenderHook::UI::Functions::SetAltAutoFKey(g_altAutoFPressKey);
            LavenderHook::UI::Functions::SetPetBoostKey(g_petBoostPressKey);
            LavenderHook::UI::Functions::SetPetBoostCooldownMs(g_petBoostCooldownMs);
            LavenderHook::UI::Functions::SetPetBoostMinMana(g_petBoostMinMana);
            LavenderHook::UI::Functions::SetPetBoostMaxMana(g_petBoostMaxMana);
            LavenderHook::UI::Functions::SetWrathFormKey(g_wrathFormPressKey);
            LavenderHook::UI::Functions::SetWrathFormCooldownMs(g_wrathFormCooldownMs);
            LavenderHook::UI::Functions::SetWrathFormMinMana(g_wrathFormMinMana);
            LavenderHook::UI::Functions::SetWrathFormMaxMana(g_wrathFormMaxMana);
            LavenderHook::UI::Functions::SetOverchargeKey(g_overchargePressKey);
            LavenderHook::UI::Functions::SetOverchargeSpamKey(g_overchargeSpamKey);
            LavenderHook::UI::Functions::SetOverchargeSpamDelayMs(g_overchargeSpamDelayMs);
            LavenderHook::UI::Functions::SetOverchargeCooldownMs(g_overchargeCooldownMs);
            LavenderHook::UI::Functions::SetOverchargeMinMana(g_overchargeMinMana);
            LavenderHook::UI::Functions::SetOverchargeMaxMana(g_overchargeMaxMana);
            LavenderHook::UI::Functions::SetDefenseBoostKey(g_defenseBoostPressKey);
            LavenderHook::UI::Functions::SetDefenseBoostIntervalMs(g_defenseBoostIntervalMs);

            cfg.Save();

            for (int i = 0; i < 27; ++i)
                prevVals[i] = currVals[i];
        }
    }

} // namespace LavenderHook::UI::Windows

// Runtime update
void LavenderHook::UI::Windows::BuffingWindow::UpdateActions()
{
    if (!g_bLoadedOnce)
        return;

    using namespace LavenderHook::UI::Functions;
    using LavenderHook::UI::Actions::SetActive;

    auto& autoF    = g_bState.Toggle("Flash Buff");
    auto& altAutoF = g_bState.Toggle("Alt Flash Buff");
    auto& petBoost = g_bState.Toggle("Pet Boost");
    auto& wrathForm    = g_bState.Toggle("Wrath Form");
    auto& overcharge   = g_bState.Toggle("Overcharge");
    auto& defenseBoost = g_bState.Toggle("Defense Boost");

    g_rtAutoF.UpdateToggle(autoF.enabled);
    g_rtAltAutoF.UpdateToggle(altAutoF.enabled);
    g_rtPetBoost.UpdateToggle(petBoost.enabled);
    g_rtWrathForm.UpdateToggle(wrathForm.enabled);
    g_rtOvercharge.UpdateToggle(overcharge.enabled);
    g_rtDefenseBoost.UpdateToggle(defenseBoost.enabled);

    // If stop_on_fail is enabled and the log indicates an abort, disable all toggles
    if (LavenderHook::Globals::stop_on_fail && LavenderHook::LogMonitor::LatestLineHasAbort())
    {
        autoF.enabled    = false;
        altAutoF.enabled = false;
        petBoost.enabled = false;
        wrathForm.enabled    = false;
        overcharge.enabled   = false;
        defenseBoost.enabled = false;

        SetActive("Flash Buff",     false);
        SetActive("Alt Flash Buff", false);
        SetActive("Pet Boost",      false);
        SetActive("Wrath Form",     false);
        SetActive("Overcharge",     false);
        SetActive("Defense Boost",  false);

        TickButton21(false);
        TickButton22(false);
        TickButton23(false);
        TickButton24(false);
        TickButton25(false);
        TickButton26(false);

        LavenderHook::Audio::PlayFailSound();
        return;
    }

    // Apply timings and settings
    SetAutoFTiming(g_autoFIntervalMs);
    SetAltAutoFTiming(g_altAutoFIntervalMs);
    SetPetBoostKey(g_petBoostPressKey);
    SetPetBoostCooldownMs(g_petBoostCooldownMs);
    SetPetBoostMinMana(g_petBoostMinMana);
    SetPetBoostMaxMana(g_petBoostMaxMana);
    SetWrathFormKey(g_wrathFormPressKey);
    SetWrathFormCooldownMs(g_wrathFormCooldownMs);
    SetWrathFormMinMana(g_wrathFormMinMana);
    SetWrathFormMaxMana(g_wrathFormMaxMana);
    SetOverchargeKey(g_overchargePressKey);
    SetOverchargeSpamKey(g_overchargeSpamKey);
    SetOverchargeSpamDelayMs(g_overchargeSpamDelayMs);
    SetOverchargeCooldownMs(g_overchargeCooldownMs);
    SetOverchargeMinMana(g_overchargeMinMana);
    SetOverchargeMaxMana(g_overchargeMaxMana);
    SetDefenseBoostKey(g_defenseBoostPressKey);
    SetDefenseBoostIntervalMs(g_defenseBoostIntervalMs);

    // Drive actions
    TickButton21(autoF.enabled);
    TickButton22(altAutoF.enabled);
    TickButton23(petBoost.enabled);
    TickButton24(wrathForm.enabled);
    TickButton25(overcharge.enabled);
    TickButton26(defenseBoost.enabled);

    // Overlay
    SetActive("Flash Buff",     autoF.enabled);
    SetActive("Alt Flash Buff", altAutoF.enabled);
    SetActive("Pet Boost",      petBoost.enabled);
    SetActive("Wrath Form",     wrathForm.enabled);
    SetActive("Overcharge",     overcharge.enabled);
    SetActive("Defense Boost",  defenseBoost.enabled);
}
