#pragma once
#include "../../imgui/imgui.h" 

namespace ImGui {
    inline void PushAllowKeyboardFocus(bool allow) {
        ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, !allow);
    }
    inline void PopAllowKeyboardFocus() {
        ImGui::PopItemFlag();
    }
}

namespace ImGui {
    inline int GetKeyIndex(ImGuiKey key) { return (int)key; }
}

namespace ImGui {
    inline bool IsKeyDown(int key) { return ImGui::IsKeyDown((ImGuiKey)key); }
    inline bool IsKeyPressed(int key, bool repeat = false)
    {
        return ImGui::IsKeyPressed((ImGuiKey)key, repeat);
    }
    inline bool IsKeyReleased(int key) { return ImGui::IsKeyReleased((ImGuiKey)key); }
}
