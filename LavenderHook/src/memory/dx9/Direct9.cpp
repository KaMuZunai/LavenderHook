#include "../hooks.h"

#include "../../imgui/imgui.h"
#include "../../imgui/imgui_impl_win32.h"
#include "../../imgui/imgui_impl_dx9.h"

#include "../../misc/Globals.h"
#include "../../misc/FileLog.h"
#include "../../ui/GUI.h"
#include "../../ui/UIRegister.h"
#include "../../ui/UiDispatch.h"
#include "../../ui/UIWindows/console.h"
#include "../../assets/TextureLoader.h"
#include "../../assets/TextureCapture.h"

#include "../../ui/UIWindows/HoldToKillButton.h"
#include "../../ui/UIWindows/ToggleMenuButton.h"

#include "../../input/Hotkeys.h"

#include "../../minhook/MinHook.h"

#include <d3d9.h>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <cmath>

#pragma comment(lib, "d3d9.lib")

extern GUI* gui;

void RegisterUIWindows();

namespace LavenderHook::Hooks::EndScene9
{
    using EndScene_t = HRESULT(APIENTRY*)(LPDIRECT3DDEVICE9);
    using Reset_t = HRESULT(APIENTRY*)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);

    static EndScene_t original_endscene = nullptr;
    static Reset_t    original_reset = nullptr;

    static std::atomic<bool> g_initialized{ false };
    static HWND g_hwnd = nullptr;

    static void LogHr(const char* prefix, HRESULT hr)
    {
        std::ostringstream oss;
        oss << prefix << " (hr=0x" << std::hex << std::uppercase
            << static_cast<unsigned long>(hr) << ")";
        LavenderConsole::GetInstance().Log(oss.str());
    }

    static void DrawTriangleCursor()
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 mp = ImGui::GetIO().MousePos;

        ImVec2 a = { floorf(mp.x) + 0.5f, floorf(mp.y) + 0.5f };
        ImVec2 b = { a.x + 17.0f, a.y + 7.0f };
        ImVec2 c = { a.x + 7.0f,  a.y + 17.0f };

        dl->AddTriangleFilled(a, b, c, IM_COL32(255, 255, 255, 255));
        dl->AddTriangle(a, b, c, IM_COL32(0, 0, 0, 255), 2.0f);
    }

    static void EnsureImGui(LPDIRECT3DDEVICE9 device)
    {
        static bool ui_registered = false;
        static bool texture_loader_initialized = false;

        if (g_initialized.load())
            return;

        D3DDEVICE_CREATION_PARAMETERS params{};
        if (FAILED(device->GetCreationParameters(&params)))
        {
            LavenderConsole::GetInstance().Log("DX9: GetCreationParameters failed.");
            return;
        }

        g_hwnd = params.hFocusWindow;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX9_Init(device);

        if (!texture_loader_initialized)
        {
            TextureLoader::Initialize(
                GraphicsBackend::DirectX9,
                device
            );

            TextureCapture::Hook(device);

            texture_loader_initialized = true;
        }

        if (!gui)
            gui = new GUI();

        if (!ui_registered)
        {
            RegisterUIWindows();
            ui_registered = true;
        }

        g_initialized.store(true);
        LavenderConsole::GetInstance().Log("DX9: ImGui initialized.");
    }

    static void BeginFrame()
    {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    static void EndFrame()
    {
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    HRESULT APIENTRY HookedReset(LPDIRECT3DDEVICE9 device, D3DPRESENT_PARAMETERS* pp)
    {
        if (g_initialized.load() && ImGui::GetCurrentContext())
            ImGui_ImplDX9_InvalidateDeviceObjects();

        HRESULT hr = original_reset(device, pp);

        if (SUCCEEDED(hr))
        {
            if (g_initialized.load() && ImGui::GetCurrentContext())
                ImGui_ImplDX9_CreateDeviceObjects();
        }
        else
        {
            LogHr("DX9: Reset failed", hr);
        }

        return hr;
    }

    HRESULT APIENTRY HookedEndScene(LPDIRECT3DDEVICE9 device)
    {
        static bool once = false;
        if (!once)
        {
            LavenderConsole::GetInstance().Log("DX9: EndScene fired.");
            LavenderHook::Hooks::g_activeRenderer =
                LavenderHook::Hooks::RendererType::DX9;
            once = true;
        }

        const HRESULT coop = device->TestCooperativeLevel();
        if (coop == D3DERR_DEVICELOST || coop == D3DERR_DEVICENOTRESET)
            return original_endscene(device);

        EnsureImGui(device);

        if (!g_initialized.load() || !ImGui::GetCurrentContext())
            return original_endscene(device);

        BeginFrame();

        LavenderHook::Input::Update();
        LavenderHook::UI::PlayAll();

        if (gui)
            gui->RenderOverlay();

        UIRegistry::Get().Update();
        UIRegistry::Get().Render();

        LavenderHook::UI::Widgets::RenderMenuSelectorButton(LavenderHook::Globals::show_menu);
        LavenderHook::UI::Widgets::RenderHoldToKillButton(LavenderHook::Globals::show_menu);

        if (LavenderHook::Globals::show_menu && gui)
            gui->Render();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        io.MouseDrawCursor = false;

        if (LavenderHook::Globals::show_menu)
            DrawTriangleCursor();

        EndFrame();

        TextureCapture::DumpQueuedTextures(device);

        bool wireframe = LavenderHook::Globals::debug_wireframe;
        bool noFog = LavenderHook::Globals::debug_no_fog;

        if (wireframe)
            device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
        else
            device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

        if (noFog)
            device->SetRenderState(D3DRS_FOGENABLE, FALSE);

        if (LavenderHook::Globals::debug_freeze)
            return S_OK;

        return original_endscene(device);
    }

    bool Hook()
    {
        LavenderHook::Log::Write("DX9: Hook() called");

        if (!LavenderHook::Globals::window_handle)
        {
            LavenderHook::Log::Write("DX9: window_handle is null");
            return false;
        }

        IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d)
        {
            LavenderHook::Log::Write("DX9: Direct3DCreate9 failed");
            return false;
        }

        D3DPRESENT_PARAMETERS pp{};
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = LavenderHook::Globals::window_handle;
        pp.Windowed = TRUE;

        LPDIRECT3DDEVICE9 device = nullptr;
        HRESULT hr = d3d->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            pp.hDeviceWindow,
            D3DCREATE_HARDWARE_VERTEXPROCESSING,
            &pp,
            &device
        );

        if (FAILED(hr))
        {
            pp.Windowed = FALSE;
            hr = d3d->CreateDevice(
                D3DADAPTER_DEFAULT,
                D3DDEVTYPE_HAL,
                pp.hDeviceWindow,
                D3DCREATE_HARDWARE_VERTEXPROCESSING,
                &pp,
                &device
            );

            if (FAILED(hr))
            {
                LogHr("DX9: CreateDevice failed", hr);
                d3d->Release();
                return false;
            }
        }

        void** vtbl = *reinterpret_cast<void***>(device);

        auto targetEndScene = reinterpret_cast<EndScene_t>(vtbl[42]);
        auto targetReset = reinterpret_cast<Reset_t>(vtbl[16]);

        MH_CreateHook(targetEndScene, &HookedEndScene,
            reinterpret_cast<void**>(&original_endscene));
        MH_CreateHook(targetReset, &HookedReset,
            reinterpret_cast<void**>(&original_reset));

        MH_EnableHook(targetEndScene);
        MH_EnableHook(targetReset);

        device->Release();
        d3d->Release();
        LavenderHook::Log::Write("DX9: hook installed successfully");
        return true;
    }

    void Unhook()
    {
        LavenderHook::Log::Write("DX9 Unhook: Start");

        LavenderHook::Log::Write("DX9 Unhook: Unhooking TextureCapture...");
        TextureCapture::Unhook();
        LavenderHook::Log::Write("DX9 Unhook: TextureCapture unhooked.");

        if (g_initialized.load() && ImGui::GetCurrentContext())
        {
            LavenderHook::Log::Write("DX9 Unhook: Shutting down ImGui...");
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            g_initialized.store(false);
            LavenderHook::Log::Write("DX9 Unhook: ImGui shut down.");
        }

        LavenderHook::Log::Write("DX9 Unhook: Complete.");
    }
}
