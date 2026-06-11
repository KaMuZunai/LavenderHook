#include "BuffingActions.h"
#include "../../../input/InputAutomation.h"
#include "../../../misc/Globals.h"
#include "../../../memory/Hooks.h"

#include <chrono>
#include <windows.h>

namespace LavenderHook {
    namespace Config {
        class Store {
        public:
            static Store& Instance();
            static Store& Instance(const std::string& file);
            int  GetInt(const std::string& key, int def = 0) const;
            void SetInt(const std::string& key, int value);
            void Load(const std::string& file);
            void Save();
        };
    }
}

namespace LavenderHook {
    namespace UI {
        namespace Functions {

            using namespace std::chrono;

            using LavenderHook::Input::PressVK;
            using LavenderHook::Input::PressVKNoInit;
            using LavenderHook::Input::AutomationAllowed;
            using LavenderHook::Input::TickEvery;

            // Auto F
            static int g_autoF_interval_ms = 8000;
            static int g_autoF_key_vk      = 'F';

            // Alt Flash Buff
            static int g_altAutoF_interval_ms = 8000;
            static int g_altAutoF_key_vk      = 0x0209; // GPVK_RB

            // Pet Boost
            static int g_petBoost_key_vk      = 'C';
            static int g_petBoost_cooldown_ms  = 8200;
            static int g_petBoost_min_mana     = 15;
            static int g_petBoost_max_mana     = 70;

            // Wrath Form
            static int g_wrathForm_key_vk      = 'C';
            static int g_wrathForm_cooldown_ms  = 8200;
            static int g_wrathForm_min_mana     = 15;
            static int g_wrathForm_max_mana     = 70;

            // Overcharge
            static int g_overcharge_key_vk       = 'C';
            static int g_overcharge_spam_vk      = 'K';
            static int g_overcharge_cooldown_ms  = 8200;
            static int g_overcharge_min_mana     = 15;
            static int g_overcharge_max_mana     = 70;
            static int g_overcharge_spam_delay_ms = 50;

            // Defense Boost
            static int g_defenseBoost_key_vk      = 'F';
            static int g_defenseBoost_interval_ms = 13000;

            static bool g_buff_loaded = false;

            static void BuffLoadOnce()
            {
                if (g_buff_loaded) return;
                g_buff_loaded = true;

                auto& cfg = LavenderHook::Config::Store::Instance("buffing.ini");
                cfg.Load("buffing.ini");

                g_autoF_interval_ms   = cfg.GetInt("auto_f_interval_ms",   8000);
                g_autoF_key_vk        = cfg.GetInt("auto_f_press_key",      'F');
                g_altAutoF_interval_ms= cfg.GetInt("alt_auto_f_interval_ms",8000);
                g_altAutoF_key_vk     = cfg.GetInt("alt_auto_f_press_key",  g_altAutoF_key_vk);
                g_petBoost_key_vk     = cfg.GetInt("pet_boost_press_key",   'C');
                g_petBoost_cooldown_ms= cfg.GetInt("pet_boost_cooldown_ms", 8200);
                g_petBoost_min_mana   = cfg.GetInt("pet_boost_min_mana",    15);
                g_petBoost_max_mana   = cfg.GetInt("pet_boost_max_mana",    70);

                g_wrathForm_key_vk      = cfg.GetInt("wrath_form_press_key",    'C');
                g_wrathForm_cooldown_ms = cfg.GetInt("wrath_form_cooldown_ms",  8200);
                g_wrathForm_min_mana    = cfg.GetInt("wrath_form_min_mana",     15);
                g_wrathForm_max_mana    = cfg.GetInt("wrath_form_max_mana",     70);

                g_overcharge_key_vk       = cfg.GetInt("overcharge_press_key",     'C');
                g_overcharge_spam_vk      = cfg.GetInt("overcharge_spam_key",      'K');
                g_overcharge_cooldown_ms  = cfg.GetInt("overcharge_cooldown_ms",   8200);
                g_overcharge_min_mana     = cfg.GetInt("overcharge_min_mana",      15);
                g_overcharge_max_mana     = cfg.GetInt("overcharge_max_mana",      70);
                g_overcharge_spam_delay_ms = cfg.GetInt("overcharge_spam_delay_ms", 50);

                g_defenseBoost_key_vk      = cfg.GetInt("defense_boost_press_key",    'F');
                g_defenseBoost_interval_ms = cfg.GetInt("defense_boost_interval_ms",  13000);
            }

