#include "ToastNotification.h"
#include "../misc/Globals.h"
#include "../sound/SoundPlayer.h"
#include "../imgui/imgui.h"

#include <windows.h>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

extern ImVec4 MAIN_RED;

namespace {

    static const char* kToastClass = "LavenderToastNotif";
    static const int kWidth = 360;
    static const int kHeight = 70;
    static const int kMargin = 12;
    static const int kCorner = 8;
    static const int kAccentH = 3;

    static const int kFadeStep = 12;
    static const int kAlphaMax = 235;
    static const DWORD kHoldMs = 3000;

    enum class Phase { FadeIn, Hold, FadeOut };

    struct ToastState {
        std::string title;
        std::string message;
        int         alpha = 0;
        Phase       phase = Phase::FadeIn;
        DWORD       holdEnd = 0;
        COLORREF    accentColor = RGB(100, 160, 220);
    };

    static COLORREF TypeToColor(LavenderHook::UI::ToastType type)
    {
        switch (type) {
        case LavenderHook::UI::ToastType::Info:    return RGB(100, 160, 220);
        case LavenderHook::UI::ToastType::Success: return RGB(80, 180, 100);
        case LavenderHook::UI::ToastType::Warning: return RGB(220, 170, 50);
        case LavenderHook::UI::ToastType::Error:   return RGB(200, 60, 60);
        default: return RGB(100, 160, 220);
        }
    }

    static LRESULT CALLBACK ToastWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        ToastState* st = reinterpret_cast<ToastState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

        switch (msg)
        {
        case WM_CREATE:
        {
            auto* cs = reinterpret_cast<CREATESTRUCTA*>(lp);
            st = reinterpret_cast<ToastState*>(cs->lpCreateParams);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
            SetTimer(hwnd, 1, 16, nullptr);
            return 0;
        }

        case WM_TIMER:
        {
            if (!st) break;

            switch (st->phase) {
            case Phase::FadeIn:
                st->alpha += kFadeStep;
                if (st->alpha >= kAlphaMax) {
                    st->alpha = kAlphaMax;
                    st->phase = Phase::Hold;
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

            HBRUSH bgBrush = CreateSolidBrush(RGB(18, 18, 22));
            FillRect(hdc, &rc, bgBrush);
            DeleteObject(bgBrush);

            RECT accent = { rc.left, rc.top, rc.right, rc.top + kAccentH };
            HBRUSH accentBrush = CreateSolidBrush(st ? st->accentColor : RGB(100, 160, 220));
            FillRect(hdc, &accent, accentBrush);
            DeleteObject(accentBrush);

            HPEN pen = CreatePen(PS_SOLID, 1, RGB(50, 50, 60));
            HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, pen));
            HBRUSH oldBr = reinterpret_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBr);
            DeleteObject(pen);

            SetBkMode(hdc, TRANSPARENT);

            if (st) {
                HFONT titleFont = CreateFontA(
                    16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
                HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, titleFont));
                SetTextColor(hdc, RGB(240, 240, 245));
                RECT titleRc = { rc.left + kMargin, rc.top + kAccentH + 8,
                                 rc.right - kMargin, rc.top + kAccentH + 28 };
                DrawTextA(hdc, st->title.c_str(), -1, &titleRc, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(hdc, oldFont);
                DeleteObject(titleFont);

                HFONT msgFont = CreateFontA(
                    13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
                oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, msgFont));
                SetTextColor(hdc, RGB(150, 150, 162));
                RECT msgRc = { rc.left + kMargin, rc.top + kAccentH + 30,
                               rc.right - kMargin, rc.bottom - 6 };
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

    static void RegisterToastClassOnce()
    {
        WNDCLASSEXA wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = ToastWndProc;
        wc.hInstance = LavenderHook::Globals::dll_module
            ? LavenderHook::Globals::dll_module
            : GetModuleHandleA(nullptr);
        wc.lpszClassName = kToastClass;
        wc.hbrBackground = nullptr;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExA(&wc);
    }

    static void ToastThread(std::string title, std::string message, COLORREF color, int durationMs)
    {
        RegisterToastClassOnce();

        // Play sound
        LavenderHook::Audio::PlayHideWindowSound();

        const int screenW = GetSystemMetrics(SM_CXSCREEN);
        const int screenH = GetSystemMetrics(SM_CYSCREEN);
        const int x = screenW - kWidth - 24;
        const int y = screenH - kHeight - 60;

        auto* st = new ToastState();
        st->title = std::move(title);
        st->message = std::move(message);
        st->accentColor = color;

        HMODULE hMod = LavenderHook::Globals::dll_module
            ? LavenderHook::Globals::dll_module
            : GetModuleHandleA(nullptr);

        HWND hwnd = CreateWindowExA(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            kToastClass, "",
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

    void ShowToast(const char* title, const char* message, ToastType type, int durationMs)
    {
        COLORREF color = TypeToColor(type);
        std::thread(ToastThread, std::string(title), std::string(message), color, durationMs).detach();
    }

}
