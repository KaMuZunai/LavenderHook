#include "Direct11.h"
#include "../hooks.h"

#include "../../imgui/imgui.h"
#include "../../imgui/imgui_impl_win32.h"
#include "../../imgui/imgui_impl_dx11.h"

#include "../../assets/TextureLoader.h"
#include "../../misc/Globals.h"
#include "../../ui/GUI.h"
#include "../../ui/UIRegister.h"
#include "../../ui/UiDispatch.h"
#include "../../ui/components/console.h"

#include "../../windows/HoldToKillButton.h"
#include "../../windows/ToggleMenuButton.h"
#include "../../windows/TravelToMenuButton.h"

#include "../../input/Hotkeys.h"

#include "../../minhook/MinHook.h"

#include <sstream>
#include <iomanip>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <windowsx.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

extern GUI* gui;

// Prototypes
HRESULT __stdcall HookedPresent(IDXGISwapChain* swap, UINT sync, UINT flags);
HRESULT __stdcall HookedResizeBuffers(
    IDXGISwapChain* swap,
    UINT BufferCount, UINT Width, UINT Height,
    DXGI_FORMAT NewFormat, UINT SwapChainFlags);

// External registry setup
void RegisterUIWindows();

namespace {

    // DX11 globals
    HWND                     g_hwnd = nullptr;
    ID3D11Device* g_device = nullptr;
    ID3D11DeviceContext* g_context = nullptr;
    ID3D11RenderTargetView* g_rtv = nullptr;

    // Hook state + retry management
    static std::atomic<bool> g_hooked{ false };
    static std::atomic<bool> g_retryScheduled{ false };
    static std::atomic<bool> g_retryStop{ false };
    static HANDLE            g_retryThread = nullptr;

    // Cursor helpers
    void ForceOSCursorVisible(bool wantVisible)
    {
        static bool last = !wantVisible;
        if (wantVisible == last) return;
        last = wantVisible;

        CURSORINFO ci{ sizeof(ci) };
        if (!GetCursorInfo(&ci)) return;

        const bool isVisible = (ci.flags & CURSOR_SHOWING) != 0;
        if (wantVisible && !isVisible)
            while (ShowCursor(TRUE) < 0);
        else if (!wantVisible && isVisible)
            while (ShowCursor(FALSE) >= 0);
    }

    void DrawTriangleCursor()
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 mp = ImGui::GetIO().MousePos;
        if (!LavenderHook::Globals::show_menu && g_hwnd)
        {
            POINT p;
            if (GetCursorPos(&p)) {
                if (ScreenToClient(g_hwnd, &p)) {
                    mp = ImVec2((float)p.x, (float)p.y);
                }
            }
        }

        ImVec2 a = { floorf(mp.x) + 0.5f, floorf(mp.y) + 0.5f };
        ImVec2 b = { a.x + 22.0f, a.y + 8.0f };
        ImVec2 c = { a.x + 8.0f, a.y + 22.0f };