            static void BuffSave()
            {
                auto& cfg = LavenderHook::Config::Store::Instance("buffing.ini");

                cfg.SetInt("auto_f_interval_ms",    g_autoF_interval_ms);
                cfg.SetInt("auto_f_press_key",       g_autoF_key_vk);
                cfg.SetInt("alt_auto_f_interval_ms", g_altAutoF_interval_ms);
                cfg.SetInt("alt_auto_f_press_key",   g_altAutoF_key_vk);
                cfg.SetInt("pet_boost_press_key",    g_petBoost_key_vk);
                cfg.SetInt("pet_boost_cooldown_ms",  g_petBoost_cooldown_ms);
                cfg.SetInt("pet_boost_min_mana",     g_petBoost_min_mana);
                cfg.SetInt("pet_boost_max_mana",     g_petBoost_max_mana);

                cfg.SetInt("wrath_form_press_key",    g_wrathForm_key_vk);
                cfg.SetInt("wrath_form_cooldown_ms",  g_wrathForm_cooldown_ms);
                cfg.SetInt("wrath_form_min_mana",     g_wrathForm_min_mana);
                cfg.SetInt("wrath_form_max_mana",     g_wrathForm_max_mana);

                cfg.SetInt("overcharge_press_key",      g_overcharge_key_vk);
                cfg.SetInt("overcharge_spam_key",       g_overcharge_spam_vk);
                cfg.SetInt("overcharge_cooldown_ms",    g_overcharge_cooldown_ms);
                cfg.SetInt("overcharge_min_mana",       g_overcharge_min_mana);
                cfg.SetInt("overcharge_max_mana",       g_overcharge_max_mana);
                cfg.SetInt("overcharge_spam_delay_ms",  g_overcharge_spam_delay_ms);

                cfg.SetInt("defense_boost_press_key",    g_defenseBoost_key_vk);
                cfg.SetInt("defense_boost_interval_ms",  g_defenseBoost_interval_ms);

                cfg.Save();
            }

            // Auto F
            void SetAutoFTiming(int intervalMs) { BuffLoadOnce(); g_autoF_interval_ms   = intervalMs; BuffSave(); }
            int  GetAutoFIntervalMs()           { BuffLoadOnce(); return g_autoF_interval_ms; }
            void SetAutoFKey(int vk)            { BuffLoadOnce(); g_autoF_key_vk         = vk;         BuffSave(); }
            int  GetAutoFKey()                  { BuffLoadOnce(); return g_autoF_key_vk; }

            // Alt Flash Buff
            void SetAltAutoFTiming(int intervalMs) { BuffLoadOnce(); g_altAutoF_interval_ms = intervalMs; BuffSave(); }
            int  GetAltAutoFIntervalMs()            { BuffLoadOnce(); return g_altAutoF_interval_ms; }
            void SetAltAutoFKey(int vk)             { BuffLoadOnce(); g_altAutoF_key_vk      = vk;         BuffSave(); }
            int  GetAltAutoFKey()                   { BuffLoadOnce(); return g_altAutoF_key_vk; }

