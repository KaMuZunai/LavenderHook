#include "hooks.h"

#include "../misc/FileLog.h"
#include "../net/NetworkMonitor.h"
#include "../input/FocusShim.h"

#include "dx11/Direct11.h"
#include "dx9/Direct9.h"
#include "dx12/Direct12.h"
#include "gl/OpenGL.h"

#include <cstdio>

GUI* gui = nullptr;

LavenderHook::Hooks::RendererType LavenderHook::Hooks::g_activeRenderer =
LavenderHook::Hooks::RendererType::None;

bool LavenderHook::Hooks::Initialize()
{
    if (MH_Initialize() != MH_OK)
    {
        LavenderConsole::GetInstance().Log("Failed to initialize MinHook.");
        return false;
    }
    return true;
}

bool LavenderHook::Hooks::Hook()
{
    bool dx11_ok = LavenderHook::Hooks::Present11::Hook();
    LavenderHook::Log::Write(dx11_ok ? "DX11: hook OK" : "DX11: hook FAIL");

    bool dx9_ok = LavenderHook::Hooks::EndScene9::Hook();
    LavenderHook::Log::Write(dx9_ok ? "DX9: hook OK" : "DX9: hook FAIL");

    bool dx12_ok = LavenderHook::Hooks::Present12::Hook();
    LavenderHook::Log::Write(dx12_ok ? "DX12: hook OK" : "DX12: hook FAIL");

    bool gl_ok = LavenderHook::Hooks::OpenGL::Hook();
    LavenderHook::Log::Write(gl_ok ? "OpenGL: hook OK" : "OpenGL: hook FAIL");

    if (!dx11_ok && !dx9_ok && !dx12_ok && !gl_ok)
    {
        LavenderHook::Log::Write("All renderer hooks failed");
        LavenderConsole::GetInstance().Log("LavenderHook: failed to hook any renderer.");
        return false;
    }

    LavenderHook::Log::Write("Renderer hooks installed");
    LavenderConsole::GetInstance().Log("LavenderHook: renderer hooks installed.");

    if (!LavenderHook::Hooks::WndProc::Hook())
    {
        LavenderHook::Log::Write("WndProc hook deferred — will retry from renderer hook");
    }

    if (!LavenderHook::Net::NetworkMonitor::Instance().InitHooks())
    {
        LavenderConsole::GetInstance().Log("NetworkMonitor: failed to initialize hooks.");
    }
    else
    {
        LavenderConsole::GetInstance().Log("NetworkMonitor: hooks active.");
    }

    if (!LavenderHook::Input::FocusShim::Install())
    {
        LavenderConsole::GetInstance().Log("FocusShim: failed to install.");
    }
    else
    {
        LavenderHook::Log::Write("FocusShim installed");
        LavenderConsole::GetInstance().Log("FocusShim: installed.");
    }

    if (!LavenderHook::Hooks::Timing::Hook())
    {
        LavenderConsole::GetInstance().Log("TimingHook: failed to hook QPC.");
    }

    return true;
}

bool LavenderHook::Hooks::Unhook()
{
    LavenderHook::Log::Write("=== UNHOOK START ===");

    // Step 1: Shutdown network monitor
    LavenderHook::Log::Write("[1/8] Shutting down network monitor...");
    LavenderHook::Net::NetworkMonitor::Instance().Shutdown();
    LavenderHook::Log::Write("[1/8] Network monitor shut down.");
    Sleep(50);

    // Step 2: Unhook timing
    LavenderHook::Log::Write("[2/8] Unhooking timing...");
    LavenderHook::Hooks::Timing::Unhook();
    LavenderHook::Log::Write("[2/8] Timing unhooked.");
    Sleep(50);

    // Step 3: Remove focus shim
    LavenderHook::Log::Write("[3/8] Removing focus shim...");
    LavenderHook::Input::FocusShim::Remove();
    LavenderHook::Log::Write("[3/8] Focus shim removed.");
    Sleep(50);

    // Step 4: Restore WndProc
    LavenderHook::Log::Write("[4/8] Restoring WndProc...");
    if (LavenderHook::Hooks::WndProc::original_wndproc &&
        LavenderHook::Globals::window_handle)
    {
        SetWindowLongPtrW(
            LavenderHook::Globals::window_handle,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(
                LavenderHook::Hooks::WndProc::original_wndproc
                )
        );
        LavenderHook::Hooks::WndProc::original_wndproc = nullptr;
        LavenderHook::Log::Write("[4/8] WndProc restored.");
    }
    else
    {
        LavenderHook::Log::Write("[4/8] WndProc already restored or no window.");
    }
    Sleep(50);

    // Step 5: Unhook renderer
    LavenderHook::Log::Write("[5/8] Unhooking renderer...");
    switch (g_activeRenderer)
    {
    case RendererType::DX11:
        LavenderHook::Log::Write("[5/8] -> DX11 Unhook...");
        LavenderHook::Hooks::Present11::Unhook();
        LavenderHook::Log::Write("[5/8] -> DX11 unhooked.");
        break;

    case RendererType::DX9:
        LavenderHook::Log::Write("[5/8] -> DX9 Unhook...");
        LavenderHook::Hooks::EndScene9::Unhook();
        LavenderHook::Log::Write("[5/8] -> DX9 unhooked.");
        break;

    case RendererType::DX12:
        LavenderHook::Log::Write("[5/8] -> DX12 Unhook...");
        LavenderHook::Hooks::Present12::Unhook();
        LavenderHook::Log::Write("[5/8] -> DX12 unhooked.");
        break;

    case RendererType::OpenGL:
        LavenderHook::Log::Write("[5/8] -> OpenGL Unhook...");
        LavenderHook::Hooks::OpenGL::Unhook();
        LavenderHook::Log::Write("[5/8] -> OpenGL unhooked.");
        break;

    default:
        LavenderHook::Log::Write("[5/8] -> Unknown renderer, trying all...");
        LavenderHook::Hooks::Present11::Unhook();
        LavenderHook::Log::Write("[5/8] -> DX11 attempt done.");
        Sleep(20);
        LavenderHook::Hooks::EndScene9::Unhook();
        LavenderHook::Log::Write("[5/8] -> DX9 attempt done.");
        Sleep(20);
        LavenderHook::Hooks::Present12::Unhook();
        LavenderHook::Log::Write("[5/8] -> DX12 attempt done.");
        Sleep(20);
        LavenderHook::Hooks::OpenGL::Unhook();
        LavenderHook::Log::Write("[5/8] -> OpenGL attempt done.");
        break;
    }
    LavenderHook::Log::Write("[5/8] Renderer unhook complete.");
    Sleep(100);

    // Step 6: All hooks should already be disabled by renderer unhook
    LavenderHook::Log::Write("[6/8] Finalizing MinHook cleanup...");
    MH_DisableHook(MH_ALL_HOOKS);
    LavenderHook::Log::Write("[6/8] MH_DisableHook done.");
    MH_RemoveHook(MH_ALL_HOOKS);
    LavenderHook::Log::Write("[6/8] MH_RemoveHook done.");
    MH_Uninitialize();
    LavenderHook::Log::Write("[6/8] MH_Uninitialize done.");

    g_activeRenderer = RendererType::None;

    LavenderHook::Log::Write("=== UNHOOK COMPLETE ===");
    LavenderConsole::GetInstance().Log("LavenderHook: clean unhook complete.");
    return true;
}
