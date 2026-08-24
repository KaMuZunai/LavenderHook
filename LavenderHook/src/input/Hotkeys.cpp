#include "Hotkeys.h"
#include "../misc/Globals.h"
#include <windows.h>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cctype>

namespace {
    struct KeyState {
        bool wasDown = false;
    };
    std::unordered_map<int, KeyState> g_states;

    inline std::string up(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return (char)std::toupper(c); });
        return s;
    }

    int VKFromName(const std::string& name) {
        const std::string k = up(name);
        if (k.size() == 1) {
            char c = k[0];
            if (c >= 'A' && c <= 'Z') return 'A' + (c - 'A');
            if (c >= '0' && c <= '9') return '0' + (c - '0');
        }
        if (k.size() >= 2 && k[0] == 'F') {
            int n = std::atoi(k.c_str() + 1);
            if (n >= 1 && n <= 24) return VK_F1 + (n - 1);
        }
        if (k == "CTRL" || k == "CONTROL") return VK_CONTROL;
        if (k == "LCTRL") return VK_LCONTROL;
        if (k == "RCTRL") return VK_RCONTROL;
        if (k == "SHIFT") return VK_SHIFT;
        if (k == "LSHIFT") return VK_LSHIFT;
        if (k == "RSHIFT") return VK_RSHIFT;
        if (k == "ALT" || k == "MENU") return VK_MENU;
        if (k == "LALT") return VK_LMENU;
        if (k == "RALT") return VK_RMENU;
        if (k == "TAB") return VK_TAB;
        if (k == "SPACE" || k == "SPACEBAR") return VK_SPACE;
        if (k == "ESC" || k == "ESCAPE") return VK_ESCAPE;
        if (k == "ENTER" || k == "RETURN") return VK_RETURN;
        if (k == "BACKSPACE" || k == "BKSP") return VK_BACK;
        if (k == "INSERT" || k == "INS") return VK_INSERT;
        if (k == "DELETE" || k == "DEL") return VK_DELETE;
        if (k == "HOME") return VK_HOME;
        if (k == "END") return VK_END;
        if (k == "PAGEUP" || k == "PGUP") return VK_PRIOR;
        if (k == "PAGEDOWN" || k == "PGDN") return VK_NEXT;
        if (k == "LEFT") return VK_LEFT;
        if (k == "RIGHT") return VK_RIGHT;
        if (k == "UP") return VK_UP;
        if (k == "DOWN") return VK_DOWN;
        return 0;
    }

    inline bool IsGameForeground() {
        return GetForegroundWindow() == LavenderHook::Globals::window_handle;
    }

    inline bool DownNow(int vk) {
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    }
}

namespace LavenderHook {
    namespace Input {

        void Update() {
        }

        bool IsPressed(const std::string& name, bool requireForeground) {
            int vk = VKFromName(name);
            if (vk == 0) return false;
            if (requireForeground && !IsGameForeground()) return false;

            KeyState& ks = g_states[vk];
            bool down = DownNow(vk);
            bool pressed = (down && !ks.wasDown);
            ks.wasDown = down;
            return pressed;
        }

        bool IsDown(const std::string& name, bool requireForeground) {
            int vk = VKFromName(name);
            if (vk == 0) return false;
            if (requireForeground && !IsGameForeground()) return false;

            KeyState& ks = g_states[vk];
            bool down = DownNow(vk);
            ks.wasDown = down;
            return down;
        }

        void Clear() {
            g_states.clear();
        }

    }
} // namespace