            // Pet Boost
            void SetPetBoostKey(int vk)        { BuffLoadOnce(); g_petBoost_key_vk      = vk;  BuffSave(); }
            int  GetPetBoostKey()              { BuffLoadOnce(); return g_petBoost_key_vk; }
            void SetPetBoostCooldownMs(int ms) { BuffLoadOnce(); g_petBoost_cooldown_ms  = ms;  BuffSave(); }
            int  GetPetBoostCooldownMs()       { BuffLoadOnce(); return g_petBoost_cooldown_ms; }
            void SetPetBoostMinMana(int v)     { BuffLoadOnce(); g_petBoost_min_mana     = v;   BuffSave(); }
            int  GetPetBoostMinMana()          { BuffLoadOnce(); return g_petBoost_min_mana; }
            void SetPetBoostMaxMana(int v)     { BuffLoadOnce(); g_petBoost_max_mana     = v;   BuffSave(); }
            int  GetPetBoostMaxMana()          { BuffLoadOnce(); return g_petBoost_max_mana; }

            static void TickAutoF(bool enabled)
            {
                BuffLoadOnce();
                static steady_clock::time_point last;
                TickEvery(enabled, last, milliseconds(g_autoF_interval_ms), []() {
                    PressVK(g_autoF_key_vk);
                });
            }

            static void TickAltAutoF(bool enabled)
            {
                BuffLoadOnce();
                static steady_clock::time_point last;
                TickEvery(enabled, last, milliseconds(g_altAutoF_interval_ms), []() {
                    PressVKNoInit(g_altAutoF_key_vk);
                });
            }

            static void TickPetBoost(bool enabled)
            {
                BuffLoadOnce();

                // State machine
                enum class State { IDLE, JUST_ENABLED, ACTIVE, COOLING };
                static State state = State::IDLE;
                static steady_clock::time_point cooldownStart{};
                static steady_clock::time_point stateStart{};

                if (!enabled) {
                    if (state == State::ACTIVE && AutomationAllowed())
                        PressVK(g_petBoost_key_vk);
                    state = State::IDLE;
                    cooldownStart = {};
                    stateStart = {};
                    return;
                }

                if (!AutomationAllowed()) return;

                const auto  now  = steady_clock::now();
                const float mana = LavenderHook::Globals::mana_current.load(std::memory_order_relaxed);

                switch (state)
                {
                case State::IDLE:
                    state = State::JUST_ENABLED;
                    break;

                case State::JUST_ENABLED:
                    PressVK(g_petBoost_key_vk);
                    state = State::ACTIVE;
                    stateStart = now;
                    break;

                case State::ACTIVE:
                    if (mana < static_cast<float>(g_petBoost_min_mana)) {
                        PressVK(g_petBoost_key_vk);
                        state = State::COOLING;
                        cooldownStart = now;
                        stateStart = now;
                    } else if ((now - stateStart) >= seconds(240)) {
                        // Guard: ability may not have activated — deactivate and cool down
                        PressVK(g_petBoost_key_vk);
                        state = State::COOLING;
                        cooldownStart = now;
                        stateStart = now;
                    }
                    break;

                case State::COOLING:
                    if ((now - cooldownStart) >= milliseconds(g_petBoost_cooldown_ms) &&
                        mana >= static_cast<float>(g_petBoost_max_mana))
                    {
                        PressVK(g_petBoost_key_vk);
                        state = State::ACTIVE;
                        stateStart = now;
                    } else if ((now - stateStart) >= seconds(30)) {
                        // Guard: mana not recovering — retry activation
                        PressVK(g_petBoost_key_vk);
                        state = State::ACTIVE;
                        stateStart = now;
                    }
                    break;
                }
            }

            void TickButton21(bool enabled) { TickAutoF(enabled);    }
            void TickButton22(bool enabled) { TickAltAutoF(enabled); }
            void TickButton23(bool enabled) { TickPetBoost(enabled); }

