#include "updater_core.h"
#include "../versionutils/version_utils.h"
#include "../httpclient/http_client.h"
#include "../logger/logger.h"
#include "../assets/resource.h"

#include <windows.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <sstream>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

static void set_dpi_awareness()
{
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser)
    {
        typedef BOOL (WINAPI *SDPIA_fn)(DPI_AWARENESS_CONTEXT);
        SDPIA_fn fn = (SDPIA_fn)GetProcAddress(hUser, "SetProcessDpiAwarenessContext");
        if (fn)
        {
            fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            return;
        }
    }
    HMODULE hShcore = LoadLibraryW(L"shcore.dll");
    if (hShcore)
    {
        typedef HRESULT (WINAPI *SDPA_fn)(int);
        SDPA_fn fn = (SDPA_fn)GetProcAddress(hShcore, "SetProcessDpiAwareness");
        if (fn)
        {
            fn(2); // PROCESS_PER_MONITOR_DPI_AWARE
        }
        FreeLibrary(hShcore);
        return;
    }
    SetProcessDPIAware();
}

static int get_dpi()
{
    HMODULE hShcore = LoadLibraryW(L"shcore.dll");
    if (hShcore)
    {
        typedef HRESULT (WINAPI *GDFM_fn)(HMONITOR, int, UINT*, UINT*);
        GDFM_fn fn = (GDFM_fn)GetProcAddress(hShcore, "GetDpiForMonitor");
        if (fn)
        {
            POINT pt = { 1, 1 };
            HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
            UINT dpiX = 96, dpiY = 96;
            if (SUCCEEDED(fn(hMon, 0, &dpiX, &dpiY)))
            {
                FreeLibrary(hShcore);
                return dpiX;
            }
        }
        FreeLibrary(hShcore);
    }
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop\\WindowMetrics",
        0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD dpi = 0;
        DWORD size = sizeof(dpi);
        RegQueryValueExW(hKey, L"AppliedDPI", NULL, NULL, (LPBYTE)&dpi, &size);
        RegCloseKey(hKey);
        if (dpi >= 96)
            return dpi;
    }
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);
    return dpi >= 96 ? dpi : 96;
}

static float dpi_scale_factor()
{
    return get_dpi() / 96.0f;
}

static int Scale(int val)
{
    return static_cast<int>(val * dpi_scale_factor() + 0.5f);
}

static int FontHeight(int pt)
{
    return MulDiv(pt, get_dpi(), 72);
}

// Theme 
static const COLORREF COL_BG        = RGB(0x1E, 0x1E, 0x2E);
static const COLORREF COL_ACCENT    = RGB(0x7C, 0x3A, 0xED);
static const COLORREF COL_TEXT_MAIN = RGB(0xFF, 0xFF, 0xFF);
static const COLORREF COL_TEXT_SUB  = RGB(0x8B, 0xA0, 0xC4);
static const COLORREF COL_BTN_BG    = RGB(0x2D, 0x2D, 0x42);
static const COLORREF COL_BTN_ACT   = RGB(0x6A, 0x2E, 0xD4);

#define IDC_BTN_YES    101
#define IDC_BTN_NO     102
#define IDC_CHK_REMIND 103
#define IDC_LBL_HEADER 104
#define IDC_LBL_MSG    105

struct UpdateDlgData
{
    std::string remoteVersion;
    std::string localVersion;
    int         result;
    bool        dontRemind;
    bool        chkChecked;
    HBRUSH      hBrushBg;
    HFONT       hFontTitle;
    HFONT       hFontBody;
    int         currentDpi;
    HWND        hHdr;
    HWND        hMsg;
    HWND        hChk;
    HWND        hYes;
    HWND        hNo;
};

// helpers
static std::wstring get_module_dir()
{
    wchar_t path[MAX_PATH];
    HMODULE hm = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&get_module_dir), &hm);
    if (hm)
    {
        DWORD len = GetModuleFileNameW(hm, path, MAX_PATH);
        if (len > 0 && len < MAX_PATH)
            return std::filesystem::path(path).parent_path().wstring();
    }
    return std::filesystem::current_path().wstring();
}


