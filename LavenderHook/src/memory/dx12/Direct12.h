#pragma once
#include <Windows.h>
#include <dxgi1_4.h>
#include <d3d12.h>

namespace LavenderHook::Hooks::Present12 {
    using ExecuteCommandLists_t = void(__stdcall*)(ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists);

    bool Hook();
    void Unhook();

    // Called from the shared Present hook when DX12 device detected
    bool HasGameCommandQueue();
    bool IsDX12SwapChain(IDXGISwapChain* swap);
    void SignalFirstPresent();
    bool EnsureImGui12(IDXGISwapChain* swap);
    void RenderUIFrame12(IDXGISwapChain* swap);
    void OnResizeBuffers12();

    // Called by ExecuteCommandLists hook to capture the game's command queue
    void OnExecuteCommandLists(ID3D12CommandQueue* queue);

    extern ExecuteCommandLists_t original_executecommandlists;
}
