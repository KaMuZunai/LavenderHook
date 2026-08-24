#include "HideNotification.h"
#include "../misc/Globals.h"
#include "../sound/SoundPlayer.h"
#include "../imgui/imgui.h"

#include <windows.h>
#include <string>
#include <thread>

// Theme color globals
extern ImVec4 MAIN_RED;
extern ImVec4 MID_RED;
extern ImVec4 DARK_RED;

namespace {

    static const char* kNotifClass = "LavenderHideNotif";

    static const int kWidth     = 400;
    static const int kHeight    = 90;
    static const int kMargin    = 18;
    static const int kCorner    = 10;
    static const int kAccentH   = 4;

    // Fade / timing constants
    static const int   kTimerMs    = 16;   // ~60 fps tick
    static const int   kFadeStep   = 14;   // alpha units per tick while fading
    static const int   kAlphaMax   = 230;
    static const DWORD kHoldMs     = 5000; // how long to stay fully visible

    enum class Phase { FadeIn, Hold, FadeOut };

    static COLORREF ImVecToColorref(const ImVec4& c)
    {
        return RGB(static_cast<BYTE>(c.x * 255.f),
                   static_cast<BYTE>(c.y * 255.f),
                   static_cast<BYTE>(c.z * 255.f));
    }

    struct NotifState {
        std::string title;
        std::string message;
        int         alpha       = 0;
        Phase       phase       = Phase::FadeIn;
        DWORD       holdEnd     = 0;
        COLORREF    accentColor = RGB(120, 80, 220); // snapshotted from MAIN_RED
    };

    static LRESULT CALLBACK NotifWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        NotifState* st = reinterpret_cast<NotifState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

        switch (msg)
        {
        case WM_CREATE:
        {
            auto* cs = reinterpret_cast<CREATESTRUCTA*>(lp);
            st = reinterpret_cast<NotifState*>(cs->lpCreateParams);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
            SetTimer(hwnd, 1, kTimerMs, nullptr);
            return 0;
        }

        case WM_TIMER:
        {
            if (!st) break;

            switch (st->phase)
            {
            case Phase::FadeIn:
                st->alpha += kFadeStep;
                if (st->alpha >= kAlphaMax) {
                    st->alpha   = kAlphaMax;
                    st->phase   = Phase::Hold;
                    st->holdEnd = GetTickCount() + kHoldMs;
                }
                break;

            case Phase::Hold:
                if (GetTickCount() >= st->holdEnd)
                    st->phase = Phase::FadeOut;
                break;

            case Phase::FadeOut:
                st->alpha -= kFadeStep;
                if (st->alpha <= 0) {
                    st->alpha = 0;
                    KillTimer(hwnd, 1);
                    DestroyWindow(hwnd);
                    return 0;
                }
                break;
            }

            SetLayeredWindowAttributes(hwnd, 0, static_cast<BYTE>(st->alpha), LWA_ALPHA);
            break;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);

            // Background
            HBRUSH bgBrush = CreateSolidBrush(RGB(22, 22, 26));
            FillRect(hdc, &rc, bgBrush);
            DeleteObject(bgBrush);

            // Accent bar at top
            RECT accent = { rc.left, rc.top, rc.right, rc.top + kAccentH };
            HBRUSH accentBrush = CreateSolidBrush(st ? st->accentColor : RGB(120, 80, 220));
            FillRect(hdc, &accent, accentBrush);
            DeleteObject(accentBrush);

            // Subtle border
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(60, 60, 70));
            HPEN oldPen   = reinterpret_cast<HPEN>(SelectObject(hdc, pen));
            HBRUSH oldBr  = reinterpret_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBr);
            DeleteObject(pen);

            SetBkMode(hdc, TRANSPARENT);

            if (st) {
                // Title
                HFONT titleFont = CreateFontA(
                    18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
                HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, titleFont));
                SetTextColor(hdc, RGB(240, 240, 245));
                RECT titleRc = { rc.left + kMargin, rc.top + kAccentH + 10,
                                 rc.right - kMargin, rc.top + kAccentH + 32 };
                DrawTextA(hdc, st->title.c_str(), -1, &titleRc, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(hdc, oldFont);
                DeleteObject(titleFont);

                // Message
                HFONT msgFont = CreateFontA(
                    15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
                oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, msgFont));
                SetTextColor(hdc, RGB(160, 160, 172));
                RECT msgRc = { rc.left + kMargin, rc.top + kAccentH + 36,
                               rc.right - kMargin, rc.bottom - 8 };
                DrawTextA(hdc, st->message.c_str(), -1, &msgRc, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
                SelectObject(hdc, oldFont);
                DeleteObject(msgFont);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            delete st;
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcA(hwnd, msg, wp, lp);
    }

    static void RegisterNotifClassOnce()
    {
        WNDCLASSEXA wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = NotifWndProc;
        wc.hInstance     = LavenderHook::Globals::dll_module
                               ? LavenderHook::Globals::dll_module
                               : GetModuleHandleA(nullptr);
        wc.lpszClassName = kNotifClass;
        wc.hbrBackground = nullptr;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExA(&wc);
    }

    static void NotifThread(std::string title, std::string message)
    {
        Sleep(1000);

        LavenderHook::Audio::PlayHideWindowSound();

        RegisterNotifClassOnce();

        // Position
        const int screenW = GetSystemMetrics(SM_CXSCREEN);
        const int screenH = GetSystemMetrics(SM_CYSCREEN);
        const int x = screenW - kWidth  - 20;
        const int y = screenH - kHeight - 60;

        auto* st = new NotifState();
        st->title       = std::move(title);
        st->message     = std::move(message);
        st->accentColor = ImVecToColorref(MAIN_RED);

        HMODULE hMod = LavenderHook::Globals::dll_module
                           ? LavenderHook::Globals::dll_module
                           : GetModuleHandleA(nullptr);

        HWND hwnd = CreateWindowExA(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            kNotifClass,
            "",
            WS_POPUP,
            x, y, kWidth, kHeight,
            nullptr, nullptr, hMod, st);

        if (!hwnd) { delete st; return; }

        SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        UpdateWindow(hwnd);

        MSG msg;
        while (GetMessageA(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

} // namespace

namespace LavenderHook::UI {

    void ShowHideNotification(const char* title, const char* message)
    {
        std::thread(NotifThread, std::string(title), std::string(message)).detach();
    }

} // namespace LavenderHook::UI
