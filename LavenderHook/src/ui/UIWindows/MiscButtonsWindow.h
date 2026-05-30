#pragma once

namespace LavenderHook {
    namespace UI {
        namespace Windows {

            class MiscButtonsWindow {
            public:
                static void Init();
                static void Render(bool wantVisible);
                static void UpdateActions();
            };

        } // namespace Windows
    } // namespace UI
} // namespace LavenderHook
