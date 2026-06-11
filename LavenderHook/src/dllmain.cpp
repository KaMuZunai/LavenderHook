#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WIN32_WINNT 0x0A00
#include <sdkddkver.h>
#include <windows.h>
#include <string>
#include <ctime>
#include <cstdlib>

#include "misc/Globals.h"
#include "memory/hooks.h"
#include "ui/components/console.h"
#include "misc/logmonitor/LogMonitor.h"

static void HideAndDetachConsole()
{
    static bool once = false;
    if (once) return;
    once = true;

    if (HWND h = GetConsoleWindow())
        ShowWindow(h, SW_HIDE);
    FreeConsole();
}

BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM)
{
    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);

    if (GetCurrentProcessId() != process_id)  return TRUE;
    if (!IsWindowVisible(hwnd))               return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;

    LavenderHook::Globals::window_handle = hwnd;
    return FALSE;
}

// Entry thread
DWORD WINAPI CheatEntry(HMODULE hModule)
{
    HideAndDetachConsole();

    srand(static_cast<unsigned>(time(nullptr)));

    // Cache the game window
    EnumWindows(EnumWindowsCallback, 0);

    if (!LavenderHook::Globals::window_handle)
    {
        LavenderConsole::GetInstance().Log("No window handle found.");
        goto end;
    }

    // Cache initial window size
    {
        RECT rect{};
        GetWindowRect(LavenderHook::Globals::window_handle, &rect);
        LavenderHook::Globals::window_width = rect.right - rect.left;
        LavenderHook::Globals::window_height = rect.bottom - rect.top;
    }

    // Install hooks
    if (!LavenderHook::Hooks::Initialize())
    {
        LavenderConsole::GetInstance().Log("Failed to initialize MinHook.");
        goto end;
    }
    if (!LavenderHook::Hooks::Hook())
    {
        LavenderConsole::GetInstance().Log("Failed to hook functions.");
        goto end;
    }

    // Start background log monitor thread
    LavenderHook::LogMonitor::Start();

end:
	while (true)  // Probably gonna remove this in the future entirely
    // while (!GetAsyncKeyState(VK_END))
        Sleep(100);

    // Unhook DX/UI
    LavenderHook::Hooks::Unhook();

    // Stop monitor
    LavenderHook::LogMonitor::Stop();

    HideAndDetachConsole();

    Sleep(200);
    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        LavenderHook::Globals::dll_module = hModule;

        HideAndDetachConsole();

        {
            HANDLE h = CreateThread(nullptr, 0,
                (LPTHREAD_START_ROUTINE)CheatEntry,
                hModule, 0, nullptr);
            if (h) CloseHandle(h);
        }
        break;

    case DLL_PROCESS_DETACH:
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}
