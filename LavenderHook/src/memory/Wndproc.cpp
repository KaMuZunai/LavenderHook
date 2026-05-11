#include "hooks.h"
#include "../windows/functions/MiscButtonActions.h"
#include "../windows/TravelWindow.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../input/VirtualGamepad.h"
#include "../ui/components/LavenderHotkey.h"
#include <Xinput.h>

LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using namespace LavenderHook::Hooks::WndProc;

static bool g_wndproc_hooked = false;

// Keep original
static WNDPROC g_saved_original = nullptr;

// XInput block hook
using XInputGetState_t = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
static XInputGetState_t oXInputGetState = nullptr;
static LPVOID g_xinput_target = nullptr;

static LPVOID FindXInputGetState()
{
    static const char* const dlls[] = {
        "xinput1_4.dll", "xinput1_3.dll", "xinput1_2.dll",
        "xinput1_1.dll", "xinput9_1_0.dll"
    };
    for (auto& dll : dlls)
    {
        HMODULE hm = GetModuleHandleA(dll);
        if (hm)
        {
            if (FARPROC fn = GetProcAddress(hm, "XInputGetState"))
                return reinterpret_cast<LPVOID>(fn);
        }
    }
    return nullptr;
}

// XInput user index
static constexpr DWORD kVigemTargetSlot = 1;

static DWORD WINAPI hkXInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState)
{
    const bool ourQuery = LavenderHook::Globals::xinput_our_query;

    // Controller slot remapping
    if (!ourQuery && LavenderHook::Input::VGamepad::Available())
    {
        DWORD vigemIdx = LavenderHook::Input::VGamepad::GetUserIndex();
        if (vigemIdx < XUSER_MAX_COUNT && vigemIdx != kVigemTargetSlot)
        {
            if (dwUserIndex == kVigemTargetSlot)
            {
                DWORD result = oXInputGetState(vigemIdx, pState);
                if (result == ERROR_SUCCESS &&
                    LavenderHook::Globals::show_menu &&
                    LavenderHook::Globals::simulate_unfocused &&
                    !LavenderHook::Input::VGamepad::AutomationActive())
                {
                    ZeroMemory(&pState->Gamepad, sizeof(pState->Gamepad));
                }
                return result;
            }
            if (dwUserIndex == vigemIdx)
            {
                ZeroMemory(pState, sizeof(XINPUT_STATE));
                return ERROR_DEVICE_NOT_CONNECTED;
            }
        }
    }

    DWORD result = oXInputGetState(dwUserIndex, pState);
    if (!ourQuery &&
        result == ERROR_SUCCESS &&
        LavenderHook::Globals::show_menu &&
        LavenderHook::Globals::simulate_unfocused &&
        !LavenderHook::Input::VGamepad::AutomationActive())
    {
        ZeroMemory(&pState->Gamepad, sizeof(pState->Gamepad));
    }
    return result;
}

bool LavenderHook::Hooks::WndProc::Hook()
{
    HWND hwnd = LavenderHook::Globals::window_handle;
    if (!hwnd) {
        LavenderConsole::GetInstance().Log("WndProc::Hook: window handle is null.");
        return false;
    }


    WNDPROC current = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd, GWLP_WNDPROC));

    if (current == HookedWndProc) {
        g_wndproc_hooked = true;
        return true;
    }

    // Save original once
    if (!g_saved_original) g_saved_original = current;
    original_wndproc = g_saved_original;

    // Install proc
    LONG_PTR prev = SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc));
    if (prev == 0 && GetLastError() != 0) {
        LavenderConsole::GetInstance().Log("WndProc::Hook: SetWindowLongPtrW failed.");
        return false;
    }

    g_wndproc_hooked = true;

    // Install XInput block hook
    if (!g_xinput_target)
        g_xinput_target = FindXInputGetState();
    if (g_xinput_target && !oXInputGetState)
    {
        MH_CreateHook(g_xinput_target, &hkXInputGetState, reinterpret_cast<LPVOID*>(&oXInputGetState));
        MH_EnableHook(g_xinput_target);
    }

    return true;
}

bool LavenderHook::Hooks::WndProc::Unhook()
{
    HWND hwnd = LavenderHook::Globals::window_handle;
    if (!g_wndproc_hooked || !hwnd) return true;

    LONG_PTR current = GetWindowLongPtrW(hwnd, GWLP_WNDPROC);
    if (reinterpret_cast<WNDPROC>(current) == HookedWndProc && g_saved_original) {
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_saved_original));
    }

    original_wndproc = nullptr;
    g_wndproc_hooked = false;

    if (g_xinput_target && oXInputGetState)
    {
        MH_DisableHook(g_xinput_target);
        oXInputGetState = nullptr;
    }

    return true;
}

static inline bool IsInputMessage(UINT msg) {
    switch (msg) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL:  case WM_MOUSEHWHEEL:
    case WM_INPUT:
        return true;

    case WM_KEYDOWN: case WM_KEYUP:
    case WM_SYSKEYDOWN: case WM_SYSKEYUP:
    case WM_CHAR: case WM_SYSCHAR:
        return true;

    default: return false;
    }
}

LRESULT CALLBACK LavenderHook::Hooks::WndProc::HookedWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // Toggle menu on Insert OR Ctrl + F1
    if (msg == WM_KEYDOWN &&
        (wparam == VK_INSERT ||
         (wparam == VK_F1 && (GetAsyncKeyState(VK_CONTROL) & 0x8000))))
    {
        LavenderHook::Globals::show_menu = !LavenderHook::Globals::show_menu;
        return 1;
    }

    if (LavenderHook::Globals::show_menu) {
        // Fix End Key scuffing the UI
        if ((msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) &&
            wparam == VK_END)
        {
            return 1;
        }

        // ESC closes the menu when no hotkey binding is active.
        if (msg == WM_KEYDOWN && wparam == VK_ESCAPE &&
            !LavenderHook::UI::Lavender::IsAnyHotkeyListening())
        {
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
            LavenderHook::Globals::show_menu = false;
            return 1;
        }

        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
            return 1;

        if (IsInputMessage(msg))
            return 1;
    }

    // Handle tray callback to restore hidden window
    if (msg == LavenderHook::Globals::tray_callback_message) {
        if (lparam == WM_LBUTTONUP || lparam == WM_RBUTTONUP) {
            LavenderHook::Input::RestoreHiddenWindowFromTray();
            return 0;
        }
    }

    // Handle travel to main menu (posted from TravelWindow)
    if (msg == LavenderHook::UI::Windows::TravelWindow::GetTravelMessageId())
    {
        LavenderHook::UI::Windows::TravelWindow::ExecuteTravel();
        return 0;
    }

    if (g_saved_original)
        return CallWindowProcW(g_saved_original, hwnd, msg, wparam, lparam);
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}
