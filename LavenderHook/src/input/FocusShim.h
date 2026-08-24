#pragma once
#include <windows.h>

namespace LavenderHook::Input::FocusShim {
    bool Install();
    void Remove();
    void PerFrameUpdate();
}
