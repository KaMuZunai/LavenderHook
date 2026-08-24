#include "hooks.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../ui/components/LavenderHotkey.h"

LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using namespace LavenderHook::Hooks::WndProc;

static bool g_wndproc_hooked = false;
static WNDPROC g_saved_original = nullptr;

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

    if (!g_saved_original) g_saved_original = current;
    original_wndproc = g_saved_original;

    LONG_PTR prev = SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc));
    if (prev == 0 && GetLastError() != 0) {
        LavenderConsole::GetInstance().Log("WndProc::Hook: SetWindowLongPtrW failed.");
        return false;
    }

    g_wndproc_hooked = true;
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

        // ESC closes the menu when no hotkey binding is active (ignore key repeats).
        if (msg == WM_KEYDOWN && wparam == VK_ESCAPE &&
            !(lparam & 0x40000000) &&
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

    if (g_saved_original)
        return CallWindowProcW(g_saved_original, hwnd, msg, wparam, lparam);
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}