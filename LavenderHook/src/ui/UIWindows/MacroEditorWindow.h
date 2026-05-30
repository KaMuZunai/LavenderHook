#pragma once

namespace LavenderHook::UI::Windows {
    class MacroEditorWindow {
    public:
        static void Init();
        static void Render(bool wantVisible);
        static void UpdateActions();
    };
}
