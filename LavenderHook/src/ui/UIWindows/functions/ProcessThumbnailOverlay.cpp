#include "ProcessThumbnailOverlay.h"
#include "../../../misc/Globals.h"
#include "../../../imgui/imgui.h"
#include "../../../memory/aobhooks/ChatHook.h"

#include <windows.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>

// Theme color globals
extern ImVec4 MAIN_RED;
extern ImVec4 MID_RED;
extern ImVec4 DARK_RED;

namespace LavenderHook {
    namespace UI {

        static const wchar_t kThumbnailClass[] = L"LvHkThumbnailOverlay";

        static constexpr int kTitleH      = 30;
        static constexpr int kBorder      = 6; 
        static constexpr int kVideoBorder = 2;
        static constexpr int kCornerR     = 10;

        static constexpr int kChatBtnW = 58;
        static constexpr int kChatBtnH = 22;
        // Chat panel height scales with window
        static int GetChatPanelHeight(int totalClientH)
        {
            int h = totalClientH / 3;
            if (h > 300) h = 300;
            if (h < 125) h = 125;
            return h;
        }
        static constexpr int kChatLineH = 22;
        static constexpr int kChatPadX = 8;
        static constexpr int kChatPadY = 4;
        static constexpr UINT kChatTimerId = 1001;
        static constexpr int kChatTimerInterval = 1000;

        // GDI colors
        struct OverlayColors {
            COLORREF bg;
            COLORREF main;
            COLORREF mid;
            COLORREF dark;
        };
        static OverlayColors g_colors;

        static HWND       g_overlayHwnd   = nullptr;
        static HANDLE     g_overlayThread = nullptr;
        static HWND       g_gameHwnd      = nullptr;
        static HTHUMBNAIL g_thumbnail     = nullptr;
        static float      g_aspectRatio   = 0.f;

        // Chat state
        static bool       g_chatExpanded  = false;
        static int        g_chatScrollOff = 0;
        static int        g_chatBaseH     = 0; // height of window when chat is hidden

        // Helpers 
        static COLORREF ImVecToColorref(const ImVec4& c)
        {
            return RGB(static_cast<BYTE>(c.x * 255.f),
                       static_cast<BYTE>(c.y * 255.f),
                       static_cast<BYTE>(c.z * 255.f));
        }

        // polished-theme GDI helpers
        static bool Polished() { return LavenderHook::Globals::use_polished_overlay; }

        static COLORREF LerpColor(COLORREF a, COLORREF b, float t)
        {
            if (t < 0.f) t = 0.f;
            if (t > 1.f) t = 1.f;
            int ar = GetRValue(a), ag = GetGValue(a), ab = GetBValue(a);
            int br = GetRValue(b), bg = GetGValue(b), bb = GetBValue(b);
            return RGB((BYTE)(ar + (br - ar) * t),
                       (BYTE)(ag + (bg - ag) * t),
                       (BYTE)(ab + (bb - ab) * t));
        }

        // Vertical gradient via scanline fills (no msimg32 dependency).
        static void VGradient(HDC hdc, const RECT& rc, COLORREF top, COLORREF bot)
        {
            const int h = rc.bottom - rc.top;
            if (h <= 0) return;
            for (int y = 0; y < h; ++y) {
                float t = (float)y / (float)(h > 1 ? h - 1 : 1);
                COLORREF c = LerpColor(top, bot, t);
                HBRUSH br = CreateSolidBrush(c);
                RECT line = { rc.left, rc.top + y, rc.right, rc.top + y + 1 };
                FillRect(hdc, &line, br);
                DeleteObject(br);
            }
        }

        // Horizontal accent strip fading
        static void HGradientStrip(HDC hdc, const RECT& rc, COLORREF accent, COLORREF fade)
        {
            const int w = rc.right - rc.left;
            if (w <= 0) return;
            for (int x = 0; x < w; ++x) {
                float t = (float)x / (float)(w > 1 ? w - 1 : 1);
                COLORREF c = LerpColor(accent, fade, t);
                HBRUSH br = CreateSolidBrush(c);
                RECT line = { rc.left + x, rc.top, rc.left + x + 1, rc.bottom };
                FillRect(hdc, &line, br);
                DeleteObject(br);
            }
        }

