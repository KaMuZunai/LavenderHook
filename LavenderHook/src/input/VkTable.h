#pragma once

#include <Windows.h>
#include <Xinput.h>
#include <string>
#include "../misc/Globals.h"
#include "../imgui/imgui.h"

namespace LavenderHook::UI::Lavender {

    static constexpr int GPVK_BASE        = 0x0200;
    static constexpr int GPVK_DPAD_UP     = 0x0200;
    static constexpr int GPVK_DPAD_DOWN   = 0x0201;
    static constexpr int GPVK_DPAD_LEFT   = 0x0202;
    static constexpr int GPVK_DPAD_RIGHT  = 0x0203;
    static constexpr int GPVK_START       = 0x0204;
    static constexpr int GPVK_BACK        = 0x0205;
    static constexpr int GPVK_LS          = 0x0206;
    static constexpr int GPVK_RS          = 0x0207;
    static constexpr int GPVK_LB          = 0x0208;
    static constexpr int GPVK_RB          = 0x0209;
    static constexpr int GPVK_A           = 0x020A;
    static constexpr int GPVK_B           = 0x020B;
    static constexpr int GPVK_X           = 0x020C;
    static constexpr int GPVK_Y           = 0x020D;
    static constexpr int GPVK_LT          = 0x020E;
    static constexpr int GPVK_RT          = 0x020F;
    static constexpr int GPVK_END         = 0x0210;

    struct KeyEntry { int vk; const char* name; };

    static const KeyEntry VK_TABLE[] = {
        {VK_F1,"F1"},{VK_F2,"F2"},{VK_F3,"F3"},{VK_F4,"F4"},
        {VK_F5,"F5"},{VK_F6,"F6"},{VK_F7,"F7"},{VK_F8,"F8"},
        {VK_F9,"F9"},{VK_F10,"F10"},{VK_F11,"F11"},{VK_F12,"F12"},
        {'0',"0"},{'1',"1"},{'2',"2"},{'3',"3"},{'4',"4"},
        {'5',"5"},{'6',"6"},{'7',"7"},{'8',"8"},{'9',"9"},
        {'A',"A"},{'B',"B"},{'C',"C"},{'D',"D"},{'E',"E"},
        {'F',"F"},{'G',"G"},{'H',"H"},{'I',"I"},{'J',"J"},
        {'K',"K"},{'L',"L"},{'M',"M"},{'N',"N"},{'O',"O"},
        {'P',"P"},{'Q',"Q"},{'R',"R"},{'S',"S"},{'T',"T"},
        {'U',"U"},{'V',"V"},{'W',"W"},{'X',"X"},{'Y',"Y"},{'Z',"Z"},
        {VK_DELETE,"DEL"},{VK_HOME,"HOME"},{VK_END,"END"},
        {VK_PRIOR,"PGUP"},{VK_NEXT,"PGDN"},
        {VK_LEFT,"LEFT"},{VK_RIGHT,"RIGHT"},
        {VK_UP,"UP"},{VK_DOWN,"DOWN"},
        {VK_TAB,"TAB"},{VK_SPACE,"SPACE"},
        {VK_ESCAPE,"ESC"},{VK_RETURN,"ENTER"},
        {VK_BACK,"BKSP"},
        {VK_LSHIFT,"LSHIFT"},{VK_RSHIFT,"RSHIFT"},
        {VK_CONTROL,"CTRL"},{VK_LCONTROL,"LCTRL"},{VK_RCONTROL,"RCTRL"},
        {VK_LMENU,"LALT"},{VK_RMENU,"RALT"},
        {VK_NUMPAD0,"NUM0"},{VK_NUMPAD1,"NUM1"},{VK_NUMPAD2,"NUM2"},
        {VK_NUMPAD3,"NUM3"},{VK_NUMPAD4,"NUM4"},{VK_NUMPAD5,"NUM5"},
        {VK_NUMPAD6,"NUM6"},{VK_NUMPAD7,"NUM7"},{VK_NUMPAD8,"NUM8"},
        {VK_NUMPAD9,"NUM9"},
        {VK_MBUTTON,"MOUSE3"},
        {VK_XBUTTON1,"MOUSE4"},
        {VK_XBUTTON2,"MOUSE5"},

        // Gamepad buttons
        {GPVK_DPAD_UP,    "GP:DPAD_UP"},
        {GPVK_DPAD_DOWN,  "GP:DPAD_DN"},
        {GPVK_DPAD_LEFT,  "GP:DPAD_LT"},
        {GPVK_DPAD_RIGHT, "GP:DPAD_RT"},
        {GPVK_START,      "GP:START"},
        {GPVK_BACK,       "GP:BACK"},
        {GPVK_LS,         "GP:LS"},
        {GPVK_RS,         "GP:RS"},
        {GPVK_LB,         "GP:LB"},
        {GPVK_RB,         "GP:RB"},
        {GPVK_A,          "GP:A"},
        {GPVK_B,          "GP:B"},
        {GPVK_X,          "GP:X"},
        {GPVK_Y,          "GP:Y"},
        {GPVK_LT,         "GP:LT"},
        {GPVK_RT,         "GP:RT"},
    };

    inline bool IsGamepadVk(int vk)
    {
        return vk >= GPVK_BASE && vk < GPVK_END;
    }

