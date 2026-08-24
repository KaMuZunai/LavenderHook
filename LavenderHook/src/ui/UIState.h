#pragma once
#include <string>
#include <unordered_map>

namespace LavenderHook::UI {

    struct ToggleState {
        bool enabled = false;
        int hotkeyVK = 0;
    };

    class StateTable {
    public:
        ToggleState& Toggle(const std::string& name);

    private:
        std::unordered_map<std::string, ToggleState> m_toggles;
    };

}