        // Rounded accent pill with a top sheen.
        static void DrawPolishedPill(HDC hdc, const RECT& rc, COLORREF fill,
                                     COLORREF border, bool sheen)
        {
            const int r = (rc.bottom - rc.top) / 2;

            // Base rounded fill + border.
            HBRUSH fb = CreateSolidBrush(fill);
            HPEN   bp = CreatePen(PS_SOLID, 1, border);
            HBRUSH ofb = (HBRUSH)SelectObject(hdc, fb);
            HPEN   obp = (HPEN)SelectObject(hdc, bp);
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, r * 2, r * 2);
            SelectObject(hdc, ofb);
            SelectObject(hdc, obp);
            DeleteObject(fb);
            DeleteObject(bp);

            // Sheen
            if (sheen) {
                HRGN clip = CreateRoundRectRgn(rc.left + 1, rc.top + 1,
                                               rc.right, rc.bottom, r * 2, r * 2);
                SelectClipRgn(hdc, clip);
                RECT topHalf = { rc.left, rc.top,
                                 rc.right, rc.top + (rc.bottom - rc.top) / 2 };
                COLORREF s = LerpColor(fill, RGB(255, 255, 255), 0.22f);
                VGradient(hdc, topHalf, s, fill);
                SelectClipRgn(hdc, nullptr);
                DeleteObject(clip);
            }
        }

        // DWM attribute constants.
        #ifndef DWMWA_WINDOW_CORNER_PREFERENCE
        #define DWMWA_WINDOW_CORNER_PREFERENCE 33
        #endif
        #ifndef DWMWA_BORDER_COLOR
        #define DWMWA_BORDER_COLOR 34
        #endif
        #ifndef DWMWCP_ROUND
        #define DWMWCP_ROUND 2
        #endif

        // hardware-anti-aliased rounded corners.
        static void ApplyWindowStyle(HWND hwnd)
        {
            // Make sure no hard region clips the corners.
            SetWindowRgn(hwnd, nullptr, TRUE);

            int corner = DWMWCP_ROUND;
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                                  &corner, sizeof(corner));