        dl->AddTriangleFilled(a, b, c, IM_COL32(255, 255, 255, 255));
        dl->AddTriangle(a, b, c, IM_COL32(0, 0, 0, 255), 2.5f);
    }

    // RTV helpers
    void ReleaseRTV()
    {
        if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
    }

    void CreateRTV(IDXGISwapChain* swap)
    {
        ReleaseRTV();
        ID3D11Texture2D* back = nullptr;
        if (SUCCEEDED(swap->GetBuffer(0, IID_PPV_ARGS(&back))) && back && g_device)
        {
            g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
            back->Release();
        }
        else
        {
            if (back) back->Release();
        }
    }

    // Dummy window for vtable extraction
    static LRESULT CALLBACK DummyWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    static HWND CreateHiddenTempWindow()
    {
        static ATOM s_atom = 0;
        static const wchar_t* kClass = L"LavenderHookDX11_TmpWin";
        HINSTANCE hInst = (HINSTANCE)GetModuleHandle(nullptr);

        if (!s_atom)
        {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = DummyWndProc;
            wc.hInstance = hInst;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.lpszClassName = kClass;
            s_atom = RegisterClassExW(&wc);
        }

        HWND hwnd = CreateWindowExW(
            0, kClass, L"", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 64, 64,
            nullptr, nullptr, hInst, nullptr
        );

        if (hwnd) ShowWindow(hwnd, SW_HIDE);
        return hwnd;
    }

    // ImGui bootstrap
    void EnsureImGui(IDXGISwapChain* swap)
    {
        static bool ui_registered = false;

        if (ImGui::GetCurrentContext())
            return;

        if (!g_device || !g_context)
        {
            if (FAILED(swap->GetDevice(IID_PPV_ARGS(&g_device))))
                return;
            g_device->GetImmediateContext(&g_context);
        }

        static bool texture_loader_initialized = false;
        if (!texture_loader_initialized)
        {
            TextureLoader::Initialize(
                GraphicsBackend::DirectX11,
                g_device
            );
            texture_loader_initialized = true;
        }

        if (ImGui::GetCurrentContext())
            return;

        DXGI_SWAP_CHAIN_DESC sd{};
        swap->GetDesc(&sd);
        g_hwnd = sd.OutputWindow;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX11_Init(g_device, g_context);

        CreateRTV(swap);

        if (!gui)
            gui = new GUI();

        // WndProc hook
        LavenderHook::Hooks::WndProc::Hook();

        // Register windows once
        if (!ui_registered)
        {
            RegisterUIWindows();
            ui_registered = true;
        }
    }

    void BeginFrame()
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void EndFrame()
    {
        ImGui::Render();
        if (g_context && g_rtv)
            g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void RenderUIFrame()
    {
        BeginFrame();

        LavenderHook::Input::Update();
        LavenderHook::UI::PlayAll();

        if (gui)
            gui->RenderOverlay();

        // Registry-driven windows
        UIRegistry::Get().Update();
        UIRegistry::Get().Render();

        LavenderHook::UI::Widgets::RenderMenuSelectorButton(LavenderHook::Globals::show_menu);
        LavenderHook::UI::Widgets::RenderTravelToMenuButton(LavenderHook::Globals::show_menu);
        LavenderHook::UI::Widgets::RenderHoldToKillButton(LavenderHook::Globals::show_menu);

        if (LavenderHook::Globals::show_menu && gui)
            gui->Render();

        // Cursor policy
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        io.MouseDrawCursor = false;

        ForceOSCursorVisible(false);
        // draw triangle cursor when menu visible or when user requested it persistently
        if (LavenderHook::Globals::show_menu || LavenderHook::Globals::show_triangle_when_menu_hidden)
            DrawTriangleCursor();

        EndFrame();
    }

    bool TryHookOnce()
    {
        const auto mh = MH_Initialize();
        if (mh != MH_OK && mh != MH_ERROR_ALREADY_INITIALIZED)
        {
            LavenderConsole::GetInstance().Log("DX11: MH_Initialize failed.");
            return false;
        }

        HWND tmpHwnd = CreateHiddenTempWindow();
        if (!tmpHwnd)
        {
            LavenderConsole::GetInstance().Log("DX11: Failed to create hidden temp window.");
            return false;
        }

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = tmpHwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* tmpSwap = nullptr;
        ID3D11Device* tmpDev = nullptr;
        ID3D11DeviceContext* tmpCtx = nullptr;
        D3D_FEATURE_LEVEL fl{};

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &sd,
            &tmpSwap,
            &tmpDev,
            &fl,
            &tmpCtx
        );

        if (FAILED(hr) || !tmpSwap)
        {
            if (tmpCtx) tmpCtx->Release();
            if (tmpDev) tmpDev->Release();
            if (tmpSwap) tmpSwap->Release();
            DestroyWindow(tmpHwnd);

            std::ostringstream oss;
            oss << "DX11: D3D11CreateDeviceAndSwapChain failed (hr=0x"
                << std::hex << std::uppercase << (unsigned long)hr << ")";
            LavenderConsole::GetInstance().Log(oss.str());
            return false;
        }

        void** vtbl = *(void***)tmpSwap;
        auto present_ptr = (LavenderHook::Hooks::Present11::Present_t)vtbl[8];
        auto resize_ptr = (LavenderHook::Hooks::Present11::ResizeBuffers_t)vtbl[13];

        tmpCtx->Release();
        tmpDev->Release();
        tmpSwap->Release();
        DestroyWindow(tmpHwnd);

        using namespace LavenderHook::Hooks::Present11;

        if (MH_CreateHook((LPVOID)present_ptr, &HookedPresent, (LPVOID*)&original_present) != MH_OK)
        {
            LavenderConsole::GetInstance().Log("DX11: MH_CreateHook(Present) failed.");
            return false;
        }

        if (MH_CreateHook((LPVOID)resize_ptr, &HookedResizeBuffers, (LPVOID*)&original_resizebuffers) != MH_OK)
        {
            LavenderConsole::GetInstance().Log("DX11: MH_CreateHook(ResizeBuffers) failed.");
            return false;
        }

        if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
        {
            LavenderConsole::GetInstance().Log("DX11: MH_EnableHook failed.");
            return false;
        }

        return true;
    }

    DWORD WINAPI RetryThreadProc(LPVOID)
    {
        while (!g_retryStop.load() && !g_hooked.load())
        {
            Sleep(5000);
            if (g_retryStop.load() || g_hooked.load())
                break;

            LavenderConsole::GetInstance().Log("DX11: Retrying hook...");
            if (TryHookOnce())
            {
                g_hooked.store(true);
                LavenderConsole::GetInstance().Log("DX11: Hook succeeded on retry.");
                break;
            }
        }

        g_retryScheduled.store(false);
        return 0;
    }

} // namespace