    // Returns true if the gamepad VK is pressed on any connected XInput controller.
    inline bool GetGamepadVkDown(int vk)
    {
        static XINPUT_STATE s_state[XUSER_MAX_COUNT];
        static bool s_connected[XUSER_MAX_COUNT];
        static int s_lastFrame = -1;

        int frame = ImGui::GetFrameCount();
        if (frame != s_lastFrame)
        {
            s_lastFrame = frame;
            for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
            {
                DWORD result = XInputGetState(i, &s_state[i]);
                s_connected[i] = (result == ERROR_SUCCESS);
            }
        }

        for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
        {
            if (!s_connected[i]) continue;
            const XINPUT_GAMEPAD& gp = s_state[i].Gamepad;
            switch (vk)
            {
                case GPVK_DPAD_UP:    if (gp.wButtons & XINPUT_GAMEPAD_DPAD_UP)            return true; break;
                case GPVK_DPAD_DOWN:  if (gp.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)          return true; break;
                case GPVK_DPAD_LEFT:  if (gp.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)          return true; break;
                case GPVK_DPAD_RIGHT: if (gp.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)         return true; break;
                case GPVK_START:      if (gp.wButtons & XINPUT_GAMEPAD_START)               return true; break;
                case GPVK_BACK:       if (gp.wButtons & XINPUT_GAMEPAD_BACK)                return true; break;
                case GPVK_LS:         if (gp.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)          return true; break;
                case GPVK_RS:         if (gp.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)         return true; break;
                case GPVK_LB:         if (gp.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)       return true; break;
                case GPVK_RB:         if (gp.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)      return true; break;
                case GPVK_A:          if (gp.wButtons & XINPUT_GAMEPAD_A)                   return true; break;
                case GPVK_B:          if (gp.wButtons & XINPUT_GAMEPAD_B)                   return true; break;
                case GPVK_X:          if (gp.wButtons & XINPUT_GAMEPAD_X)                   return true; break;
                case GPVK_Y:          if (gp.wButtons & XINPUT_GAMEPAD_Y)                   return true; break;
                case GPVK_LT:         if (gp.bLeftTrigger  > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) return true; break;
                case GPVK_RT:         if (gp.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) return true; break;
            }
        }
        return false;
    }

    // Down state query
    inline bool IsVkDown(int vk)
    {
        if (IsGamepadVk(vk)) return GetGamepadVkDown(vk);
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    inline bool IsVkPressed(int vk)
    {
        if (IsGamepadVk(vk)) return false;
        return (GetAsyncKeyState(vk) & 0x0001) != 0;
    }

    inline bool IsBindableVk(int vk)
    {
        // L+R Mouse, Insert and CTRL + F1 not allowed as Hotkeys
        if (vk == VK_LBUTTON || vk == VK_RBUTTON) return false;
        if (vk == VK_INSERT) return false;
        if ((vk & 0xFFFF0000) != 0) {
            int first = vk & 0xFFFF;
            int second = (vk >> 16) & 0xFFFF;
            auto isCtrl = [](int k) { return k == VK_CONTROL || k == VK_LCONTROL || k == VK_RCONTROL; };
            if ((first == VK_F1 && isCtrl(second)) || (second == VK_F1 && isCtrl(first)))
                return false;
        }
        return true;
    }

    inline bool IsModifierVk(int vk)
    {
        return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
               vk == VK_SHIFT   || vk == VK_LSHIFT   || vk == VK_RSHIFT ||
               vk == VK_MENU    || vk == VK_LMENU    || vk == VK_RMENU ||
               vk == VK_LWIN    || vk == VK_RWIN;
    }

    inline bool IsAnyModifierDown(int excludeVk = 0)
    {
        auto isDown = [&](int vk) {
            return vk != excludeVk && IsVkDown(vk);
        };

        return isDown(VK_CONTROL) || isDown(VK_LCONTROL) || isDown(VK_RCONTROL) ||
               isDown(VK_SHIFT)   || isDown(VK_LSHIFT)   || isDown(VK_RSHIFT)   ||
               isDown(VK_MENU)    || isDown(VK_LMENU)    || isDown(VK_RMENU)    ||
               isDown(VK_LWIN)    || isDown(VK_RWIN);
    }

    inline bool IsAnyNonModifierKeyDown(int excludeVk = 0)
    {
        for (int code = 1; code < 256; ++code)
        {
            if (code == excludeVk) continue;
            if (IsModifierVk(code)) continue;
            if (IsVkDown(code)) return true;
        }
        for (int code = GPVK_BASE; code < GPVK_END; ++code)
        {
            if (code == excludeVk) continue;
            if (IsVkDown(code)) return true;
        }
        return false;
    }

    inline const char* VkToString(int vk)
    {
        // Combo Hotkeys
        if ((vk & 0xFFFF0000) != 0) {
            int first = vk & 0xFFFF;
            int second = (vk >> 16) & 0xFFFF;
            static thread_local std::string combo;
            combo.clear();
            const char* f = VkToString(first);
            const char* s = VkToString(second);
            combo = std::string(f) + "+" + std::string(s);
            return combo.c_str();
        }

        if (!vk) return "None";
        for (auto& e : VK_TABLE)
            if (e.vk == vk) return e.name;

        // UTF-8 UI
        static thread_local std::string s;
        s.clear();

        wchar_t wbuf[64] = {0};
        UINT sc = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC);
        if (sc != 0)
        {
            LONG lParam = (sc << 16);
            if (vk == VK_RCONTROL || vk == VK_RMENU || vk == VK_INSERT || vk == VK_DELETE ||
                vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT ||
                vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN)
            {
                lParam |= (1 << 24);
            }

            if (GetKeyNameTextW(lParam, wbuf, (int)std::size(wbuf)) > 0)
            {
                int needed = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0, NULL, NULL);
                if (needed > 0)
                {
                    s.resize(needed - 1);
                    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, &s[0], needed, NULL, NULL);
                    return s.c_str();
                }
            }
        }

        return "VK?";
    }
}