            // Wrath Form
            static void TickWrathForm(bool enabled)
            {
                BuffLoadOnce();

                enum class State { IDLE, JUST_ENABLED, ACTIVE, COOLING };
                static State state = State::IDLE;
                static steady_clock::time_point cooldownStart{};
                static steady_clock::time_point stateStart{};

                if (!enabled) {
                    if (state == State::ACTIVE && AutomationAllowed())
                        PressVK(g_wrathForm_key_vk);
                    state = State::IDLE;
                    cooldownStart = {};
                    stateStart = {};
                    return;
                }

                if (!AutomationAllowed()) return;

                const auto  now  = steady_clock::now();
                const float mana = LavenderHook::Globals::mana_current.load(std::memory_order_relaxed);

                switch (state)
                {
                case State::IDLE:
                    state = State::JUST_ENABLED;
                    break;
                case State::JUST_ENABLED:
                    PressVK(g_wrathForm_key_vk);
                    state = State::ACTIVE;
                    stateStart = now;
                    break;
                case State::ACTIVE:
                    if (mana < static_cast<float>(g_wrathForm_min_mana)) {
                        PressVK(g_wrathForm_key_vk);
                        state = State::COOLING;
                        cooldownStart = now;
                        stateStart = now;
                    } else if ((now - stateStart) >= seconds(240)) {
                        // Guard: ability may not have activated — deactivate and cool down
                        PressVK(g_wrathForm_key_vk);
                        state = State::COOLING;
                        cooldownStart = now;
                        stateStart = now;
                    }
                    break;
                case State::COOLING:
                    if ((now - cooldownStart) >= milliseconds(g_wrathForm_cooldown_ms) &&
                        mana >= static_cast<float>(g_wrathForm_max_mana))
                    {
                        PressVK(g_wrathForm_key_vk);
                        state = State::ACTIVE;
                        stateStart = now;
                    } else if ((now - stateStart) >= seconds(30)) {
                        // Guard: mana not recovering — retry activation
                        PressVK(g_wrathForm_key_vk);
                        state = State::ACTIVE;
                        stateStart = now;
                    }
                    break;
                }
            }

            // Wrath Form setters/getters
            void SetWrathFormKey(int vk)        { BuffLoadOnce(); g_wrathForm_key_vk      = vk;  BuffSave(); }
            int  GetWrathFormKey()              { BuffLoadOnce(); return g_wrathForm_key_vk; }
            void SetWrathFormCooldownMs(int ms) { BuffLoadOnce(); g_wrathForm_cooldown_ms  = ms;  BuffSave(); }
            int  GetWrathFormCooldownMs()       { BuffLoadOnce(); return g_wrathForm_cooldown_ms; }
            void SetWrathFormMinMana(int v)     { BuffLoadOnce(); g_wrathForm_min_mana     = v;   BuffSave(); }
            int  GetWrathFormMinMana()          { BuffLoadOnce(); return g_wrathForm_min_mana; }
            void SetWrathFormMaxMana(int v)     { BuffLoadOnce(); g_wrathForm_max_mana     = v;   BuffSave(); }
            int  GetWrathFormMaxMana()          { BuffLoadOnce(); return g_wrathForm_max_mana; }

            // Overcharge
            static void TickOvercharge(bool enabled)
            {
                BuffLoadOnce();

                enum class State { IDLE, JUST_ENABLED, DRAINING, COOLING };
                static State state = State::IDLE;
                static steady_clock::time_point cooldownStart{};
                static steady_clock::time_point lastSpam{};
                static steady_clock::time_point stateStart{};

                if (!enabled) {
                    if (state == State::DRAINING && AutomationAllowed())
                        PressVK(g_overcharge_key_vk);
                    state = State::IDLE;
                    cooldownStart = {};
                    lastSpam = {};
                    stateStart = {};
                    return;
                }

                if (!AutomationAllowed()) return;

                const auto  now  = steady_clock::now();
                const float mana = LavenderHook::Globals::mana_current.load(std::memory_order_relaxed);

                switch (state)
                {
                case State::IDLE:
                    state = State::JUST_ENABLED;
                    break;

                case State::JUST_ENABLED:
                    PressVK(g_overcharge_key_vk);
                    state = State::DRAINING;
                    stateStart = now;
                    break;

                case State::DRAINING:
                    // Spam configured key while draining
                    if ((now - lastSpam) >= milliseconds(g_overcharge_spam_delay_ms)) {
                        lastSpam = now;
                        PressVK(g_overcharge_spam_vk);
                    }
                    if (mana < static_cast<float>(g_overcharge_min_mana)) {
                        PressVK(g_overcharge_key_vk);
                        state = State::COOLING;
                        cooldownStart = now;
                        stateStart = now;
                    } else if ((now - stateStart) >= seconds(240)) {
                        // Guard: mana not draining — deactivate and cool down
                        PressVK(g_overcharge_key_vk);
                        state = State::COOLING;
                        cooldownStart = now;
                        stateStart = now;
                    }
                    break;

                case State::COOLING:
                    if ((now - cooldownStart) >= milliseconds(g_overcharge_cooldown_ms) &&
                        mana >= static_cast<float>(g_overcharge_max_mana))
                    {
                        PressVK(g_overcharge_key_vk);
                        state = State::DRAINING;
                        stateStart = now;
                    } else if ((now - stateStart) >= seconds(30)) {
                        // Guard: mana not recovering — retry activation
                        PressVK(g_overcharge_key_vk);
                        state = State::DRAINING;
                        stateStart = now;
                    }
                    break;
                }
            }