// function pointers
LavenderHook::Hooks::Present11::Present_t
LavenderHook::Hooks::Present11::original_present = nullptr;

LavenderHook::Hooks::Present11::ResizeBuffers_t
LavenderHook::Hooks::Present11::original_resizebuffers = nullptr;

// Hooked Present
HRESULT __stdcall HookedPresent(IDXGISwapChain* swap, UINT sync, UINT flags)
{
    static bool once = false;
    if (!once)
    {
        LavenderConsole::GetInstance().Log("DX11: Present active.");
        LavenderHook::Hooks::g_activeRenderer = LavenderHook::Hooks::RendererType::DX11;
        once = true;
    }

    EnsureImGui(swap);

    if (!g_rtv)
        CreateRTV(swap);

    if (ImGui::GetCurrentContext())
        RenderUIFrame();

    return LavenderHook::Hooks::Present11::original_present(swap, sync, flags);
}

// Hooked ResizeBuffers
HRESULT __stdcall HookedResizeBuffers(
    IDXGISwapChain* swap,
    UINT BufferCount, UINT Width, UINT Height,
    DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        ReleaseRTV();
    }

    auto hr = LavenderHook::Hooks::Present11::original_resizebuffers(
        swap, BufferCount, Width, Height, NewFormat, SwapChainFlags);

    if (SUCCEEDED(hr) && ImGui::GetCurrentContext())
    {
        CreateRTV(swap);
        ImGui_ImplDX11_CreateDeviceObjects();
    }

    return hr;
}

bool LavenderHook::Hooks::Present11::Hook()
{
    if (g_hooked.load())
        return true;

    if (TryHookOnce())
    {
        g_hooked.store(true);
        return true;
    }

    LavenderConsole::GetInstance().Log(
        "Failed to hook DX11 Present/ResizeBuffers. Will retry in 5 seconds."
    );

    if (!g_retryScheduled.exchange(true))
    {
        g_retryStop.store(false);
        g_retryThread = CreateThread(nullptr, 0, RetryThreadProc, nullptr, 0, nullptr);
        if (!g_retryThread)
        {
            g_retryScheduled.store(false);
            LavenderConsole::GetInstance().Log("DX11: Failed to spawn retry thread.");
        }
    }

    return false;
}

void LavenderHook::Hooks::Present11::Unhook()
{
    g_retryStop.store(true);
    if (g_retryThread)
    {
        WaitForSingleObject(g_retryThread, 100);
        CloseHandle(g_retryThread);
        g_retryThread = nullptr;
        g_retryScheduled.store(false);
    }

    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    ReleaseRTV();

    if (g_context) { g_context->Release(); g_context = nullptr; }
    if (g_device) { g_device->Release();  g_device = nullptr; }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    g_hooked.store(false);

    LavenderConsole::GetInstance().Log("DX11: Unhook complete.");
}
