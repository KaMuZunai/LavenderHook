#pragma once
#include "../../imgui/imgui.h"
#include <string>
#include <unordered_map>

namespace LavenderHook::UI::Lavender {

    struct Hotkey {
        const char* label;	// display name
        int* keyVK;			// binds directly to item storage

        bool listening = false;
        bool suppress_once = false;
        bool ignore_until_up = false;
        int ignore_vk = 0;
        bool just_bound = false;

        int pending_first_vk = 0;
        bool waiting_for_combo = false;
        bool saw_other_key_while_held = false;

        std::unordered_map<int, bool> keyEdge;

        bool Render(const ImVec2& size = ImVec2(70, 0));

        void UpdateToggle(bool& toggleState);
    };

    const char* VkToString(int vk);	// exposed for displaying names
    bool IsAnyHotkeyListening();	// true while any hotkey button is capturing input
    extern bool g_hotkey_binding_active;

}
