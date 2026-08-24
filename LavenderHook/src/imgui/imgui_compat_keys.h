// imgui_compat_keys.h
#pragma once
#include "../../imgui/imgui.h" 

// ImGui 1.87+ removed GetKeyIndex(). Provide a shim that returns the enum value.
// This makes legacy code that calls io.KeysDown[ImGui::GetKeyIndex(...)] compile again.
namespace ImGui {
    inline int GetKeyIndex(ImGuiKey key) { return (int)key; }
}