static void self_unload_and_exit()
{
    HMODULE hSelf = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&self_unload_and_exit), &hSelf);
    if (hSelf)
        FreeLibraryAndExitThread(hSelf, 0);
    ExitThread(0);
}

// appdata / ini
static std::filesystem::path get_appdata_dir()
{
    wchar_t *p = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &p)))
    {
        std::filesystem::path result(p);
        CoTaskMemFree(p);
        return result / L"LavenderHook";
    }

    wchar_t env[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"APPDATA", env, MAX_PATH) > 0)
        return std::filesystem::path(env) / L"LavenderHook";
    return std::filesystem::temp_directory_path() / L"LavenderHook";
}

static std::filesystem::path get_ini_path()
{
    return get_appdata_dir() / L"Updater.ini";
}

static std::wstring read_skip_version()
{
    wchar_t buf[64] = {};
    GetPrivateProfileStringW(L"Version", L"SkipVersion", L"",
        buf, (DWORD)std::size(buf), get_ini_path().wstring().c_str());
    return buf;
}

static void write_skip_version(const std::wstring &ver)
{
    std::filesystem::path ini = get_ini_path();
    std::error_code ec;
    std::filesystem::create_directories(ini.parent_path(), ec);
    WritePrivateProfileStringW(L"Version", L"SkipVersion",
        ver.c_str(), ini.wstring().c_str());
}