            // Overcharge setters/getters
            void SetOverchargeKey(int vk)        { BuffLoadOnce(); g_overcharge_key_vk      = vk;  BuffSave(); }
            int  GetOverchargeKey()              { BuffLoadOnce(); return g_overcharge_key_vk; }
            void SetOverchargeSpamKey(int vk)    { BuffLoadOnce(); g_overcharge_spam_vk      = vk;  BuffSave(); }
            int  GetOverchargeSpamKey()          { BuffLoadOnce(); return g_overcharge_spam_vk; }
            void SetOverchargeCooldownMs(int ms) { BuffLoadOnce(); g_overcharge_cooldown_ms   = ms;  BuffSave(); }
            int  GetOverchargeCooldownMs()       { BuffLoadOnce(); return g_overcharge_cooldown_ms; }
            void SetOverchargeMinMana(int v)     { BuffLoadOnce(); g_overcharge_min_mana      = v;   BuffSave(); }
            int  GetOverchargeMinMana()          { BuffLoadOnce(); return g_overcharge_min_mana; }
            void SetOverchargeMaxMana(int v)        { BuffLoadOnce(); g_overcharge_max_mana       = v;   BuffSave(); }
            int  GetOverchargeMaxMana()               { BuffLoadOnce(); return g_overcharge_max_mana; }
            void SetOverchargeSpamDelayMs(int ms)     { BuffLoadOnce(); g_overcharge_spam_delay_ms  = ms;  BuffSave(); }
            int  GetOverchargeSpamDelayMs()           { BuffLoadOnce(); return g_overcharge_spam_delay_ms; }

            // Defense Boost
            static void TickDefenseBoost(bool enabled)
            {
                BuffLoadOnce();
                static steady_clock::time_point last;
                TickEvery(enabled, last, milliseconds(g_defenseBoost_interval_ms), []() {
                    PressVK(g_defenseBoost_key_vk);
                });
            }

            // Defense Boost setters/getters
            void SetDefenseBoostKey(int vk)        { BuffLoadOnce(); g_defenseBoost_key_vk      = vk;  BuffSave(); }
            int  GetDefenseBoostKey()              { BuffLoadOnce(); return g_defenseBoost_key_vk; }
            void SetDefenseBoostIntervalMs(int ms) { BuffLoadOnce(); g_defenseBoost_interval_ms  = ms;  BuffSave(); }
            int  GetDefenseBoostIntervalMs()       { BuffLoadOnce(); return g_defenseBoost_interval_ms; }

            void TickButton24(bool enabled) { TickWrathForm(enabled);    }
            void TickButton25(bool enabled) { TickOvercharge(enabled);   }
            void TickButton26(bool enabled) { TickDefenseBoost(enabled); }

        } // namespace Functions
    } // namespace UI
} // namespace LavenderHook
