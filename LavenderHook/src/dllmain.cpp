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
#include "misc/FileLog.h"
#include "memory/hooks.h"
#include "config/ConfigManager.h"
#include "ui/UIWindows/console.h"

static void HideAndDetachConsole()
{
    static bool once = false;
    if (once) return;
    once = true;

    if (HWND h = GetConsoleWindow())
        ShowWindow(h, SW_HIDE);
    FreeConsole();
}

static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM)
{
    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);

    if (GetCurrentProcessId() != process_id)  return TRUE;
    if (!IsWindowVisible(hwnd))               return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;

    LavenderHook::Globals::window_handle = hwnd;

    char title[256] = {};
    GetWindowTextA(hwnd, title, sizeof(title));
    LavenderHook::Globals::window_title = title;

    return FALSE;
}

// Entry thread
DWORD WINAPI CheatEntry(HMODULE hModule)
{
    LavenderHook::Log::Write("CheatEntry started");

    HideAndDetachConsole();

    srand(static_cast<unsigned>(time(nullptr)));

    EnumWindows(EnumWindowsCallback, 0);

    // Use process executable name for settings path (more reliable than window title)
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string exeName = exePath;
    size_t pos = exeName.find_last_of("\\/");
    if (pos != std::string::npos)
        exeName = exeName.substr(pos + 1);
    // Remove .exe extension if present
    size_t dotPos = exeName.find_last_of(".");
    if (dotPos != std::string::npos)
        exeName = exeName.substr(0, dotPos);

    LavenderHook::Config::SetCurrentGameTitle(exeName);
    LavenderHook::Log::Write(("Settings path: " + exeName).c_str());

    if (LavenderHook::Globals::window_handle)
    {
        RECT rect{};
        GetWindowRect(LavenderHook::Globals::window_handle, &rect);
        LavenderHook::Globals::window_width = rect.right - rect.left;
        LavenderHook::Globals::window_height = rect.bottom - rect.top;
        LavenderHook::Log::Write("Window handle found");
    }
    else
    {
        LavenderHook::Log::Write("No window handle found — continuing with DX11/DX12 hooks");
    }

    LavenderHook::Log::Write("Initializing MinHook");
    if (!LavenderHook::Hooks::Initialize())
    {
        LavenderHook::Log::Write("MinHook init failed");
        goto end;
    }
    LavenderHook::Log::Write("MinHook init OK");

    if (!LavenderHook::Hooks::Hook())
    {
        LavenderHook::Log::Write("Hook() returned false");
        goto end;
    }
    LavenderHook::Log::Write("Hook() completed successfully");

end:
    while (true)
        Sleep(100);

    LavenderHook::Hooks::Unhook();
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
    {
        DisableThreadLibraryCalls(hModule);
        LavenderHook::Globals::dll_module = hModule;

        HANDLE h = CreateThread(nullptr, 0,
            (LPTHREAD_START_ROUTINE)CheatEntry,
            hModule, 0, nullptr);
        if (h) CloseHandle(h);
        break;
    }
    case DLL_PROCESS_DETACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}