            COLORREF border = Polished() ? g_colors.mid : g_colors.main;
            DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR,
                                  &border, sizeof(border));
        }

        // Returns the chat panel client rect
        static void GetChatPanelRect(const RECT& cr, RECT& out)
        {
            const int chatH = GetChatPanelHeight(cr.bottom - cr.top);
            const int videoBottom = cr.bottom - kVideoBorder;
            out.left   = cr.left   + kVideoBorder;
            out.top    = videoBottom - chatH;
            out.right  = cr.right  - kVideoBorder;
            out.bottom = videoBottom - kVideoBorder;
        }

        static void GetChatBtnRect(const RECT& cr, RECT& out)
        {
            out.left   = cr.right - kChatBtnW - 6;
            out.top    = (kTitleH - kChatBtnH) / 2;
            out.right  = out.left + kChatBtnW;
            out.bottom = out.top + kChatBtnH;
        }

        // Cached Segoe UI Semibold title font.
        static HFONT TitleFont()
        {
            static HFONT s_font = nullptr;
            if (!s_font) {
                s_font = CreateFontW(
                    16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Semibold");
            }
            return s_font;
        }
        static HFONT ButtonFont()
        {
            static HFONT s_font = nullptr;
            if (!s_font) {
                s_font = CreateFontW(
                    12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Semibold");
            }
            return s_font;
        }
        static HFONT ChatFont()
        {
            static HFONT s_font = nullptr;
            if (!s_font) {
                s_font = CreateFontW(
                    15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Semibold");
            }
            return s_font;
        }

        static void DrawTitleBar(HDC hdc, const RECT& cr)
        {
            RECT titleRc = { cr.left, cr.top, cr.right, cr.top + kTitleH };

            if (Polished()) {
                // vertical sheen across the whole bar
                COLORREF sheenTop = LerpColor(g_colors.bg, RGB(255, 255, 255), 0.12f);
                COLORREF sheenMid = LerpColor(g_colors.bg, RGB(255, 255, 255), 0.03f);
                RECT topHalf = { cr.left, cr.top, cr.right, cr.top + kTitleH / 2 };
                RECT botHalf = { cr.left, cr.top + kTitleH / 2, cr.right, cr.top + kTitleH };
                VGradient(hdc, topHalf, sheenTop, sheenMid);
                VGradient(hdc, botHalf, sheenMid, g_colors.bg);

                // Accent left tick.
                HBRUSH tick = CreateSolidBrush(g_colors.mid);
                RECT tickRc = { cr.left, cr.top, cr.left + 3, cr.top + kTitleH };
                FillRect(hdc, &tickRc, tick);
                DeleteObject(tick);

                // Glowing accent underline.
                RECT sep = { cr.left, cr.top + kTitleH - 2, cr.right, cr.top + kTitleH };
                HGradientStrip(hdc, sep, g_colors.main, g_colors.bg);
                RECT glow = { cr.left, cr.top + kTitleH, cr.right, cr.top + kTitleH + 3 };
                VGradient(hdc, glow, LerpColor(g_colors.bg, g_colors.main, 0.25f), g_colors.bg);
            }
            else {
                HBRUSH bg = CreateSolidBrush(g_colors.bg);
                FillRect(hdc, &titleRc, bg);
                DeleteObject(bg);

                HBRUSH s1 = CreateSolidBrush(g_colors.main);
                RECT sep1 = { cr.left, cr.top + kTitleH - 2, cr.right, cr.top + kTitleH - 1 };
                FillRect(hdc, &sep1, s1);
                DeleteObject(s1);

                HBRUSH s2 = CreateSolidBrush(g_colors.dark);
                RECT sep2 = { cr.left, cr.top + kTitleH - 1, cr.right, cr.top + kTitleH };
                FillRect(hdc, &sep2, s2);
                DeleteObject(s2);
            }

            SetBkMode(hdc, TRANSPARENT);

            // title text.
            const int textLeft = cr.left + 14;
            int dotCx = cr.left + (Polished() ? 14 : 12);
            int dotCy = cr.top + kTitleH / 2;
            int textX = textLeft;
            if (Polished()) {
                HBRUSH dot = CreateSolidBrush(g_colors.mid);
                HPEN   dpen = CreatePen(PS_SOLID, 1, g_colors.mid);
                HBRUSH od = (HBRUSH)SelectObject(hdc, dot);
                HPEN   op = (HPEN)SelectObject(hdc, dpen);
                Ellipse(hdc, dotCx - 3, dotCy - 3, dotCx + 3, dotCy + 3);
                SelectObject(hdc, od); SelectObject(hdc, op);
                DeleteObject(dot); DeleteObject(dpen);
                textX = dotCx + 10;
            }

            HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, TitleFont()));
            // soft text shadow for depth
            RECT trS = { textX + 1, cr.top + 1, cr.right - kChatBtnW - 16, cr.top + kTitleH };
            SetTextColor(hdc, RGB(0, 0, 0));
            DrawTextW(hdc, L"LavenderHook", -1, &trS, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            RECT tr = { textX, cr.top, cr.right - kChatBtnW - 16, cr.top + kTitleH };
            SetTextColor(hdc, Polished() ? LerpColor(g_colors.mid, RGB(255,255,255), 0.55f)
                                         : g_colors.dark);
            DrawTextW(hdc, L"LavenderHook", -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, oldFont);

            // Chat button
            RECT btnRc;
            GetChatBtnRect(cr, btnRc);
            if (Polished()) {
                COLORREF fill   = g_chatExpanded ? g_colors.main
                                                 : LerpColor(g_colors.bg, g_colors.main, 0.28f);
                COLORREF border = g_chatExpanded ? g_colors.mid : g_colors.main;
                DrawPolishedPill(hdc, btnRc, fill, border, true);
            }
            else {
                HBRUSH btnBg = CreateSolidBrush(g_chatExpanded ? g_colors.main : g_colors.dark);
                FillRect(hdc, &btnRc, btnBg);
                DeleteObject(btnBg);
            }

            HFONT oldBf = static_cast<HFONT>(SelectObject(hdc, ButtonFont()));
            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkMode(hdc, TRANSPARENT);
            DrawTextW(hdc, g_chatExpanded ? L"CHAT \u25B8" : L"CHAT", -1, &btnRc,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, oldBf);
        }

        static void DrawChatPanel(HDC hdc, const RECT& cr)
        {
            if (!g_chatExpanded) return;

            RECT panelRc;
            GetChatPanelRect(cr, panelRc);
            if (panelRc.bottom <= panelRc.top || panelRc.right <= panelRc.left)
                return;

            // Dark background
            HBRUSH bg = CreateSolidBrush(g_colors.bg);
            FillRect(hdc, &panelRc, bg);
            DeleteObject(bg);

            // Top border line / bloom
            if (Polished()) {
                RECT bloom = { panelRc.left, panelRc.top, panelRc.right, panelRc.top + 1 };
                HGradientStrip(hdc, bloom, g_colors.main, g_colors.bg);
                RECT glow = { panelRc.left, panelRc.top + 1, panelRc.right, panelRc.top + 4 };
                VGradient(hdc, glow, LerpColor(g_colors.bg, g_colors.main, 0.30f), g_colors.bg);
            }
            else {
                HPEN sepPen = CreatePen(PS_SOLID, 1, g_colors.main);
                HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, sepPen));
                MoveToEx(hdc, panelRc.left, panelRc.top, nullptr);
                LineTo(hdc, panelRc.right, panelRc.top);
                SelectObject(hdc, oldPen);
                DeleteObject(sepPen);
            }

            // Collect messages for display
            struct ChatLine {
                std::wstring sender;
                std::wstring text;
            };
            std::vector<ChatLine> chatLines;
            {
                std::lock_guard<std::mutex> lock(Memory::ChatHook::g_mutex);
                const int count = Memory::ChatHook::g_count.load(std::memory_order_relaxed);
                if (count > 0)
                {
                    const int startIdx = count < Memory::ChatHook::kMaxMessages
                        ? 0
                        : Memory::ChatHook::g_writeIndex;
                    for (int i = 0; i < count; ++i)
                    {
                        const int idx = (startIdx + i) % Memory::ChatHook::kMaxMessages;
                        const auto& msg = Memory::ChatHook::g_messages[idx];
                        if (msg.message.empty()) continue;

                        ChatLine cl;
                        cl.sender = std::wstring(msg.sender.begin(), msg.sender.end());
                        cl.text   = std::wstring(msg.message.begin(), msg.message.end());
                        chatLines.push_back(std::move(cl));
                    }
                }
            }

            if (chatLines.empty())
            {
                HFONT oldEf = static_cast<HFONT>(SelectObject(hdc, ChatFont()));
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, Polished()
                    ? LerpColor(g_colors.bg, RGB(255, 255, 255), 0.40f)
                    : RGB(120, 120, 130));
                RECT emptyRc = panelRc;
                emptyRc.left += kChatPadX;
                emptyRc.top += kChatPadY;
                DrawTextW(hdc, L"No messages yet", -1, &emptyRc,
                          DT_LEFT | DT_TOP | DT_SINGLELINE);
                SelectObject(hdc, oldEf);
                return;
            }

            HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, ChatFont()));
            SetBkMode(hdc, TRANSPARENT);

            const int panelW = panelRc.right - panelRc.left;
            const int panelBodyW = panelW - kChatPadX * 2;

            // Determine sender name column width
            int nameW = 0;
            for (const auto& cl : chatLines)
            {
                if (cl.sender.empty()) continue;
                std::wstring tmp = cl.sender + L": ";
                SIZE sz;
                GetTextExtentPoint32W(hdc, tmp.c_str(),
                    static_cast<int>(tmp.size()), &sz);
                if (sz.cx > nameW) nameW = sz.cx;
            }
            if (nameW < 50) nameW = 50;
            if (nameW > panelW - 20) nameW = panelW / 3;
            const int textW = panelBodyW - nameW;
            const RECT crc = { 0, 0, 0, 0 };

            // Pre-calculate wrapped height for each message
            std::vector<int> msgHeights(chatLines.size());
            for (size_t i = 0; i < chatLines.size(); ++i)
            {
                const auto& cl = chatLines[i];
                if (!cl.sender.empty() && !cl.text.empty())
                {
                    RECT mr = { 0, 0, textW, 0 };
                    DrawTextW(hdc, cl.text.c_str(), -1, &mr,
                              DT_CALCRECT | DT_WORDBREAK);
                    msgHeights[i] = mr.bottom - mr.top;
                }
                else
                {
                    const wchar_t* txt = !cl.sender.empty()
                        ? cl.sender.c_str() : cl.text.c_str();
                    RECT mr = { 0, 0, panelBodyW, 0 };
                    DrawTextW(hdc, txt, -1, &mr,
                              DT_CALCRECT | DT_WORDBREAK);
                    msgHeights[i] = mr.bottom - mr.top;
                }
                if (msgHeights[i] < kChatLineH)
                    msgHeights[i] = kChatLineH;
                msgHeights[i] += 1; // gap between messages
            }

            // Total content height
            int contentH = 0;
            for (auto h : msgHeights) contentH += h;

            const int panelContentH = (panelRc.bottom - panelRc.top) - kChatPadY * 2;

            // Clamp pixel scroll
            const int maxScroll = std::max(0, contentH - panelContentH);
            g_chatScrollOff = std::clamp(g_chatScrollOff, 0, maxScroll);

            // Draw from bottom to top, most recent at the bottom
            int yCursor = panelRc.bottom - kChatPadY + g_chatScrollOff;
            for (size_t ri = 1; ri <= chatLines.size(); ++ri)
            {
                const size_t i = chatLines.size() - ri;
                int h = msgHeights[i] - 1; // remove gap
                yCursor -= h;
                if (yCursor + h < panelRc.top + kChatPadY)
                    break; // above visible area
                if (yCursor > panelRc.bottom - kChatPadY)
                {
                    yCursor -= 1; // gap still applies
                    continue; // below visible area
                }

                const auto& cl = chatLines[i];

                if (!cl.sender.empty() && !cl.text.empty())
                {
                    // Draw sender in main color
                    SetTextColor(hdc, g_colors.main);
                    RECT nameRc = {
                        panelRc.left + kChatPadX, yCursor,
                        panelRc.left + kChatPadX + nameW, yCursor + h
                    };
                    std::wstring label = cl.sender + L":";
                    DrawTextW(hdc, label.c_str(), static_cast<int>(label.size()),
                              &nameRc, DT_LEFT | DT_TOP | DT_SINGLELINE);

                    // Draw message in white
                    SetTextColor(hdc, RGB(220, 220, 220));
                    RECT msgRc = {
                        panelRc.left + kChatPadX + nameW, yCursor,
                        panelRc.right - kChatPadX, yCursor + h
                    };
                    DrawTextW(hdc, cl.text.c_str(), -1,
                              &msgRc, DT_LEFT | DT_TOP | DT_WORDBREAK);
                }
                else
                {
                    SetTextColor(hdc, RGB(200, 200, 200));
                    const wchar_t* txt = !cl.sender.empty()
                        ? cl.sender.c_str() : cl.text.c_str();
                    RECT lineRc = {
                        panelRc.left + kChatPadX, yCursor,
                        panelRc.right - kChatPadX, yCursor + h
                    };
                    DrawTextW(hdc, txt, -1,
                              &lineRc, DT_LEFT | DT_TOP | DT_WORDBREAK);
                }

                yCursor -= 1; // gap
            }

            SelectObject(hdc, oldFont);
        }

        // Fits the DWM thumbnail inside the border strips
        static void UpdateThumbnailRect(HWND hwnd)
        {
            if (!g_thumbnail || !hwnd) return;

            RECT rc{};
            GetClientRect(hwnd, &rc);

            int videoBottom = rc.bottom - kVideoBorder;
            if (g_chatExpanded)
                videoBottom -= GetChatPanelHeight(rc.bottom - rc.top);

            RECT dest = {
                rc.left   + kVideoBorder,
                rc.top    + kTitleH,
                rc.right  - kVideoBorder,
                videoBottom
            };
            if (dest.right <= dest.left || dest.bottom <= dest.top) return;

            DWM_THUMBNAIL_PROPERTIES props{};
            props.dwFlags               = DWM_TNP_SOURCECLIENTAREAONLY
                                        | DWM_TNP_VISIBLE
                                        | DWM_TNP_RECTDESTINATION
                                        | DWM_TNP_OPACITY;
            props.fSourceClientAreaOnly = TRUE;
            props.fVisible              = TRUE;
            props.rcDestination         = dest;
            props.opacity               = 255;

            if (FAILED(DwmUpdateThumbnailProperties(g_thumbnail, &props)))
            {
                DwmUnregisterThumbnail(g_thumbnail);
                g_thumbnail = nullptr;
            }
        }

        // Window procedure
        static LRESULT CALLBACK ThumbnailWndProc(HWND hwnd, UINT msg,
                                                 WPARAM wp, LPARAM lp)
        {
            switch (msg)
            {
            case WM_NCHITTEST:
            {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                RECT  wr{};
                GetWindowRect(hwnd, &wr);

                const bool onLeft   = pt.x <  wr.left   + kBorder;
                const bool onRight  = pt.x >  wr.right  - kBorder;
                const bool onTop    = pt.y <  wr.top    + kBorder;
                const bool onBottom = pt.y >  wr.bottom - kBorder;

                if (onLeft  && onTop)    return HTTOPLEFT;
                if (onRight && onTop)    return HTTOPRIGHT;
                if (onLeft  && onBottom) return HTBOTTOMLEFT;
                if (onRight && onBottom) return HTBOTTOMRIGHT;
                if (onLeft)              return HTLEFT;
                if (onRight)             return HTRIGHT;
                if (onTop)               return HTTOP;
                if (onBottom)            return HTBOTTOM;

                // Check if click is on the chat button — pass to client area
                if (pt.y >= wr.top && pt.y < wr.top + kTitleH)
                {
                    POINT clientPt = { pt.x - wr.left, pt.y - wr.top };
                    RECT cr;
                    GetClientRect(hwnd, &cr);
                    RECT btnRc;
                    GetChatBtnRect(cr, btnRc);
                    if (clientPt.x >= btnRc.left && clientPt.x <= btnRc.right)
                        return HTCLIENT;
                }

                return HTCAPTION;
            }

            case WM_NCRBUTTONDOWN:
            case WM_NCRBUTTONUP:
            case WM_NCLBUTTONDBLCLK:
                return 0;

            case WM_SIZING:
            {
                if (g_aspectRatio <= 0.f)
                    break;

                RECT* r = reinterpret_cast<RECT*>(lp);
                const int frameW = kVideoBorder * 2;
                const int videoW = (r->right  - r->left) - frameW;

                const bool dragH = (wp == WMSZ_LEFT   || wp == WMSZ_RIGHT  ||
                                    wp == WMSZ_TOPLEFT || wp == WMSZ_TOPRIGHT ||
                                    wp == WMSZ_BOTTOMLEFT || wp == WMSZ_BOTTOMRIGHT);

                const int rawVideoH = (videoW > 0)
                    ? static_cast<int>(videoW / g_aspectRatio)
                    : 0;

                // Compute chat panel from target dimensions
                int chatExtra = 0;
                if (g_chatExpanded)
                {
                    // chatH = (rawVideoH + titleH + bottomBorder) / 2  => solves
                    //   totalH = videoH + titleH + border + chatH
                    //   chatH = f(totalH) = totalH/3
                    //   totalH = videoH + titleH + border + totalH/3
                    //   2*totalH/3 = videoH + titleH + border
                    //   totalH = 3/2*(videoH + titleH + border)
                    //   chatH = totalH/3 = (videoH + titleH + border)/2
                    const int nonVideoH = kTitleH + kVideoBorder;
                    chatExtra = (rawVideoH + nonVideoH) / 2;
                    if (chatExtra > 300) chatExtra = 300;
                    if (chatExtra < 125) chatExtra = 125;
                }

                const int frameH = kTitleH + kVideoBorder + chatExtra;
                const int newH = (rawVideoH > 0) ? rawVideoH + frameH : frameH + 1;

                if (dragH) {
                    if (wp == WMSZ_TOPLEFT || wp == WMSZ_TOPRIGHT)
                        r->top = r->bottom - newH;
                    else
                        r->bottom = r->top + newH;
                } else {
                    r->bottom = r->top + newH;
                }
                return TRUE;
            }

            case WM_LBUTTONDOWN:
            {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                if (pt.y < kTitleH)
                {
                    RECT cr;
                    GetClientRect(hwnd, &cr);
                    RECT btnRc;
                    GetChatBtnRect(cr, btnRc);
                    if (pt.x >= btnRc.left && pt.x <= btnRc.right &&
                        pt.y >= btnRc.top  && pt.y <= btnRc.bottom)
                    {
                        g_chatExpanded = !g_chatExpanded;
                        if (g_chatExpanded)
                        {
                            RECT wr;
                            GetWindowRect(hwnd, &wr);
                            g_chatBaseH = wr.bottom - wr.top;
                            // chatH = base/2 so that after resize:
                            //   GetChatPanelHeight(base+chatH) = (base+chatH)/3
                            //   = (base+base/2)/3 = base/2 = chatH
                            int chatH = g_chatBaseH / 2;
                            if (chatH > 300) chatH = 300;
                            if (chatH < 125) chatH = 125;
                            SetWindowPos(hwnd, nullptr, 0, 0,
                                wr.right - wr.left,
                                g_chatBaseH + chatH,
                                SWP_NOZORDER | SWP_NOMOVE);
                            SetTimer(hwnd, kChatTimerId, kChatTimerInterval, nullptr);
                        }
                        else
                        {
                            KillTimer(hwnd, kChatTimerId);
                            RECT wr;
                            GetWindowRect(hwnd, &wr);
                            const int curH = wr.bottom - wr.top;
                            const int chatH = GetChatPanelHeight(curH);
                            const int newH = curH - chatH;
                            if (newH > 0)
                                g_chatBaseH = newH;
                            SetWindowPos(hwnd, nullptr, 0, 0,
                                wr.right - wr.left,
                                newH > 0 ? newH : g_chatBaseH,
                                SWP_NOZORDER | SWP_NOMOVE);
                        }
                        UpdateThumbnailRect(hwnd);
                        ApplyWindowStyle(hwnd);
                        InvalidateRect(hwnd, nullptr, TRUE);
                        return 0;
                    }
                }
                return DefWindowProcW(hwnd, msg, wp, lp);
            }

            case WM_MOUSEWHEEL:
            {
                if (!g_chatExpanded) break;
                const int delta = GET_WHEEL_DELTA_WPARAM(wp);
                g_chatScrollOff += (delta > 0) ? 20 : -20;
                if (g_chatScrollOff < 0) g_chatScrollOff = 0;
                RECT cr, panelRc;
                GetClientRect(hwnd, &cr);
                GetChatPanelRect(cr, panelRc);
                InvalidateRect(hwnd, &panelRc, FALSE);
                return 0;
            }

            case WM_TIMER:
                if (wp == kChatTimerId)
                {
                    // Keep accent colors + DWM border in sync
                    static bool     s_lastPolished = !Polished();
                    static COLORREF s_lastMain = 0;
                    COLORREF curMain = ImVecToColorref(MAIN_RED);
                    if (s_lastPolished != Polished() || s_lastMain != curMain)
                    {
                        g_colors.bg = Polished() ? RGB(22, 20, 28) : RGB(18, 12, 28);
                        g_colors.main = ImVecToColorref(MAIN_RED);
                        g_colors.mid  = ImVecToColorref(MID_RED);
                        g_colors.dark = ImVecToColorref(DARK_RED);
                        ApplyWindowStyle(hwnd);
                        InvalidateRect(hwnd, nullptr, FALSE);
                        s_lastPolished = Polished();
                        s_lastMain = curMain;
                    }

                    RECT cr, panelRc;
                    GetClientRect(hwnd, &cr);
                    GetChatPanelRect(cr, panelRc);
                    InvalidateRect(hwnd, &panelRc, FALSE);
                    return 0;
                }
                break;

            case WM_SIZE:
                ApplyWindowStyle(hwnd);
                UpdateThumbnailRect(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;

            case WM_ERASEBKGND:
                return 1;

            case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC  hdc = BeginPaint(hwnd, &ps);
                RECT rc{};
                GetClientRect(hwnd, &rc);

                // Double-buffer to eliminate flicker
                HDC memDc = CreateCompatibleDC(hdc);
                HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
                HGDIOBJ oldBmp = SelectObject(memDc, memBmp);

                HBRUSH bgBr = CreateSolidBrush(g_colors.bg);
                FillRect(memDc, &rc, bgBr);
                DeleteObject(bgBr);

                DrawTitleBar(memDc, rc);

                int videoBottom = rc.bottom - kVideoBorder;
                if (g_chatExpanded)
                    videoBottom -= GetChatPanelHeight(rc.bottom - rc.top);

                // Accent frame around the live video.
                HBRUSH vBr = CreateSolidBrush(Polished() ? g_colors.mid : g_colors.main);
                RECT strip = { rc.left, rc.top + kTitleH,
                               rc.left + kVideoBorder, videoBottom };
                FillRect(memDc, &strip, vBr);
                strip = { rc.right - kVideoBorder, rc.top + kTitleH,
                          rc.right, videoBottom };
                FillRect(memDc, &strip, vBr);
                strip = { rc.left, videoBottom - kVideoBorder,
                          rc.right, videoBottom };
                FillRect(memDc, &strip, vBr);
                DeleteObject(vBr);

                // accent glow just inside the video frame.
                if (Polished()) {
                    COLORREF glowC = LerpColor(g_colors.bg, g_colors.main, 0.45f);
                    // left + right inner glow columns
                    RECT gl = { rc.left + kVideoBorder, rc.top + kTitleH,
                                rc.left + kVideoBorder + 2, videoBottom };
                    HGradientStrip(memDc, gl, glowC, g_colors.bg);
                    RECT gr2 = { rc.right - kVideoBorder - 2, rc.top + kTitleH,
                                 rc.right - kVideoBorder, videoBottom };
                    HGradientStrip(memDc, gr2, g_colors.bg, glowC);
                }

                DrawChatPanel(memDc, rc);

                BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDc, 0, 0, SRCCOPY);

                SelectObject(memDc, oldBmp);
                DeleteObject(memBmp);
                DeleteDC(memDc);

                EndPaint(hwnd, &ps);
                return 0;
            }

            case WM_GETMINMAXINFO:
            {
                auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
                const int minVideoW = 200;
                const int minVideoH = (g_aspectRatio > 0.f)
                    ? static_cast<int>(minVideoW / g_aspectRatio)
                    : 120;
                int extra = 0;
                if (g_chatExpanded)
                {
                    const int h = minVideoH + kTitleH + kVideoBorder + 125;
                    extra = GetChatPanelHeight(h);
                }
                mmi->ptMinTrackSize = {
                    minVideoW + kVideoBorder * 2,
                    minVideoH + kTitleH + kVideoBorder + extra
                };
                return 0;
            }

            case WM_DESTROY:
                KillTimer(hwnd, kChatTimerId);
                if (g_thumbnail)
                {
                    DwmUnregisterThumbnail(g_thumbnail);
                    g_thumbnail = nullptr;
                }
                PostQuitMessage(0);
                return 0;
            }

            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        // Background thread
        static DWORD WINAPI ThumbnailThreadProc(LPVOID)
        {
            WNDCLASSEXW wc{};
            wc.cbSize        = sizeof(wc);
            wc.lpfnWndProc   = ThumbnailWndProc;
            wc.hInstance     = LavenderHook::Globals::dll_module;
            wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
            wc.lpszClassName = kThumbnailClass;
            RegisterClassExW(&wc);

            int initW = 480;
            int initH = 290 + kTitleH;
            if (g_gameHwnd)
            {
                RECT gr{};
                GetWindowRect(g_gameHwnd, &gr);
                int qw = (gr.right  - gr.left) / 4;
                int qh = (gr.bottom - gr.top)  / 4 + kTitleH;
                initW = qw > 240           ? qw : 240;
                initH = qh > kTitleH + 120 ? qh : kTitleH + 120;
            }

            HWND hwnd = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                kThumbnailClass,
                L"LavenderHook",
                WS_POPUP,
                100, 100, initW, initH,
                nullptr, nullptr,
                LavenderHook::Globals::dll_module,
                nullptr
            );

            if (!hwnd)
                return 1;

            g_chatBaseH = initH;
            ApplyWindowStyle(hwnd);

            if (g_gameHwnd &&
                SUCCEEDED(DwmRegisterThumbnail(hwnd, g_gameHwnd, &g_thumbnail)))
            {
                UpdateThumbnailRect(hwnd);
            }

            g_overlayHwnd = hwnd;
            ShowWindow(hwnd, SW_SHOW);
            UpdateWindow(hwnd);

            // Pull foreground focus away from the hidden game window.
            SetForegroundWindow(hwnd);
            SetActiveWindow(hwnd);

            MSG m{};
            BOOL bRet;
            while ((bRet = GetMessageW(&m, nullptr, 0, 0)) != 0)
            {
                if (bRet == -1)
                {
                    DestroyWindow(hwnd);
                    break;
                }
                TranslateMessage(&m);
                DispatchMessageW(&m);
            }

            UnregisterClassW(kThumbnailClass, LavenderHook::Globals::dll_module);
            g_overlayHwnd = nullptr;
            return 0;
        }

        // API 
        void CreateProcessOverlay(HWND gameHwnd)
        {
            if (g_overlayHwnd)
                return;

            g_colors.bg   = LavenderHook::Globals::use_polished_overlay
                                ? RGB(22, 20, 28)
                                : RGB(18, 12, 28);
            g_colors.main = ImVecToColorref(MAIN_RED);
            g_colors.mid  = ImVecToColorref(MID_RED);
            g_colors.dark = ImVecToColorref(DARK_RED);

            g_aspectRatio = 0.f;
            if (gameHwnd) {
                RECT cr{};
                GetClientRect(gameHwnd, &cr);
                const int w = cr.right  - cr.left;
                const int h = cr.bottom - cr.top;
                if (w > 0 && h > 0)
                    g_aspectRatio = static_cast<float>(w) / static_cast<float>(h);
            }

            g_gameHwnd      = gameHwnd;
            g_chatExpanded  = false;
            g_chatScrollOff = 0;
            g_overlayThread = CreateThread(nullptr, 0,
                                           ThumbnailThreadProc, nullptr, 0, nullptr);
        }

        void DestroyProcessOverlay()
        {
            if (!g_overlayThread)
                return;

            for (int i = 0; i < 100 && !g_overlayHwnd; ++i)
                Sleep(10);

            if (g_overlayHwnd)
                PostMessageW(g_overlayHwnd, WM_CLOSE, 0, 0);

            WaitForSingleObject(g_overlayThread, 5000);
            CloseHandle(g_overlayThread);
            g_overlayThread = nullptr;
            g_overlayHwnd   = nullptr;
        }

    } // namespace UI
} // namespace LavenderHook
