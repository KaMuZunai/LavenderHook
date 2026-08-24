#include "hooks.h"

namespace
{
    using QueryPerformanceCounter_t = BOOL(WINAPI*)(LARGE_INTEGER*);
    QueryPerformanceCounter_t original_QPC = nullptr;

    LARGE_INTEGER s_fakeQPC = {0};
    LARGE_INTEGER s_lastRealQPC = {0};
    bool s_initialized = false;
}

BOOL WINAPI HookedQueryPerformanceCounter(LARGE_INTEGER* lpCounter)
{
    BOOL result = original_QPC(lpCounter);
    if (!result) return FALSE;

    if (!s_initialized)
    {
        s_fakeQPC = *lpCounter;
        s_lastRealQPC = *lpCounter;
        s_initialized = true;
        return TRUE;
    }

    LONGLONG realDelta = lpCounter->QuadPart - s_lastRealQPC.QuadPart;
    s_lastRealQPC = *lpCounter;

    float speed = LavenderHook::Globals::game_speed_enabled
        ? LavenderHook::Globals::game_speed
        : 1.0f;

    s_fakeQPC.QuadPart += (LONGLONG)(realDelta * speed);
    *lpCounter = s_fakeQPC;

    return TRUE;
}

bool LavenderHook::Hooks::Timing::Hook()
{
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) return false;

    FARPROC qpc = GetProcAddress(kernel32, "QueryPerformanceCounter");
    if (!qpc) return false;

    if (MH_CreateHook(qpc, &HookedQueryPerformanceCounter,
        reinterpret_cast<void**>(&original_QPC)) != MH_OK)
        return false;

    if (MH_EnableHook(qpc) != MH_OK)
        return false;

    LavenderConsole::GetInstance().Log("TimingHook: QPC hooked.");
    return true;
}

void LavenderHook::Hooks::Timing::Unhook()
{
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) return;

    FARPROC qpc = GetProcAddress(kernel32, "QueryPerformanceCounter");
    if (!qpc) return;

    MH_DisableHook(qpc);
    MH_RemoveHook(qpc);
}
