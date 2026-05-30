#include "TravelWindow.h"
#include "../../misc/Globals.h"
#include "../../ui/components/console.h"
#include "../../memory/aobutils/ScannerUtils.h"
#include <windows.h>

namespace LavenderHook::UI::Windows {
    namespace {
        constexpr UINT kTravelMsg = WM_APP + 0x110;
        constexpr uintptr_t kTravelToOffset = 0x13c1776;
        static uintptr_t FindTravelTo(uintptr_t moduleBase, size_t moduleSize)
        {
        // AOB
        const uint8_t pat[] = {
            0x55, 0x53, 0x57, 0x41, 0x55, 0x48, 0x8D, 0x68,
            0xA1, 0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00
        };
        uint8_t* hit = LavenderHook::Memory::Utils::ScanPattern(
            reinterpret_cast<uint8_t*>(moduleBase), moduleSize, pat, sizeof(pat));
        if (hit) return reinterpret_cast<uintptr_t>(hit);

        // Fallback: hardcoded offset
        LavenderConsole::GetInstance().Log("Travel: AOB scan failed.");
        return moduleBase + kTravelToOffset;
        }
    }

    UINT TravelWindow::GetTravelMessageId()
    {
        return kTravelMsg;
    }

 	void TravelWindow::ExecuteTravel()
 	{
 		LavenderConsole::GetInstance().Log("Travel: Executing travel...");

 		uint8_t* modBase = nullptr;
 		size_t modSize = 0;
 		if (!LavenderHook::Memory::Utils::GetModuleRange(L"DDS-Win64-Shipping.exe", modBase, modSize))
 		{
 			LavenderConsole::GetInstance().Log("Travel: Failed to find game module.");
 			return;
 		}

 		uintptr_t base = reinterpret_cast<uintptr_t>(modBase);
 		uintptr_t travelAddr = FindTravelTo(base, modSize);
 		if (!travelAddr)
 		{
 			LavenderConsole::GetInstance().Log("Travel: Could not resolve TravelTo address.");
 			return;
 		}

 		using TravelToFn = void(__fastcall*)(int32_t, int32_t);
 		auto travelTo = reinterpret_cast<TravelToFn>(travelAddr);
 		LavenderConsole::GetInstance().Log("Travel: this log somehow fixes a crash.");
 		travelTo(6, 0);
 	}
}
