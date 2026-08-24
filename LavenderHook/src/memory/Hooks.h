#pragma once

#include <Windows.h>
#include <dxgi1_4.h>
#include <d3d11.h>
#include <iostream>

#include "../minhook/MinHook.h"

#include "../misc/Globals.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx11.h"
#include "../imgui/imgui_impl_win32.h"

#include "../ui/GUI.h"
#include "../ui/UIWindows/console.h"

namespace LavenderHook::Hooks
{
    enum class RendererType
    {
        None,
        DX11,
        DX9,
        DX12,
        OpenGL
    };

    extern RendererType g_activeRenderer;

    bool Initialize();
    bool Hook();
    bool Unhook();

    namespace Present11
    {
        bool Hook();
        void Unhook();
    };

    namespace EndScene9
    {
        bool Hook();
        void Unhook();
    };

    namespace Present12
    {
        bool Hook();
        void Unhook();
    };

    namespace OpenGL
    {
        bool Hook();
        void Unhook();
    };

    namespace Timing
    {
        bool Hook();
        void Unhook();
    };

    namespace WndProc
    {
        bool Hook();
        bool Unhook();

        inline WNDPROC original_wndproc = nullptr;
        LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    }
}
