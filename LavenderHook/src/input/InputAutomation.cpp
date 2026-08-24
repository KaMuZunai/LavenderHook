#include "InputAutomation.h"
#include "VkTable.h"
#include "../misc/Globals.h"

namespace LavenderHook::Input {

    constexpr bool kRequireForeground = false;

    bool AutomationAllowed()
    {
        if (LavenderHook::Globals::show_menu)
            return false;

        if (kRequireForeground &&
            GetForegroundWindow() != LavenderHook::Globals::window_handle)
            return false;

        return true;
    }

    static WORD GamepadToKeyboard(int gpvk)
    {
        using namespace LavenderHook::UI::Lavender;
        switch (gpvk) {
            case GPVK_DPAD_UP:    return VK_UP;
            case GPVK_DPAD_DOWN:  return VK_DOWN;
            case GPVK_DPAD_LEFT:  return VK_LEFT;
            case GPVK_DPAD_RIGHT: return VK_RIGHT;
            case GPVK_START:      return VK_RETURN;
            case GPVK_BACK:       return VK_ESCAPE;
            case GPVK_LS:         return 'L';
            case GPVK_RS:         return 'R';
            case GPVK_LB:         return VK_LSHIFT;
            case GPVK_RB:         return VK_RSHIFT;
            case GPVK_A:          return VK_SPACE;
            case GPVK_B:          return VK_ESCAPE;
            case GPVK_X:          return 'X';
            case GPVK_Y:          return 'Y';
            case GPVK_LT:         return 'Z';
            case GPVK_RT:         return 'C';
            default: return 0;
        }
    }

    static void PostKey(HWND hwnd, UINT msg, WORD vk)
    {
        UINT sc = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC);
        LPARAM lParam = 1 | (sc << 16);
        if (vk == VK_RCONTROL || vk == VK_RMENU || vk == VK_INSERT || vk == VK_DELETE ||
            vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT ||
            vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN)
        {
            lParam |= (1 << 24);
        }
        PostMessage(hwnd, msg, (WPARAM)vk, lParam);
    }

    void PressVK(WORD vk)
    {
        HWND hwnd = LavenderHook::Globals::window_handle;
        if (!hwnd) return;

        if (LavenderHook::UI::Lavender::IsGamepadVk((int)vk)) {
            WORD mapped = GamepadToKeyboard((int)vk);
            if (mapped) {
                PostKey(hwnd, WM_KEYDOWN, mapped);
                PostKey(hwnd, WM_KEYUP, mapped);
            }
            return;
        }

        PostMessage(hwnd, WM_KEYDOWN, vk, 1);
        PostMessage(hwnd, WM_KEYUP, vk, (1 << 30) | (1 << 31));
    }

    void PressVKNoInit(WORD vk)
    {
        HWND hwnd = LavenderHook::Globals::window_handle;
        if (!hwnd) return;

        if (LavenderHook::UI::Lavender::IsGamepadVk((int)vk)) {
            WORD mapped = GamepadToKeyboard((int)vk);
            if (mapped) {
                PostKey(hwnd, WM_KEYDOWN, mapped);
                PostKey(hwnd, WM_KEYUP, mapped);
            }
            return;
        }

        PostMessage(hwnd, WM_KEYDOWN, vk, 1);
        PostMessage(hwnd, WM_KEYUP, vk, (1 << 30) | (1 << 31));
    }

    void PressDownVK(WORD vk)
    {
        HWND hwnd = LavenderHook::Globals::window_handle;
        if (!hwnd) return;

        if (LavenderHook::UI::Lavender::IsGamepadVk((int)vk)) {
            WORD mapped = GamepadToKeyboard((int)vk);
            if (mapped) PostKey(hwnd, WM_KEYDOWN, mapped);
            return;
        }

        PostMessage(hwnd, WM_KEYDOWN, vk, 1);
    }

    void PressUpVK(WORD vk)
    {
        HWND hwnd = LavenderHook::Globals::window_handle;
        if (!hwnd) return;

        if (LavenderHook::UI::Lavender::IsGamepadVk((int)vk)) {
            WORD mapped = GamepadToKeyboard((int)vk);
            if (mapped) PostKey(hwnd, WM_KEYUP, mapped);
            return;
        }

        PostMessage(hwnd, WM_KEYUP, vk, (1 << 30) | (1 << 31));
    }

    void HoldVK(bool enabled, WORD vk, HoldState& state)
    {
        if (!AutomationAllowed())
            enabled = false;

        if (enabled && !state.isDown) {
            PressDownVK(vk);
            state.isDown = true;
        }
        else if (!enabled && state.isDown) {
            PressUpVK(vk);
            state.isDown = false;
        }
    }

} // namespace LavenderHook::Input
