#include "FocusShim.h"
#include "../minhook/MinHook.h"
#include "../misc/Globals.h"

namespace {
    using SetCursorPos_t = BOOL(WINAPI*)(int, int);
    using ClipCursor_t = BOOL(WINAPI*)(const RECT*);
    using SetCapture_t = HWND(WINAPI*)(HWND);

    SetCursorPos_t  oSetCursorPos = nullptr;
    ClipCursor_t    oClipCursor = nullptr;
    SetCapture_t    oSetCapture = nullptr;

    bool g_clip_saved = false;
    RECT g_prev_clip = {};
    bool g_last_menu_state = false;

    BOOL WINAPI hkSetCursorPos(int X, int Y) {
        if (LavenderHook::Globals::show_menu && LavenderHook::Globals::simulate_unfocused)
            return TRUE;
        return oSetCursorPos(X, Y);
    }

    BOOL WINAPI hkClipCursor(const RECT* rect) {
        if (LavenderHook::Globals::show_menu && LavenderHook::Globals::simulate_unfocused)
            return TRUE;
        return oClipCursor(rect);
    }

    HWND WINAPI hkSetCapture(HWND hWnd) {
        if (LavenderHook::Globals::show_menu && LavenderHook::Globals::simulate_unfocused)
            return nullptr;
        return oSetCapture(hWnd);
    }

    void SaveAndReleaseClip() {
        if (!g_clip_saved) {
            RECT cur{};
            if (GetClipCursor(&cur)) { g_prev_clip = cur; g_clip_saved = true; }
        }
        ::ClipCursor(nullptr);
    }

    void RestoreClipIfNeeded() {
        if (g_clip_saved) {
            ::ClipCursor(&g_prev_clip);
            g_clip_saved = false;
            g_prev_clip = {};
        }
    }
}

namespace LavenderHook::Input::FocusShim {

    bool Install() {
        const MH_STATUS init = MH_Initialize();
        if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED)
            return false;

        if (MH_CreateHook(&SetCursorPos, &hkSetCursorPos, reinterpret_cast<LPVOID*>(&oSetCursorPos)) != MH_OK) return false;
        if (MH_CreateHook(&ClipCursor, &hkClipCursor, reinterpret_cast<LPVOID*>(&oClipCursor)) != MH_OK) return false;
        if (MH_CreateHook(&SetCapture, &hkSetCapture, reinterpret_cast<LPVOID*>(&oSetCapture)) != MH_OK) return false;

        if (MH_EnableHook(&SetCursorPos) != MH_OK) return false;
        if (MH_EnableHook(&ClipCursor) != MH_OK) return false;
        if (MH_EnableHook(&SetCapture) != MH_OK) return false;

        g_last_menu_state = LavenderHook::Globals::show_menu;
        if (g_last_menu_state && LavenderHook::Globals::simulate_unfocused)
            SaveAndReleaseClip();

        return true;
    }

    void Remove() {
        RestoreClipIfNeeded();

        MH_DisableHook(&SetCursorPos);
        MH_DisableHook(&ClipCursor);
        MH_DisableHook(&SetCapture);
    }

    void PerFrameUpdate() {
        const bool want_unfocus = (LavenderHook::Globals::show_menu && LavenderHook::Globals::simulate_unfocused);

        if (want_unfocus && !g_last_menu_state)
            SaveAndReleaseClip();
        else if (!want_unfocus && g_last_menu_state)
            RestoreClipIfNeeded();

        g_last_menu_state = LavenderHook::Globals::show_menu;

        if (want_unfocus)
            while (ShowCursor(TRUE) < 0) {}
    }
}
