#include "UIState.h"

namespace LavenderHook::UI {

    ToggleState& StateTable::Toggle(const std::string& name) {
        return m_toggles[name];
    }

}
