#include "OpenGL.h"
#include "../hooks.h"

#include "../../imgui/imgui.h"
#include "../../imgui/imgui_impl_win32.h"
#include "../../imgui/imgui_impl_opengl3.h"

#include "../../misc/Globals.h"
#include "../../misc/FileLog.h"
#include "../../ui/GUI.h"
#include "../../ui/UIRegister.h"
#include "../../ui/UiDispatch.h"
#include "../../ui/UIWindows/console.h"

#include "../../ui/UIWindows/HoldToKillButton.h"
#include "../../ui/UIWindows/ToggleMenuButton.h"

#include "../../input/Hotkeys.h"

#include "../../assets/TextureLoader.h"

#include "../../minhook/MinHook.h"

#include <atomic>
#include <cmath>
#include <gl/GL.h>

#pragma comment(lib, "opengl32.lib")

extern GUI* gui;

void RegisterUIWindows();

namespace LavenderHook::Hooks::OpenGL
{
    static SwapBuffers_t original_swapbuffers = nullptr;

    static std::atomic<bool> g_hooked{ false };
    static std::atomic<bool> g_retryScheduled{ false };
    static std::atomic<bool> g_retryStop{ false };
    static HANDLE            g_retryThread = nullptr;

    static HWND  g_hwnd = nullptr;
    static HDC   g_hdc = nullptr;
    static HGLRC g_hglrc = nullptr;

    static void DrawTriangleCursor()
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 mp = ImGui::GetIO().MousePos;

        ImVec2 a = { floorf(mp.x) + 0.5f, floorf(mp.y) + 0.5f };
        ImVec2 b = { a.x + 22.0f, a.y + 8.0f };
        ImVec2 c = { a.x + 8.0f, a.y + 22.0f };

        dl->AddTriangleFilled(a, b, c, IM_COL32(255, 255, 255, 255));
        dl->AddTriangle(a, b, c, IM_COL32(0, 0, 0, 255), 2.5f);
    }

    static void EnsureImGui(HDC hdc)
    {
        static bool ui_registered = false;
        static bool texture_loader_initialized = false;

        if (ImGui::GetCurrentContext())
            return;

        g_hdc = hdc;
        g_hwnd = WindowFromDC(hdc);

        if (!g_hwnd)
            return;

        g_hglrc = wglGetCurrentContext();

        if (!g_hglrc)
            return;

        if (!LavenderHook::Globals::window_handle)
        {
            LavenderHook::Globals::window_handle = g_hwnd;
            RECT rect{};
            GetWindowRect(g_hwnd, &rect);
            LavenderHook::Globals::window_width = rect.right - rect.left;
            LavenderHook::Globals::window_height = rect.bottom - rect.top;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplOpenGL3_Init("#version 130");

        if (!texture_loader_initialized)
        {
            TextureLoader::Initialize(
                GraphicsBackend::OpenGL,
                nullptr
            );
            texture_loader_initialized = true;
        }

        if (!gui)
            gui = new GUI();

        if (!ui_registered)
        {
            RegisterUIWindows();
            ui_registered = true;
        }

        LavenderConsole::GetInstance().Log("OpenGL: ImGui initialized.");
    }

    static void BeginFrame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    static void EndFrame()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    static void RenderUIFrame()
    {
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
    }

    BOOL WINAPI HookedSwapBuffers(HDC hdc)
    {
        static bool once = false;
        if (!once)
        {
            LavenderHook::Log::Write("OpenGL: SwapBuffers called - hook is working");
            LavenderConsole::GetInstance().Log("OpenGL: SwapBuffers active.");
            LavenderHook::Hooks::g_activeRenderer = LavenderHook::Hooks::RendererType::OpenGL;
            once = true;
        }

        EnsureImGui(hdc);

        if (ImGui::GetCurrentContext())
            RenderUIFrame();

        if (LavenderHook::Globals::debug_freeze)
            return TRUE;

        return original_swapbuffers(hdc);
    }

    bool TryHookOnce()
    {
        LavenderHook::Log::Write("OpenGL: TryHookOnce");

        HMODULE hOpengl32 = GetModuleHandleA("opengl32.dll");
        if (!hOpengl32)
        {
            LavenderHook::Log::Write("OpenGL: opengl32.dll not loaded");
            LavenderConsole::GetInstance().Log("OpenGL: opengl32.dll not loaded.");
            return false;
        }

        auto swapBuffersAddr = reinterpret_cast<SwapBuffers_t>(
            GetProcAddress(hOpengl32, "wglSwapBuffers")
        );

        if (!swapBuffersAddr)
        {
            LavenderHook::Log::Write("OpenGL: wglSwapBuffers not found");
            LavenderConsole::GetInstance().Log("OpenGL: wglSwapBuffers not found.");
            return false;
        }

        if (MH_CreateHook((LPVOID)swapBuffersAddr, &HookedSwapBuffers, (LPVOID*)&original_swapbuffers) != MH_OK)
        {
            LavenderConsole::GetInstance().Log("OpenGL: MH_CreateHook(SwapBuffers) failed.");
            return false;
        }

        if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
        {
            LavenderHook::Log::Write("OpenGL: MH_EnableHook failed");
            return false;
        }

        LavenderHook::Log::Write("OpenGL: hook installed successfully");
        return true;
    }

    DWORD WINAPI RetryThreadProc(LPVOID)
    {
        while (!g_retryStop.load() && !g_hooked.load())
        {
            Sleep(5000);
            if (g_retryStop.load() || g_hooked.load())
                break;

            LavenderConsole::GetInstance().Log("OpenGL: Retrying hook...");
            if (TryHookOnce())
            {
                g_hooked.store(true);
                LavenderConsole::GetInstance().Log("OpenGL: Hook succeeded on retry.");
                break;
            }
        }

        g_retryScheduled.store(false);
        return 0;
    }
}

bool LavenderHook::Hooks::OpenGL::Hook()
{
    if (g_hooked.load())
        return true;

    if (TryHookOnce())
    {
        g_hooked.store(true);
        return true;
    }

    LavenderConsole::GetInstance().Log(
        "Failed to hook OpenGL SwapBuffers. Will retry in 5 seconds."
    );

    if (!g_retryScheduled.exchange(true))
    {
        g_retryStop.store(false);
        g_retryThread = CreateThread(nullptr, 0, RetryThreadProc, nullptr, 0, nullptr);
        if (!g_retryThread)
        {
            g_retryScheduled.store(false);
            LavenderConsole::GetInstance().Log("OpenGL: Failed to spawn retry thread.");
        }
    }

    return false;
}

void LavenderHook::Hooks::OpenGL::Unhook()
{
    LavenderHook::Log::Write("OpenGL Unhook: Start");

    g_retryStop.store(true);
    if (g_retryThread)
    {
        LavenderHook::Log::Write("OpenGL Unhook: Stopping retry thread...");
        WaitForSingleObject(g_retryThread, 100);
        CloseHandle(g_retryThread);
        g_retryThread = nullptr;
        g_retryScheduled.store(false);
    }

    if (ImGui::GetCurrentContext())
    {
        LavenderHook::Log::Write("OpenGL Unhook: Shutting down ImGui...");
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        LavenderHook::Log::Write("OpenGL Unhook: ImGui shut down.");
    }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    g_hooked.store(false);

    LavenderConsole::GetInstance().Log("OpenGL: Unhook complete.");
}
