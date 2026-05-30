#pragma once
#include <windows.h>

namespace LavenderHook {
    namespace UI {
        void CreateProcessOverlay(HWND gameHwnd);
        void DestroyProcessOverlay();
    } // namespace UI
} // namespace LavenderHook
