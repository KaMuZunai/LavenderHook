#pragma once
#include <windows.h>

namespace LavenderHook::UI::Windows {
    class TravelWindow {
    public:
        static void ExecuteTravel();
        static UINT GetTravelMessageId();
    };
}
