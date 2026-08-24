#pragma once

namespace LavenderHook {
    namespace UI {
        namespace Functions {

            // Public tick entrypoints
            void TickButton1(bool enabled);

            // Auto G
            void SetAutoGTimings(int holdMs, int delayMs);
            int  GetAutoGHoldMs();
            int  GetAutoGDelayMs();

            // Auto G press key
            void SetAutoGKey(int vk);
            int  GetAutoGKey();

        }
    }
}
