#pragma once

namespace LavenderHook {
    namespace UI {
        namespace Functions {

            // Public tick entrypoints
            void TickButton1(bool enabled);
            void TickButton3(bool enabled);
            void TickButton4(bool enabled);
            void TickButton5(bool enabled);

            // Auto G
            void SetAutoGTimings(int holdMs, int delayMs);
            int  GetAutoGHoldMs();
            int  GetAutoGDelayMs();

            // Auto G press key
            void SetAutoGKey(int vk);
            int  GetAutoGKey();

            // Auto Ctrl+G press keys
            void SetAutoCtrlGCtrlKey(int vk);
            int  GetAutoCtrlGCtrlKey();
            void SetAutoCtrlGGKey(int vk);
            int  GetAutoCtrlGGKey();

            // Auto Ctrl + G
            void SetAutoCtrlGTimings(int holdMs, int delayMs);
            int  GetAutoCtrlGHoldMs();
            int  GetAutoCtrlGDelayMs();

            // Skip Cutscene
            void SetSkipCutsceneTimings(int holdMs, int delayMs);
            int  GetSkipCutsceneHoldMs();
            int  GetSkipCutsceneDelayMs();
            void SetSkipCutsceneKey(int vk);
            int  GetSkipCutsceneKey();

            // Auto Clicker
            void SetAutoClickInterval(int intervalMs);
            int  GetAutoClickIntervalMs();

        } // namespace Functions
    } // namespace UI
} // namespace LavenderHook
