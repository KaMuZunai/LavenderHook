#pragma once

namespace LavenderHook::UI::Windows {
    class MacroManagerWindow {
    public:
        static void Init();
        static void Render(bool wantVisible);
        static void UpdateActions();
        static void SwapRowStates(int a, int b);
    };
}
