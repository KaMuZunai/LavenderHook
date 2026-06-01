#include "GeneralButtonActions.h"
#include "../../../input/InputAutomation.h"
#include "../../../misc/Globals.h"

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

            using LavenderHook::Input::HoldVK;
            using LavenderHook::Input::HoldState;
            using LavenderHook::Input::PressVK;
            using LavenderHook::Input::PressVKNoInit;
            using LavenderHook::Input::PressUpVK;
            using LavenderHook::Input::AutomationAllowed;
            using LavenderHook::Input::TickEvery;

            // Auto G
            static int g_autoG_hold_ms = 2000;
            static int g_autoG_delay_ms = 3000;

            // Auto G press key
            static int g_autoG_key_vk = 'G';

            // Auto Ctrl+G press keys
            static int g_autoCtrlg_ctrl_vk = VK_CONTROL;
            static int g_autoCtrlg_g_vk = 'G';

            // Auto Ctrl+G
            static int g_ctrlG_hold_ms = 5000;
            static int g_ctrlG_delay_ms = 10000;

            // Skip Cutscene
            static int g_skip_hold_ms = 2000;
            static int g_skip_delay_ms = 4000;
            static int g_skip_key_vk = VK_SPACE;

            // Auto Clicker
            static int g_autoClick_interval_ms = 100;

            static bool g_loaded = false;

            static void LoadOnce()
            {
                if (g_loaded)
                    return;
                g_loaded = true;

                auto& cfg = LavenderHook::Config::Store::Instance("general_buttons.ini");
                cfg.Load("general_buttons.ini");

                g_autoG_hold_ms = cfg.GetInt("auto_g_hold_ms", 2000);
                g_autoG_delay_ms = cfg.GetInt("auto_g_delay_ms", 3000);
                g_autoG_key_vk = cfg.GetInt("auto_g_press_key", g_autoG_key_vk);
                g_autoCtrlg_ctrl_vk = cfg.GetInt("auto_ctrlg_ctrl_key", g_autoCtrlg_ctrl_vk);
                g_autoCtrlg_g_vk = cfg.GetInt("auto_ctrlg_g_key", g_autoCtrlg_g_vk);

                g_ctrlG_hold_ms = cfg.GetInt("auto_ctrlg_hold_ms", 5000);
                g_ctrlG_delay_ms = cfg.GetInt("auto_ctrlg_delay_ms", 10000);

                g_skip_hold_ms = cfg.GetInt("skip_hold_ms", 2000);
                g_skip_delay_ms = cfg.GetInt("skip_delay_ms", 4000);
                g_skip_key_vk = cfg.GetInt("skip_cutscene_key", VK_SPACE);

                g_autoClick_interval_ms = cfg.GetInt("auto_click_interval_ms", 100);
            }

            static void Save()
            {
                auto& cfg = LavenderHook::Config::Store::Instance("general_buttons.ini");

                cfg.SetInt("auto_g_hold_ms", g_autoG_hold_ms);
                cfg.SetInt("auto_g_delay_ms", g_autoG_delay_ms);
                cfg.SetInt("auto_g_press_key", g_autoG_key_vk);
                cfg.SetInt("auto_ctrlg_ctrl_key", g_autoCtrlg_ctrl_vk);
                cfg.SetInt("auto_ctrlg_g_key", g_autoCtrlg_g_vk);

                cfg.SetInt("auto_ctrlg_hold_ms", g_ctrlG_hold_ms);
                cfg.SetInt("auto_ctrlg_delay_ms", g_ctrlG_delay_ms);

                cfg.SetInt("skip_hold_ms", g_skip_hold_ms);
                cfg.SetInt("skip_delay_ms", g_skip_delay_ms);
                cfg.SetInt("skip_cutscene_key", g_skip_key_vk);

                cfg.SetInt("auto_click_interval_ms", g_autoClick_interval_ms);

                cfg.Save();
            }

            // Auto G
            void SetAutoGTimings(int holdMs, int delayMs)
            {
                LoadOnce();
                g_autoG_hold_ms = holdMs;
                g_autoG_delay_ms = delayMs;
                Save();
            }

            int GetAutoGHoldMs() { LoadOnce(); return g_autoG_hold_ms; }
            int GetAutoGDelayMs() { LoadOnce(); return g_autoG_delay_ms; }

            void SetAutoGKey(int vk) { LoadOnce(); g_autoG_key_vk = vk; Save(); }
            int GetAutoGKey() { LoadOnce(); return g_autoG_key_vk; }

            void SetAutoCtrlGCtrlKey(int vk) { LoadOnce(); g_autoCtrlg_ctrl_vk = vk; Save(); }
            int GetAutoCtrlGCtrlKey() { LoadOnce(); return g_autoCtrlg_ctrl_vk; }
            void SetAutoCtrlGGKey(int vk) { LoadOnce(); g_autoCtrlg_g_vk = vk; Save(); }
            int GetAutoCtrlGGKey() { LoadOnce(); return g_autoCtrlg_g_vk; }

            // Auto Ctrl+G
            void SetAutoCtrlGTimings(int holdMs, int delayMs)
            {
                LoadOnce();
                g_ctrlG_hold_ms = holdMs;
                g_ctrlG_delay_ms = delayMs;
                Save();
            }

            int GetAutoCtrlGHoldMs() { LoadOnce(); return g_ctrlG_hold_ms; }
            int GetAutoCtrlGDelayMs() { LoadOnce(); return g_ctrlG_delay_ms; }

            // Skip Cutscene
            void SetSkipCutsceneTimings(int holdMs, int delayMs)
            {
                LoadOnce();
                g_skip_hold_ms = holdMs;
                g_skip_delay_ms = delayMs;
                Save();
            }

            int GetSkipCutsceneHoldMs() { LoadOnce(); return g_skip_hold_ms; }
            int GetSkipCutsceneDelayMs() { LoadOnce(); return g_skip_delay_ms; }

            void SetSkipCutsceneKey(int vk) { LoadOnce(); g_skip_key_vk = vk; Save(); }
            int GetSkipCutsceneKey() { LoadOnce(); return g_skip_key_vk; }

            void SetAutoClickInterval(int intervalMs)
            {
                LoadOnce();
                if (intervalMs < 50) intervalMs = 50;
                if (intervalMs > 200) intervalMs = 200;
                g_autoClick_interval_ms = intervalMs;
                Save();
            }

            int GetAutoClickIntervalMs() { LoadOnce(); return g_autoClick_interval_ms; }

            static void TickAutoG(bool enabled)
            {
                LoadOnce();

                static HoldState hold;
                static int hold_vk = 0;
                static steady_clock::time_point phaseStart{};
                static bool holding = false;

                if (!enabled) {
                    if (hold_vk != 0) {
                        HoldVK(false, hold_vk, hold);
                        PressUpVK(hold_vk);
                        hold_vk = 0;
                    }
                    holding = false;
                    phaseStart = {};
                    return;
                }

                const auto now = steady_clock::now();

                    if (phaseStart.time_since_epoch().count() == 0) {
                    phaseStart = now;
                    holding = true;
                        if (hold_vk != 0 && hold_vk != g_autoG_key_vk) {
                            HoldVK(false, hold_vk, hold);
                            PressUpVK(hold_vk);
                        }
                        hold_vk = g_autoG_key_vk;
                        HoldVK(true, hold_vk, hold);
                    return;
                }

                if (holding) {
                    if ((now - phaseStart) >= milliseconds(g_autoG_hold_ms)) {
                        HoldVK(false, hold_vk != 0 ? hold_vk : g_autoG_key_vk, hold);
                        PressUpVK(hold_vk != 0 ? hold_vk : g_autoG_key_vk);
                        hold_vk = 0;
                        holding = false;
                        phaseStart = now;
                    }
                }
                else {
                    if ((now - phaseStart) >= milliseconds(g_autoG_delay_ms)) {
                        holding = true;
                        phaseStart = now;
                        if (hold_vk != 0 && hold_vk != g_autoG_key_vk) {
                            HoldVK(false, hold_vk, hold);
                            PressUpVK(hold_vk);
                        }
                        hold_vk = g_autoG_key_vk;
                        HoldVK(true, hold_vk, hold);
                    }
                }
            }

            static void TickAutoCtrlG(bool enabled)
            {
                LoadOnce();

                static HoldState holdCtrl;
                static HoldState holdG;
                static steady_clock::time_point phaseStart{};
                static bool holding = false;

                if (!enabled) {
                    HoldVK(false, g_autoCtrlg_g_vk, holdG);
                    HoldVK(false, g_autoCtrlg_ctrl_vk, holdCtrl);
                    holding = false;
                    phaseStart = {};
                    return;
                }

                const auto now = steady_clock::now();

                if (!holding) {
                    if (phaseStart.time_since_epoch().count() == 0 ||
                        (now - phaseStart) >= milliseconds(g_ctrlG_delay_ms))
                    {
                        phaseStart = now;
                        holding = true;
                        HoldVK(true, g_autoCtrlg_ctrl_vk, holdCtrl);
                        HoldVK(true, g_autoCtrlg_g_vk, holdG);
                    }
                }
                else {
                    if ((now - phaseStart) >= milliseconds(g_ctrlG_hold_ms)) {
                        HoldVK(false, g_autoCtrlg_g_vk, holdG);
                        HoldVK(false, g_autoCtrlg_ctrl_vk, holdCtrl);
                        holding = false;
                        phaseStart = now;
                    }
                }
            }

            static void TickSkipCutscene(bool enabled)
            {
                LoadOnce();

                static HoldState hold;
                static steady_clock::time_point phaseStart{};
                static bool holding = false;

                if (!enabled) {
                    HoldVK(false, g_skip_key_vk, hold);
                    holding = false;
                    phaseStart = {};
                    return;
                }

                const auto now = steady_clock::now();

                if (!holding) {
                    if (phaseStart.time_since_epoch().count() == 0 ||
                        (now - phaseStart) >= milliseconds(g_skip_delay_ms))
                    {
                        phaseStart = now;
                        holding = true;
                        HoldVK(true, g_skip_key_vk, hold);
                    }
                }
                else {
                    if ((now - phaseStart) >= milliseconds(g_skip_hold_ms)) {
                        HoldVK(false, g_skip_key_vk, hold);
                        holding = false;
                        phaseStart = now;
                    }
                }
            }

            void TickButton1(bool enabled) { TickAutoG(enabled); }
            void TickButton3(bool enabled) { TickAutoCtrlG(enabled); }
            void TickButton4(bool enabled) { TickSkipCutscene(enabled); }

            static void TickAutoClick(bool enabled)
            {
                LoadOnce();

                static std::chrono::steady_clock::time_point last;
                TickEvery(enabled, last, std::chrono::milliseconds(g_autoClick_interval_ms), []() {
                    HWND hwnd = LavenderHook::Globals::window_handle;
                    if (!hwnd) return;

                    POINT pt;
                    if (!GetCursorPos(&pt)) return;
                    if (!ScreenToClient(hwnd, &pt)) return;
                    LPARAM lParam = MAKELPARAM(pt.x, pt.y);

                    PostMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lParam);
                    PostMessage(hwnd, WM_LBUTTONUP,   0,          lParam);
                });
            }

            void TickButton5(bool enabled) { TickAutoClick(enabled); }

        }
    }
}
