#pragma once

namespace LavenderHook::UI::Windows {

    class GeneralButtonsWindow {
    public:
        static void Init();
        static void Render(bool wantVisible);
        static void UpdateActions();
    };

}
