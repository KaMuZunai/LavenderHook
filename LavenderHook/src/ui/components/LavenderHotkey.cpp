#include "lavenderhotkey.h"
#include "lavenderui.h"
#include "../../sound/SoundPlayer.h"
#include "../../input/VkTable.h"
#include <unordered_map>

namespace LavenderHook::UI::Lavender {

    static std::unordered_map<int, bool> bindEdge;
    static Hotkey* g_listening_hotkey = nullptr;
    static std::unordered_map<int, bool> s_suppressToggleUntilUp;
    static DWORD g_last_bind_end_time = 0;
    static constexpr DWORD kBindCooldownMs = 200;
    bool g_hotkey_binding_active = false;

    bool Hotkey::Render(const ImVec2& size)
    {
        const char* base = listening ? "..." : VkToString(*keyVK);

        char label[128];
        snprintf(label, sizeof(label), "%s\xC2\xA0##HK_%p", base, this);

        if (Lavender::HotkeyButton(label, size))
        {
            if (g_listening_hotkey == nullptr) {
                listening = true;
                // Reset edge state so key does not instantly trigger button after listening process.
                bindEdge.clear();
                keyEdge.clear();
                pending_first_vk = 0;
                waiting_for_combo = false;
                g_listening_hotkey = this;
                g_hotkey_binding_active = true;
            }
            else if (g_listening_hotkey != this) {
                // Cancel previous listener and start listening on this one.
                g_listening_hotkey->listening = false;
                listening = true;
                bindEdge.clear();
                keyEdge.clear();
                pending_first_vk = 0;
                waiting_for_combo = false;
                g_listening_hotkey = this;
                g_hotkey_binding_active = true;
            }
            return true;
        }

        if (!listening)
            return false;

        // Only listen to foreground inputs.
        HWND fg = GetForegroundWindow();
        DWORD fgPid = 0;
        if (fg)
            GetWindowThreadProcessId(fg, &fgPid);

        if (fgPid != GetCurrentProcessId())
        {
            listening = false;
            g_last_bind_end_time = GetTickCount(); g_hotkey_binding_active = false;
            if (g_listening_hotkey == this) g_listening_hotkey = nullptr;
            return false;
        }

        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        {
            *keyVK = 0;
            listening = false;
            ignore_until_up = false;
            g_last_bind_end_time = GetTickCount(); g_hotkey_binding_active = false;
            if (g_listening_hotkey == this) g_listening_hotkey = nullptr;
            return true;
        }

        // Scan all possible virtual-key codes so any key can be bound.
        for (int code = 1; code < 256; ++code)
        {
            if (!IsBindableVk(code))
                continue;
            if (code == VK_ESCAPE)
                continue;

            bool down = (GetAsyncKeyState(code) & 0x8000) != 0;
            bool prev = bindEdge[code];

            // Key pressed edge
            if (down && !prev)
            {
                int assigned = code;

                if (code == VK_CONTROL && (GetAsyncKeyState(VK_RMENU) & 0x8000)) {
                    bindEdge[code] = down;
                    continue;
                }

                // Prefer specific side variants.
                if (code == VK_CONTROL) {
                    if ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0)
                        assigned = VK_RCONTROL;
                    else if ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0)
                        assigned = VK_LCONTROL;
                }
                else if (code == VK_MENU) {
                    if ((GetAsyncKeyState(VK_RMENU) & 0x8000) != 0)
                        assigned = VK_RMENU;
                    else if ((GetAsyncKeyState(VK_LMENU) & 0x8000) != 0)
                        assigned = VK_LMENU;
                }
                else if (code == VK_SHIFT) {
                    if ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0)
                        assigned = VK_RSHIFT;
                    else if ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0)
                        assigned = VK_LSHIFT;
                }

                // Instantly Bind Combo otherwise bind key on release.
                if (waiting_for_combo && pending_first_vk != 0 && assigned != pending_first_vk) {
                    int combo = (assigned << 16) | (pending_first_vk & 0xFFFF);

                    // Disallow Ctrl+F1 combo
                    auto isCtrlKey = [](int k) { return k == VK_CONTROL || k == VK_LCONTROL || k == VK_RCONTROL; };
                    int first = pending_first_vk;
                    int second = assigned;
                    if ((first == VK_F1 && isCtrlKey(second)) || (second == VK_F1 && isCtrlKey(first))) {
                        pending_first_vk = 0;
                        waiting_for_combo = false;
                        bindEdge[code] = down;
                        if (g_listening_hotkey == this) g_listening_hotkey = this;
                        continue;
                    }

                    *keyVK = combo;
                    listening = false;

                    ignore_vk = assigned;
                    ignore_until_up = true;
                    just_bound = true;

                    keyEdge[assigned] = true;
                    s_suppressToggleUntilUp[assigned] = true;

                    // clear pending state
                    pending_first_vk = 0;
                    waiting_for_combo = false;

                    // unstuck keys
                    bindEdge.clear();
                    g_last_bind_end_time = GetTickCount(); g_hotkey_binding_active = false;
                    if (g_listening_hotkey == this) g_listening_hotkey = nullptr;
                    return true;
                }

                if (!waiting_for_combo) {
                    pending_first_vk = assigned;
                    waiting_for_combo = true;

                    keyEdge[assigned] = true;
                    s_suppressToggleUntilUp[assigned] = true;

                    bindEdge[code] = down;
                    continue;
                }
            }

            // Key released edge
            if (!down && prev)
            {
                if (waiting_for_combo && pending_first_vk == code) {
                    *keyVK = pending_first_vk;
                    listening = false;

                    ignore_vk = pending_first_vk;
                    ignore_until_up = true;
                    just_bound = true;

                    keyEdge[pending_first_vk] = true;
                    s_suppressToggleUntilUp[pending_first_vk] = true;

                    pending_first_vk = 0;
                    waiting_for_combo = false;

                    bindEdge.clear();
                    g_last_bind_end_time = GetTickCount(); g_hotkey_binding_active = false;
                    if (g_listening_hotkey == this) g_listening_hotkey = nullptr;
                    bindEdge[code] = down;
                    return true;
                }

                if (!waiting_for_combo) {
                    int assigned = code;
                    *keyVK = assigned;
                    listening = false;

                    ignore_vk = assigned;
                    ignore_until_up = true;
                    just_bound = true;

                    keyEdge[assigned] = true;
                    s_suppressToggleUntilUp[assigned] = true;

                    // unstuck keys
                    bindEdge.clear();
                    g_last_bind_end_time = GetTickCount(); g_hotkey_binding_active = false;
                    if (g_listening_hotkey == this) g_listening_hotkey = nullptr;
                    bindEdge[code] = down;
                    return true;
                }

                bindEdge[code] = down;
                continue;
            }

            bindEdge[code] = down;
        }

        // Scan gamepad buttons
        for (int code = GPVK_BASE; code < GPVK_END; ++code)
        {
            bool down = GetGamepadVkDown(code);
            bool prev = bindEdge[code];

            // Button pressed edge
            if (down && !prev)
            {
                int assigned = code;

                if (waiting_for_combo && pending_first_vk != 0 && assigned != pending_first_vk)
                {
                    int combo = (assigned << 16) | (pending_first_vk & 0xFFFF);
                    *keyVK = combo;
                    listening = false;

                    ignore_vk = assigned;
                    ignore_until_up = true;
                    just_bound = true;

                    keyEdge[assigned] = true;
                    s_suppressToggleUntilUp[assigned] = true;

                    pending_first_vk = 0;
                    waiting_for_combo = false;

                    bindEdge.clear();
                    g_last_bind_end_time = GetTickCount(); g_hotkey_binding_active = false;
                    if (g_listening_hotkey == this) g_listening_hotkey = nullptr;
                    return true;
                }

                if (!waiting_for_combo)
                {
                    pending_first_vk = assigned;
                    waiting_for_combo = true;

                    keyEdge[assigned] = true;
                    s_suppressToggleUntilUp[assigned] = true;

                    bindEdge[code] = down;
                    continue;
                }
            }

            // Button released edge
            if (!down && prev)
            {
                if (waiting_for_combo && pending_first_vk == code)
                {
                    *keyVK = pending_first_vk;
                    listening = false;

                    ignore_vk = pending_first_vk;
                    ignore_until_up = true;
                    just_bound = true;

                    keyEdge[pending_first_vk] = true;
                    s_suppressToggleUntilUp[pending_first_vk] = true;

                    pending_first_vk = 0;
                    waiting_for_combo = false;

                    bindEdge.clear();
                    g_last_bind_end_time = GetTickCount(); g_hotkey_binding_active = false;
                    if (g_listening_hotkey == this) g_listening_hotkey = nullptr;
                    bindEdge[code] = down;
                    return true;
                }

                if (!waiting_for_combo)
                {
                    *keyVK = code;
                    listening = false;

                    ignore_vk = code;
                    ignore_until_up = true;
                    just_bound = true;

                    keyEdge[code] = true;
                    s_suppressToggleUntilUp[code] = true;

                    bindEdge.clear();
                    g_last_bind_end_time = GetTickCount(); g_hotkey_binding_active = false;
                    if (g_listening_hotkey == this) g_listening_hotkey = nullptr;
                    bindEdge[code] = down;
                    return true;
                }

                bindEdge[code] = down;
                continue;
            }

            bindEdge[code] = down;
        }

        return false;
    }

    void Hotkey::UpdateToggle(bool& toggleState)
    {
        if (!keyVK || *keyVK == 0 || listening)
            return;

        // Prevent triggering the reserved Ctrl+F1 menu toggle even if somehow bound.
        {
            const int rawchk = *keyVK;
            const int chk_first = rawchk & 0xFFFF;
            const int chk_second = (rawchk >> 16) & 0xFFFF;
            auto isCtrlKey = [](int k) { return k == VK_CONTROL || k == VK_LCONTROL || k == VK_RCONTROL; };
            if ((chk_first == VK_F1 && isCtrlKey(chk_second)) || (chk_second == VK_F1 && isCtrlKey(chk_first)))
                return;
        }

        if (g_listening_hotkey != nullptr && g_listening_hotkey != this)
            return;

        HWND fg = GetForegroundWindow();
        DWORD fgPid = 0;
        if (fg)
            GetWindowThreadProcessId(fg, &fgPid);

        if (fgPid != GetCurrentProcessId())
            return;

        if (just_bound)
        {
            just_bound = false;
            return;
        }

        if (ignore_until_up)
        {
            if (!IsVkDown(ignore_vk))
            {
                ignore_until_up = false;
                keyEdge[ignore_vk] = false;
            }
            return;
        }

        const int raw = *keyVK;
        const int first = raw & 0xFFFF;
        const int second = (raw >> 16) & 0xFFFF;

        if (second != 0) {
            // Combo binding
            const bool down1 = IsVkDown(first);
            const bool down2 = IsVkDown(second);
            const bool pressed2 = IsVkPressed(second);
            const bool prev1 = keyEdge[first];
            const bool prev2 = keyEdge[second];

            keyEdge[first] = down1;
            keyEdge[second] = down2;

            auto sit2 = s_suppressToggleUntilUp.find(second);
            if (sit2 != s_suppressToggleUntilUp.end() && sit2->second) {
                if (!down2) {
                    sit2->second = false;
                    g_last_bind_end_time = GetTickCount(); g_hotkey_binding_active = false;
                    if (g_listening_hotkey != nullptr && !g_listening_hotkey->listening)
                        g_listening_hotkey = nullptr;
                }
                return;
            }

            bool comboTriggered = false;
            if ((pressed2 || (!down2 && prev2)) && (down1 || prev1)) {
                comboTriggered = true;
            }
            else if ((!down1 && prev1) && down2 && prev2) {
                comboTriggered = true;
            }

            if (comboTriggered) {
                toggleState = !toggleState;
                LavenderHook::Audio::PlayToggleSound(toggleState);

                s_suppressToggleUntilUp[first] = true;
                s_suppressToggleUntilUp[second] = true;
            }
            return;
        }

        // single-key behavior
        const int vk = first;
        const bool down = IsVkDown(vk);
        auto sit = s_suppressToggleUntilUp.find(vk);
        if (sit != s_suppressToggleUntilUp.end() && sit->second) {
            if (!down) {
                sit->second = false;
                g_last_bind_end_time = GetTickCount(); g_hotkey_binding_active = false;
                if (g_listening_hotkey != nullptr && !g_listening_hotkey->listening)
                    g_listening_hotkey = nullptr;
            }
            return;
        }

        if (!IsModifierVk(vk) && down && keyEdge[vk]) {
            // When the key is held, detect if another key becomes held as well.
            if (IsAnyModifierDown(vk) || IsAnyNonModifierKeyDown(vk))
                saw_other_key_while_held = true;
        }

        // Prevent plain hotkeys from firing when a modifier or another non-modifier
        // key is held down, so combos like Ctrl+F9 or G+K can override F9/G/K.
        if (!IsModifierVk(vk) && (IsAnyModifierDown(vk) || IsAnyNonModifierKeyDown(vk)))
            return;

        const bool prev = keyEdge[vk];
        keyEdge[vk] = down;

        if (!down && prev && !saw_other_key_while_held) {
            toggleState = !toggleState;
            LavenderHook::Audio::PlayToggleSound(toggleState);
        }

        if (!down) {
            saw_other_key_while_held = false;
        }
    }

    bool IsAnyHotkeyListening()
    {
        if (g_listening_hotkey != nullptr)
            return true;
        if (g_last_bind_end_time != 0 && GetTickCount() - g_last_bind_end_time < kBindCooldownMs)
            return true;
        g_last_bind_end_time = 0;
        return false;
    }

} // namespace LavenderHook::UI::Lavender
