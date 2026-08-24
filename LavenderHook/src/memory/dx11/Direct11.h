#pragma once
#include <Windows.h>
#include <dxgi1_4.h>
#include <d3d11.h>

namespace LavenderHook::Hooks::Present11 {
    using Present_t = HRESULT(__stdcall*)(IDXGISwapChain* swap, UINT sync, UINT flags);
    using ResizeBuffers_t = HRESULT(__stdcall*)(IDXGISwapChain* swap, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

    bool Hook();
    void Unhook();

    extern Present_t original_present;
    extern ResizeBuffers_t original_resizebuffers;
}
