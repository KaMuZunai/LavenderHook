#include "MiscButtonActions.h"
#include "ProcessThumbnailOverlay.h"

#include "../../../minhook/minhook.h"
#include "../../../misc/Globals.h"

#include <windows.h>
#include <shellapi.h>
#include <cstring>
#include "../../../ui/HideNotification.h"
#include "../../../imgui/imgui.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace LavenderHook {
    namespace Input {

        static bool g_fakeCursorEnabled = false;
        static bool g_hooksInstalled = false;

        // Persistent hide-state
        static bool g_hiddenApplied = false;
        static LONG g_prevExStyle = 0;
        static RECT g_prevRect = {};
        static bool g_prevMaximized = false;

        static bool g_trayAdded = false;
        static const UINT g_trayId = 1;

        using GetCursorPos_t = BOOL(WINAPI*)(LPPOINT);
        static GetCursorPos_t oGetCursorPos = nullptr;

        using SetCursorPos_t = BOOL(WINAPI*)(int, int);
        static SetCursorPos_t oSetCursorPos = nullptr;

        using GetMessagePos_t = DWORD(WINAPI*)();
        static GetMessagePos_t oGetMessagePos = nullptr;

        using WindowFromPoint_t = HWND(WINAPI*)(POINT);
        static WindowFromPoint_t oWindowFromPoint = nullptr;

        using ChildWindowFromPointEx_t = HWND(WINAPI*)(HWND, POINT, UINT);
        static ChildWindowFromPointEx_t oChildWindowFromPointEx = nullptr;

        static void MoveWindowToCornerNotContainingCursor(HWND hwnd)
        {
            if (!hwnd) return;

            POINT cur{};
            if (oGetCursorPos) {
                oGetCursorPos(&cur);
            } else {
                ::GetCursorPos(&cur);
            }

            // Enumerate monitors
            std::vector<MONITORINFO> mons;
            struct EnumCtx { std::vector<MONITORINFO>* p; } ctx{ &mons };
            auto enumProc = [](HMONITOR hMon, HDC, LPRECT, LPARAM lParam) -> BOOL {
                auto ctx = reinterpret_cast<EnumCtx*>(lParam);
                MONITORINFO mi; mi.cbSize = sizeof(mi);
                if (GetMonitorInfo(hMon, &mi))
                    ctx->p->push_back(mi);
                return TRUE;
            };
            EnumDisplayMonitors(NULL, NULL, enumProc, reinterpret_cast<LPARAM>(&ctx));
            if (mons.empty()) return;

            // Find monitors that dont contain the cursor
            std::vector<MONITORINFO> candidates;
            for (auto &m : mons) {
                RECT r = m.rcMonitor;
                if (!PtInRect(&r, cur)) candidates.push_back(m);
            }

            MONITORINFO chosen = mons.front();
            if (!candidates.empty()) {
                chosen = *std::max_element(candidates.begin(), candidates.end(), [](const MONITORINFO &a, const MONITORINFO &b){
                    int aw = a.rcMonitor.right - a.rcMonitor.left;
                    int ah = a.rcMonitor.bottom - a.rcMonitor.top;
                    int bw = b.rcMonitor.right - b.rcMonitor.left;
                    int bh = b.rcMonitor.bottom - b.rcMonitor.top;
                    return (aw*ah) < (bw*bh);
                });
            } else {
                // choose monitor with farthest corner
                long bestDist = -1;
                for (auto &m : mons) {
                    RECT r = m.rcMonitor;
                    POINT corners[4] = {
                        { r.left, r.top }, { r.right - 1, r.top }, { r.left, r.bottom - 1 }, { r.right - 1, r.bottom - 1 }
                    };
                    for (auto &c : corners) {
                        long dx = c.x - cur.x; long dy = c.y - cur.y; long d2 = dx*dx + dy*dy;
                        if (d2 > bestDist) { bestDist = d2; chosen = m; }
                    }
                }
            }

            RECT rc = chosen.rcMonitor;
            POINT corners[4] = { { rc.left, rc.top }, { rc.right - 1, rc.top }, { rc.left, rc.bottom - 1 }, { rc.right - 1, rc.bottom - 1 } };
            // pick corner farthest from cursor
            long bestDist = -1; POINT chosenCorner = corners[0];
            for (auto &c : corners) { long dx = c.x - cur.x; long dy = c.y - cur.y; long d2 = dx*dx + dy*dy; if (d2 > bestDist) { bestDist = d2; chosenCorner = c; } }

            const int inset = 4;
            POINT desiredCenter = chosenCorner;
            if (chosenCorner.x == rc.left) desiredCenter.x += inset; else desiredCenter.x -= inset;
            if (chosenCorner.y == rc.top) desiredCenter.y += inset; else desiredCenter.y -= inset;

            RECT wr{}; GetWindowRect(hwnd, &wr);
            int w = wr.right - wr.left; int h = wr.bottom - wr.top;
            int x = desiredCenter.x - (w / 2); int y = desiredCenter.y - (h / 2);

            // Only move if sufficiently far from current pos
            const int moveThreshold = 3;
            if (g_hiddenApplied &&
                (abs(wr.left - x) > moveThreshold || abs(wr.top - y) > moveThreshold)) {
                SetWindowPos(hwnd, HWND_TOP, x, y, w, h, SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
            }
        }

        // Cached screen-space and client-space
        static POINT g_cachedScreenCenter = {};
        static POINT g_cachedClientCenter = {};
        static bool  g_hasCachedCenter = false;

        static bool ResolveFakeCenter(POINT& outScreen, POINT& outClient)
        {
            HWND hwnd = LavenderHook::Globals::window_handle;
            if (!hwnd) return false;

            RECT rc{};
            if (!GetClientRect(hwnd, &rc)) return false;

            POINT clientCenter{
                (rc.right - rc.left) / 2,
                (rc.bottom - rc.top) / 2
            };
            POINT screenCenter = clientCenter;
            ClientToScreen(hwnd, &screenCenter);

            // Small offset that moves the simulated cursor slightly
            POINT modifiedScreen = screenCenter;
            HMONITOR hMon = MonitorFromPoint(screenCenter, MONITOR_DEFAULTTONEAREST);
            if (hMon) {
                MONITORINFO mInfo; mInfo.cbSize = sizeof(mInfo);
                if (GetMonitorInfo(hMon, &mInfo)) {
                    RECT m = mInfo.rcMonitor;
                    POINT corners[4] = { {m.left, m.top}, {m.right - 1, m.top}, {m.left, m.bottom - 1}, {m.right - 1, m.bottom - 1} };
                    int best = 0;
                    long bestDist = -1;
                    for (int i = 0; i < 4; ++i) {
                        long dx = screenCenter.x - corners[i].x;
                        long dy = screenCenter.y - corners[i].y;
                        long d2 = dx*dx + dy*dy;
                        if (bestDist < 0 || d2 < bestDist) { bestDist = d2; best = i; }
                    }

                    // Opposite corner index
                    int opposite = (best ^ 3); // 0<->3, 1<->2

                    // Pixel offset for Simulated Cursor
                    const int off = 20;
                    int dx = (corners[opposite].x > screenCenter.x) ? off : -off;
                    int dy = (corners[opposite].y > screenCenter.y) ? off : -off;

                    modifiedScreen.x += dx;
                    modifiedScreen.y += dy;
                }
            }

            POINT modifiedClient = modifiedScreen;
            ScreenToClient(hwnd, &modifiedClient);

            g_cachedClientCenter = modifiedClient;
            g_cachedScreenCenter = modifiedScreen;
            g_hasCachedCenter = true;

            outClient = g_cachedClientCenter;
            outScreen = g_cachedScreenCenter;
            return true;
        }

        static BOOL WINAPI hkGetCursorPos(LPPOINT lpPoint)
        {
            if (lpPoint && g_fakeCursorEnabled)
            {
                if (g_hasCachedCenter) {
                    *lpPoint = g_cachedScreenCenter;
                    return TRUE;
                }

                POINT scr, cli;
                if (ResolveFakeCenter(scr, cli)) {
                    *lpPoint = scr;
                    return TRUE;
                }
            }
            return oGetCursorPos(lpPoint);
        }

        static HWND WINAPI hkWindowFromPoint(POINT pt)
        {
            if (g_fakeCursorEnabled && g_hasCachedCenter)
            {
                const int threshold = 50; // pixels
                int dx = pt.x - g_cachedScreenCenter.x;
                int dy = pt.y - g_cachedScreenCenter.y;
                if (abs(dx) <= threshold && abs(dy) <= threshold)
                {
                    HWND hwnd = LavenderHook::Globals::window_handle;
                    if (hwnd) return hwnd;
                }
            }
            return oWindowFromPoint(pt);
        }

        static HWND WINAPI hkChildWindowFromPointEx(HWND hWndParent, POINT pt, UINT flags)
        {
            if (g_fakeCursorEnabled && g_hasCachedCenter)
            {
                HWND gameHwnd = LavenderHook::Globals::window_handle;
                if (hWndParent == gameHwnd)
                {
                    const int threshold = 50;
                    int dx = pt.x - g_cachedClientCenter.x;
                    int dy = pt.y - g_cachedClientCenter.y;
                    if (abs(dx) <= threshold && abs(dy) <= threshold)
                        return gameHwnd;
                }
            }
            return oChildWindowFromPointEx(hWndParent, pt, flags);
        }

        static BOOL WINAPI hkSetCursorPos(int X, int Y)
        {
            if (g_fakeCursorEnabled)
                return TRUE;
            return oSetCursorPos(X, Y);
        }

        static DWORD WINAPI hkGetMessagePos()
        {
            if (g_fakeCursorEnabled)
            {
                if (g_hasCachedCenter) {
                    return MAKELONG(static_cast<WORD>(g_cachedScreenCenter.x), static_cast<WORD>(g_cachedScreenCenter.y));
                }
                POINT scr, cli;
                if (ResolveFakeCenter(scr, cli))
                    return MAKELONG(static_cast<WORD>(scr.x), static_cast<WORD>(scr.y));
            }
            return oGetMessagePos();
        }

        static void InstallHooksOnce()
        {
            if (g_hooksInstalled)
                return;

            g_hooksInstalled = true;

            MH_CreateHook(
                &GetCursorPos,
                &hkGetCursorPos,
                reinterpret_cast<void**>(&oGetCursorPos)
            );

            MH_EnableHook(&GetCursorPos);

            // Hook SetCursorPos
            MH_CreateHook(
                &SetCursorPos,
                &hkSetCursorPos,
                reinterpret_cast<void**>(&oSetCursorPos)
            );
            MH_EnableHook(&SetCursorPos);

            // Hook GetMessagePos
            MH_CreateHook(
                &GetMessagePos,
                &hkGetMessagePos,
                reinterpret_cast<void**>(&oGetMessagePos)
            );
            MH_EnableHook(&GetMessagePos);

            // Hook WindowFromPoint
            MH_CreateHook(
                &WindowFromPoint,
                &hkWindowFromPoint,
                reinterpret_cast<void**>(&oWindowFromPoint)
            );
            MH_EnableHook(&WindowFromPoint);

            // Hook ChildWindowFromPointEx
            MH_CreateHook(
                &ChildWindowFromPointEx,
                &hkChildWindowFromPointEx,
                reinterpret_cast<void**>(&oChildWindowFromPointEx)
            );
            MH_EnableHook(&ChildWindowFromPointEx);
        }

        void TickButton11(bool enabled)
        {
            InstallHooksOnce();
            g_fakeCursorEnabled = enabled;

            if (enabled) {
                POINT scr, cli;
                ResolveFakeCenter(scr, cli);
            }
        }

        void TickButton12(bool enabled)
        {
            static bool last = false;

            if (enabled && !last)
            {
                HWND hwnd = LavenderHook::Globals::window_handle;
                if (hwnd)
                {
                    LONG style = GetWindowLong(hwnd, GWL_STYLE);
                    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

                    style &= ~(WS_OVERLAPPEDWINDOW | WS_CAPTION |
                        WS_THICKFRAME | WS_MINIMIZEBOX |
                        WS_MAXIMIZEBOX | WS_SYSMENU);

                    exStyle &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE);

                    SetWindowLong(hwnd, GWL_STYLE, style);
                    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

                    MONITORINFO mi{};
                    mi.cbSize = sizeof(mi);

                    if (GetMonitorInfo(
                        MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
                        &mi))
                    {
                        SetWindowPos(
                            hwnd,
                            HWND_TOP,
                            mi.rcMonitor.left,
                            mi.rcMonitor.top,
                            mi.rcMonitor.right - mi.rcMonitor.left,
                            mi.rcMonitor.bottom - mi.rcMonitor.top,
                            SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_NOZORDER
                        );
                    }
                }
            }

            last = enabled;
        }

        void RestoreHiddenWindowFromTray()
        {
            HWND hwnd = LavenderHook::Globals::window_handle;
            if (!hwnd) return;
            if (!g_hiddenApplied) return;

            // Clear the hidden flags before restoring the window position.
            g_hiddenApplied = false;
            LavenderHook::Globals::window_hidden = false;

            SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
            SetWindowLong(hwnd, GWL_EXSTYLE, g_prevExStyle);

            {
                int w = g_prevRect.right - g_prevRect.left;
                int h = g_prevRect.bottom - g_prevRect.top;
                SetWindowPos(hwnd, HWND_TOP, g_prevRect.left, g_prevRect.top, w, h,
                    SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
                if (g_prevMaximized)
                    ShowWindow(hwnd, SW_MAXIMIZE);
            }

            if (g_trayAdded) {
                NOTIFYICONDATAA nid{};
                nid.cbSize = sizeof(nid);
                nid.hWnd = hwnd;
                nid.uID = g_trayId;
                Shell_NotifyIconA(NIM_DELETE, &nid);
                g_trayAdded = false;
            }

            LavenderHook::UI::DestroyProcessOverlay();

            LavenderHook::Globals::minimize_auto_clear_requested = true;
        }

        void TickButton13(bool enabled)
        {
            static bool last = false;

            HWND hwnd = LavenderHook::Globals::window_handle;

            if (enabled && !last)
            {
                if (hwnd)
                {
                    // mark menu hidden and reset input state
                    LavenderHook::Globals::show_menu = false;
                    LavenderHook::Globals::menu_animating = false;

                    ::ReleaseCapture();
                    ImGui::GetIO().MouseDown[0] = false;
                    ImGui::GetIO().MouseDownDuration[0] = 0.0f;
                    ImGui::ResetMouseDragDelta(0);

                    if (!g_hiddenApplied) {
                        g_prevExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
                        g_prevMaximized = (IsZoomed(hwnd) != FALSE);
                        if (g_prevMaximized)
                            ShowWindow(hwnd, SW_RESTORE);
                        GetWindowRect(hwnd, &g_prevRect);
                        LONG newEx = g_prevExStyle | WS_EX_LAYERED;
                        if (SetWindowLong(hwnd, GWL_EXSTYLE, newEx) != 0) {
                            // Alpha 1, makes window practically invisible
                                if (SetLayeredWindowAttributes(hwnd, 0, 1, LWA_ALPHA)) {
                                g_hiddenApplied = true;
                                LavenderHook::Globals::window_hidden = true;

                                RECT preRect{}; GetWindowRect(hwnd, &preRect);
                                int preW = preRect.right - preRect.left;
                                int preH = preRect.bottom - preRect.top;
                                int preX = preRect.left;
                                int preY = preRect.top;

                                // bump size by 2 pixels and restore to force DWM update
                                SetWindowPos(hwnd, HWND_TOP, preX - 1, preY - 1, preW + 2, preH + 2,
                                    SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
                                SetWindowPos(hwnd, HWND_TOP, preX, preY, preW, preH,
                                    SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);

                                MoveWindowToCornerNotContainingCursor(hwnd);
                            } else {
                                SetWindowLong(hwnd, GWL_EXSTYLE, g_prevExStyle);
                            }
                        }

                        // Add tray icon to allow restore via system tray
                        if (g_hiddenApplied && !g_trayAdded) {
                            HICON hIcon = (HICON)SendMessageA(hwnd, WM_GETICON, ICON_BIG, 0);
                            if (!hIcon) hIcon = (HICON)SendMessageA(hwnd, WM_GETICON, ICON_SMALL, 0);
                            if (!hIcon) hIcon = (HICON)GetClassLongPtrA(hwnd, GCLP_HICON);
                            if (!hIcon) hIcon = (HICON)GetClassLongPtrA(hwnd, GCLP_HICONSM);
                            if (!hIcon) hIcon = LoadIconA(nullptr, IDI_APPLICATION);

                            NOTIFYICONDATAA nid{};
                            nid.cbSize           = sizeof(nid);
                            nid.hWnd             = hwnd;
                            nid.uID              = g_trayId;
                            nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
                            nid.uCallbackMessage = LavenderHook::Globals::tray_callback_message;
                            nid.hIcon            = hIcon;
                            strcpy_s(nid.szTip, sizeof(nid.szTip), "Dungeon Defenders: Awakend");
                            Shell_NotifyIconA(NIM_ADD, &nid);
                            g_trayAdded = true;

                            // Show custom overlay notification
                            LavenderHook::UI::ShowHideNotification(
                                "LavenderHook",
                                "Window hidden. Click System Tray icon to restore.");
                        }

                        if (g_hiddenApplied && LavenderHook::Globals::show_process_overlay_on_hide)
                            LavenderHook::UI::CreateProcessOverlay(hwnd);
                    }
                }
            }

            if (!enabled && last)
            {
                HWND hwnd = LavenderHook::Globals::window_handle;
                if (hwnd && g_hiddenApplied)
                {
                    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
                        SetWindowLong(hwnd, GWL_EXSTYLE, g_prevExStyle);

                        // Restore previous position/size
                        {
                            int w = g_prevRect.right - g_prevRect.left;
                            int h = g_prevRect.bottom - g_prevRect.top;
                            SetWindowPos(hwnd, HWND_TOP, g_prevRect.left, g_prevRect.top, w, h,
                                SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
                            if (g_prevMaximized)
                                ShowWindow(hwnd, SW_MAXIMIZE);
                        }

                    if (g_trayAdded) {
                        NOTIFYICONDATAA nid{};
                        nid.cbSize = sizeof(nid);
                        nid.hWnd = hwnd;
                        nid.uID = g_trayId;
                        Shell_NotifyIconA(NIM_DELETE, &nid);
                        g_trayAdded = false;
                    }

                    g_hiddenApplied = false;
                    LavenderHook::Globals::window_hidden = false;
                    LavenderHook::UI::DestroyProcessOverlay();
                }
            }

            // While hidden, actively ensure the window stays at a corner not
            // containing the cursor so the simulated cursor offset remains valid.
            if (g_hiddenApplied && hwnd) {
                MoveWindowToCornerNotContainingCursor(hwnd);
            }

            last = enabled;
        }

    } // namespace Input
} // namespace LavenderHook