// download
static bool download_file(const std::wstring &url, const std::filesystem::path &dest)
{
    std::string data;
    if (!fetch_url(url, data) || data.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(dest.parent_path(), ec);
    std::ofstream out(dest, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out.write(data.data(), (std::streamsize)data.size());
    return out.good();
}

// load hook
static void load_lavenderhook(const std::filesystem::path &dllPath)
{
    HMODULE h = LoadLibraryW(dllPath.wstring().c_str());
    if (h)
        log_message("RunUpdater: LavenderHook.dll loaded successfully");
    else
    {
        std::ostringstream oss;
        oss << "RunUpdater: LoadLibrary failed, error=" << GetLastError();
        log_message(oss.str());
    }
}

// icon loader
static HICON load_png_icon(HINSTANCE hInst, int resId, int sz)
{
    HRSRC   hRes  = FindResourceW(hInst, MAKEINTRESOURCEW(resId), L"PNG");
    if (!hRes) return NULL;
    HGLOBAL hGlob = LoadResource(hInst, hRes);
    if (!hGlob) return NULL;
    LPVOID  pData = LockResource(hGlob);
    DWORD   size  = SizeofResource(hInst, hRes);
    if (!pData || !size) return NULL;

    // Copy into a moveable block
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hMem) return NULL;
    void *pMem = GlobalLock(hMem);
    if (!pMem) { GlobalFree(hMem); return NULL; }
    memcpy(pMem, pData, size);
    GlobalUnlock(hMem);

    IStream *pStream = NULL;
    if (FAILED(CreateStreamOnHGlobal(hMem, TRUE /*fDeleteOnRelease*/, &pStream)))
    {
        GlobalFree(hMem);
        return NULL;
    }

    Gdiplus::Bitmap *bmp = Gdiplus::Bitmap::FromStream(pStream);
    pStream->Release(); // also frees hMem

    if (!bmp || bmp->GetLastStatus() != Gdiplus::Ok)
    {
        delete bmp;
        return NULL;
    }

    HICON hIcon = NULL;
    if (bmp->GetWidth() != (UINT)sz || bmp->GetHeight() != (UINT)sz)
    {
        // Scale to the requested size
        Gdiplus::Bitmap scaled(sz, sz, PixelFormat32bppARGB);
        Gdiplus::Graphics g(&scaled);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(bmp, 0, 0, sz, sz);
        scaled.GetHICON(&hIcon);
    }
    else
    {
        bmp->GetHICON(&hIcon);
    }

    delete bmp;
    return hIcon;
}

// dialog
static void draw_button(LPDRAWITEMSTRUCT dis)
{
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    HBRUSH hBr = CreateSolidBrush(pressed ? COL_BTN_ACT : COL_BTN_BG);
    FillRect(dis->hDC, &dis->rcItem, hBr);
    DeleteObject(hBr);

    HPEN   hPen  = CreatePen(PS_SOLID, 1, COL_ACCENT);
    HPEN   hOldP = (HPEN)SelectObject(dis->hDC, hPen);
    HBRUSH hNullB = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH hOldB  = (HBRUSH)SelectObject(dis->hDC, hNullB);
    Rectangle(dis->hDC,
        dis->rcItem.left, dis->rcItem.top,
        dis->rcItem.right, dis->rcItem.bottom);
    SelectObject(dis->hDC, hOldP);
    SelectObject(dis->hDC, hOldB);
    DeleteObject(hPen);

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, COL_TEXT_MAIN);
    wchar_t buf[64] = {};
    GetWindowTextW(dis->hwndItem, buf, 64);
    DrawTextW(dis->hDC, buf, -1, &dis->rcItem,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void draw_checkbox(LPDRAWITEMSTRUCT dis, bool checked)
{
    RECT rc = dis->rcItem;

    // Background
    HBRUSH hBrBg = CreateSolidBrush(COL_BG);
    FillRect(dis->hDC, &rc, hBrBg);
    DeleteObject(hBrBg);

    // 13x13 box, vertically centred in the control rect
    const int BOX = Scale(13);
    int bx = rc.left;
    int by = rc.top + (rc.bottom - rc.top - BOX) / 2;

    HPEN   hPen  = CreatePen(PS_SOLID, 1, COL_ACCENT);
    HPEN   hOldP = (HPEN)SelectObject(dis->hDC, hPen);
    HBRUSH hFill = CreateSolidBrush(checked ? COL_ACCENT : COL_BTN_BG);
    HBRUSH hOldB = (HBRUSH)SelectObject(dis->hDC, hFill);
    Rectangle(dis->hDC, bx, by, bx + BOX, by + BOX);
    SelectObject(dis->hDC, hOldP);
    SelectObject(dis->hDC, hOldB);
    DeleteObject(hPen);
    DeleteObject(hFill);

    // White tick when checked
    if (checked)
    {
        HPEN hTick = CreatePen(PS_SOLID, 2, COL_TEXT_MAIN);
        HPEN hOldT = (HPEN)SelectObject(dis->hDC, hTick);
        MoveToEx(dis->hDC, bx + 2,  by + 7,  NULL);
        LineTo  (dis->hDC, bx + 5,  by + 10);
        LineTo  (dis->hDC, bx + 11, by + 3);
        SelectObject(dis->hDC, hOldT);
        DeleteObject(hTick);
    }

    // White label to the right of the box
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, COL_TEXT_MAIN);
    wchar_t lbl[128] = {};
    GetWindowTextW(dis->hwndItem, lbl, 128);
    RECT textRc = { bx + BOX + 6, rc.top, rc.right, rc.bottom };
    DrawTextW(dis->hDC, lbl, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static LRESULT CALLBACK UpdateDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UpdateDlgData *d = reinterpret_cast<UpdateDlgData*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCTW *cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        d = reinterpret_cast<UpdateDlgData*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d));

        d->currentDpi = get_dpi();
        d->hBrushBg   = CreateSolidBrush(COL_BG);
        d->hFontTitle = CreateFontW(FontHeight(15), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        d->hFontBody  = CreateFontW(FontHeight(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        d->hHdr = CreateWindowExW(0, L"STATIC", L"Update Available",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            Scale(15), Scale(12), Scale(330), Scale(22), hwnd, (HMENU)IDC_LBL_HEADER, NULL, NULL);
        SendMessageW(d->hHdr, WM_SETFONT, (WPARAM)d->hFontTitle, TRUE);

        std::wstring lv(d->localVersion.begin(), d->localVersion.end());
        std::wstring rv(d->remoteVersion.begin(), d->remoteVersion.end());
        std::wstring msgTxt = lv + L" \u2192 " + rv +
            L"\nWould you like to update LavenderHook?";
        d->hMsg = CreateWindowExW(0, L"STATIC", msgTxt.c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            Scale(15), Scale(40), Scale(330), Scale(44), hwnd, (HMENU)IDC_LBL_MSG, NULL, NULL);
        SendMessageW(d->hMsg, WM_SETFONT, (WPARAM)d->hFontBody, TRUE);

        d->hChk = CreateWindowExW(0, L"BUTTON",
            L"Don\u2019t remind me again for this version",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            Scale(15), Scale(94), Scale(330), Scale(20), hwnd, (HMENU)IDC_CHK_REMIND, NULL, NULL);
        SendMessageW(d->hChk, WM_SETFONT, (WPARAM)d->hFontBody, TRUE);

        d->hYes = CreateWindowExW(0, L"BUTTON", L"Yes",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            Scale(15), Scale(124), Scale(80), Scale(28), hwnd, (HMENU)IDC_BTN_YES, NULL, NULL);
        SendMessageW(d->hYes, WM_SETFONT, (WPARAM)d->hFontBody, TRUE);

        d->hNo = CreateWindowExW(0, L"BUTTON", L"No",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            Scale(107), Scale(124), Scale(80), Scale(28), hwnd, (HMENU)IDC_BTN_NO, NULL, NULL);
        SendMessageW(d->hNo, WM_SETFONT, (WPARAM)d->hFontBody, TRUE);

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, d ? d->hBrushBg : (HBRUSH)GetStockObject(BLACK_BRUSH));
        RECT strip = { 0, 0, rc.right, 3 };
        HBRUSH hAcc = CreateSolidBrush(COL_ACCENT);
        FillRect(hdc, &strip, hAcc);
        DeleteObject(hAcc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        if (d)
        {
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect((HDC)wParam, &rc, d->hBrushBg);
            return 1;
        }
        break;

    case WM_CTLCOLORSTATIC:
    {
        if (!d) break;
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetBkColor(hdc, COL_BG);
        SetTextColor(hdc,
            GetDlgCtrlID((HWND)lParam) == IDC_LBL_HEADER
                ? COL_TEXT_MAIN : COL_TEXT_SUB);
        return (LRESULT)d->hBrushBg;
    }

    case WM_CTLCOLORBTN:
    {
        if (!d) break;
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetBkColor(hdc, COL_BG);
        SetTextColor(hdc, COL_TEXT_SUB);
        return (LRESULT)d->hBrushBg;
    }

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlType == ODT_BUTTON)
        {
            if (dis->CtlID == IDC_CHK_REMIND)
                draw_checkbox(dis, d ? d->chkChecked : false);
            else
                draw_button(dis);
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        int  id   = LOWORD(wParam);
        int  code = HIWORD(wParam);
        if (id == IDC_CHK_REMIND && code == BN_CLICKED)
        {
            if (d)
            {
                d->chkChecked = !d->chkChecked;
                HWND hChk = GetDlgItem(hwnd, IDC_CHK_REMIND);
                InvalidateRect(hChk, NULL, TRUE);
                UpdateWindow(hChk);
            }
        }
        else if (id == IDC_BTN_YES || id == IDC_BTN_NO)
        {
            if (d)
            {
                d->dontRemind = d->chkChecked;
                d->result = (id == IDC_BTN_YES) ? 1 : 0;
            }
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_DPICHANGED:
    {
        int newDpi = LOWORD(wParam);
        if (d && newDpi != d->currentDpi)
        {
            d->currentDpi = newDpi;
            if (d->hFontTitle) { DeleteObject(d->hFontTitle); d->hFontTitle = NULL; }
            if (d->hFontBody)  { DeleteObject(d->hFontBody);  d->hFontBody  = NULL; }
            d->hFontTitle = CreateFontW(MulDiv(15, newDpi, 72), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            d->hFontBody  = CreateFontW(MulDiv(12, newDpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

            auto S = [newDpi](int v) { return MulDiv(v, newDpi, 96); };

            SetWindowPos(d->hHdr, NULL, S(15), S(12), S(330), S(22), SWP_NOZORDER);
            SendMessageW(d->hHdr, WM_SETFONT, (WPARAM)d->hFontTitle, TRUE);
            SetWindowPos(d->hMsg, NULL, S(15), S(40), S(330), S(44), SWP_NOZORDER);
            SendMessageW(d->hMsg, WM_SETFONT, (WPARAM)d->hFontBody, TRUE);
            SetWindowPos(d->hChk, NULL, S(15), S(94), S(330), S(20), SWP_NOZORDER);
            SendMessageW(d->hChk, WM_SETFONT, (WPARAM)d->hFontBody, TRUE);
            SetWindowPos(d->hYes, NULL, S(15), S(124), S(80), S(28), SWP_NOZORDER);
            SendMessageW(d->hYes, WM_SETFONT, (WPARAM)d->hFontBody, TRUE);
            SetWindowPos(d->hNo, NULL, S(107), S(124), S(80), S(28), SWP_NOZORDER);
            SendMessageW(d->hNo, WM_SETFONT, (WPARAM)d->hFontBody, TRUE);

            RECT *suggested = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hwnd, NULL,
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_RETURN)
        {
            if (d)
            {
                d->dontRemind = d->chkChecked;
                d->result = 1;
            }
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        if (d) { d->result = 0; d->dontRemind = false; }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (d)
        {
            if (d->hBrushBg)   { DeleteObject(d->hBrushBg);   d->hBrushBg   = NULL; }
            if (d->hFontTitle) { DeleteObject(d->hFontTitle); d->hFontTitle = NULL; }
            if (d->hFontBody)  { DeleteObject(d->hFontBody);  d->hFontBody  = NULL; }
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void show_update_dialog(UpdateDlgData &data)
{
    HMODULE hMod = NULL;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&show_update_dialog), &hMod);
    HINSTANCE hInst = (HINSTANCE)hMod;

    Gdiplus::GdiplusStartupInput gdipInput = {};
    ULONG_PTR gdipToken = 0;
    Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, NULL);

    HICON hIconSm = load_png_icon(hInst, IDR_MENU_LOGO, 16);
    HICON hIconBig = load_png_icon(hInst, IDR_MENU_LOGO, 32);

    static const wchar_t WNDCLS[] = L"LavenderUpdaterDlg_v1";
    WNDCLASSEXW wc  = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = UpdateDlgProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = WNDCLS;
    wc.hCursor       = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = NULL;
    wc.hIcon         = hIconBig;
    wc.hIconSm       = hIconSm;
    ATOM atom = RegisterClassExW(&wc);

    const int WW = Scale(370), WH = Scale(200);
    int x = (GetSystemMetrics(SM_CXSCREEN) - WW) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - WH) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_APPWINDOW,
        WNDCLS, L"LavenderHook",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, WW, WH, NULL, NULL, hInst, &data);

    if (!hwnd)
    {
        log_message("show_update_dialog: CreateWindowExW failed");
        if (atom)     UnregisterClassW(WNDCLS, hInst);
        if (hIconSm)  DestroyIcon(hIconSm);
        if (hIconBig) DestroyIcon(hIconBig);
        if (gdipToken) Gdiplus::GdiplusShutdown(gdipToken);
        return;
    }

    // Override per-window icons
    if (hIconSm)  SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);
    if (hIconBig) SendMessageW(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetFocus(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (atom)      UnregisterClassW(WNDCLS, hInst);
    if (hIconSm)   DestroyIcon(hIconSm);
    if (hIconBig)  DestroyIcon(hIconBig);
    if (gdipToken) Gdiplus::GdiplusShutdown(gdipToken);
}

// RunUpdater
extern "C" __declspec(dllexport) void RunUpdater()
{
    set_dpi_awareness();
    log_message("RunUpdater: starting");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    std::wstring moduleDir = get_module_dir();
    std::filesystem::path dllPath =
        std::filesystem::path(moduleDir) / L"LavenderHook" / L"LavenderHook.dll";

    log_message("RunUpdater: moduleDir=" + std::filesystem::path(moduleDir).string());
    log_message("RunUpdater: dllPath="   + dllPath.string());

    bool found = false;
    for (int i = 0; i < 20; ++i)
    {
        bool exists = std::filesystem::exists(dllPath);
        std::ostringstream oss;
        oss << "RunUpdater: scan attempt " << (i + 1)
            << ": exists=" << (exists ? "yes" : "no");
        log_message(oss.str());
        if (exists) { found = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    if (!found) log_message("RunUpdater: DLL not found after waiting");

    std::string localVersion = get_local_dll_version(dllPath);
    log_message(localVersion.empty()
        ? "RunUpdater: local version unavailable"
        : "RunUpdater: local version=" + localVersion);

    std::string remoteText;
    bool fetched = fetch_url(
        L"https://raw.githubusercontent.com/CoolGamer2000-dda/LavenderHook/refs/heads/main/HookVersion",
        remoteText);
    log_message("RunUpdater: fetch_url=" + std::string(fetched ? "ok" : "fail"));

    auto trim = [](const std::string &s) -> std::string {
        auto a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return {};
        auto b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    };
    std::string remoteVersion = fetched ? trim(remoteText) : std::string();
    log_message(fetched
        ? "RunUpdater: remote version=" + remoteVersion
        : "RunUpdater: could not get remote version");

    // Can't compare
    if (localVersion.empty() || !fetched)
    {
        log_message("RunUpdater: missing data, loading existing DLL");
        if (found) load_lavenderhook(dllPath);
        self_unload_and_exit();
        return;
    }

    // Compare versions numerically so a newer local build doesn't trigger a spurious update
    int cmp = compare_version_strings(localVersion, remoteVersion);
    if (cmp >= 0)
    {
        log_message("RunUpdater: local version (" + localVersion + ") >= remote (" + remoteVersion + ") — loading LavenderHook.dll");
        load_lavenderhook(dllPath);
        self_unload_and_exit();
        return;
    }

    // Remote is newer
    log_message("RunUpdater: UPDATE available local=" + localVersion + " remote=" + remoteVersion);

    std::wstring remoteVersionW(remoteVersion.begin(), remoteVersion.end());
    std::wstring skipVersion = read_skip_version();
    if (!skipVersion.empty() && skipVersion == remoteVersionW)
    {
        log_message("RunUpdater: dont-remind active for " + remoteVersion + " — loading existing DLL");
        load_lavenderhook(dllPath);
        self_unload_and_exit();
        return;
    }

    
    UpdateDlgData dlgData = {};
    dlgData.remoteVersion = remoteVersion;
    dlgData.localVersion  = localVersion;
    dlgData.result        = 0;
    dlgData.dontRemind    = false;

    show_update_dialog(dlgData);

    log_message("RunUpdater: dialog closed result=" + std::to_string(dlgData.result) +
                " dontRemind=" + (dlgData.dontRemind ? "yes" : "no"));

    if (dlgData.dontRemind)
    {
        write_skip_version(remoteVersionW);
        log_message("RunUpdater: wrote dont-remind for " + remoteVersion);
    }

    if (dlgData.result == 1)
    {
        // Yes
        std::filesystem::path backupPath(dllPath.wstring() + L".bak");
        std::error_code ec;
        std::filesystem::rename(dllPath, backupPath, ec);
        bool hasBackup = !ec;
        log_message(hasBackup ? "RunUpdater: old DLL backed up"
                              : "RunUpdater: backup failed");

        log_message("RunUpdater: downloading new LavenderHook.dll");
        bool ok = download_file(
            L"https://github.com/CoolGamer2000-dda/LavenderHook/releases/latest/download/LavenderHook.dll",
            dllPath);
        log_message(ok ? "RunUpdater: download ok" : "RunUpdater: download failed");

        if (ok)
        {
            if (hasBackup) std::filesystem::remove(backupPath, ec);
            load_lavenderhook(dllPath);
        }
        else
        {
            // Download failed
            if (hasBackup)
            {
                std::filesystem::rename(backupPath, dllPath, ec);
                log_message(ec ? "RunUpdater: restore failed" : "RunUpdater: backup restored");
            }
            if (std::filesystem::exists(dllPath))
                load_lavenderhook(dllPath);
        }
    }
    else
    {
        // No
        log_message("RunUpdater: user declined — loading existing LavenderHook.dll");
        load_lavenderhook(dllPath);
    }

    self_unload_and_exit();
}
