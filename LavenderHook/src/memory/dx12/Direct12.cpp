#include "Direct12.h"
#include "../hooks.h"

#include "../../imgui/imgui.h"
#include "../../imgui/imgui_impl_win32.h"
#include "../../imgui/imgui_impl_dx12.h"

#include "../../assets/TextureLoader.h"
#include "../../assets/TextureLoaderDX12.h"
#include "../../misc/Globals.h"
#include "../../misc/FileLog.h"
#include "../../ui/GUI.h"
#include "../../ui/UIRegister.h"
#include "../../ui/UiDispatch.h"
#include "../../ui/UIWindows/console.h"

#include "../../ui/UIWindows/HoldToKillButton.h"
#include "../../ui/UIWindows/ToggleMenuButton.h"

#include "../../input/Hotkeys.h"

#include "../../minhook/MinHook.h"

#include <cmath>
#include <windowsx.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

extern GUI* gui;
void RegisterUIWindows();

LavenderHook::Hooks::Present12::ExecuteCommandLists_t
LavenderHook::Hooks::Present12::original_executecommandlists = nullptr;

namespace {

    ID3D12Device*                g_device = nullptr;
    ID3D12CommandQueue*          g_commandQueue = nullptr;
    ID3D12DescriptorHeap*        g_rtvHeap = nullptr;
    ID3D12DescriptorHeap*        g_srvHeap = nullptr;
    ID3D12GraphicsCommandList*   g_commandList = nullptr;
    ID3D12Fence*                 g_overlayFence = nullptr;
    HANDLE                       g_fenceEvent = nullptr;
    UINT64                       g_overlayFenceValue = 0;
    UINT                         g_bufferCount = 0;
    bool                         g_initialized = false;
    bool                         g_afterFirstPresent = false;

    struct FrameContext {
        ID3D12CommandAllocator* allocator = nullptr;
        ID3D12Resource*         renderTarget = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
    };
    FrameContext* g_frameContexts = nullptr;

    // SRV descriptor allocator
    struct SrvAllocator {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = {};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = {};
        UINT increment = 0;
        UINT nextSlot = 0;

        void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
        {
            *out_cpu = cpuStart;
            out_cpu->ptr += nextSlot * increment;
            *out_gpu = gpuStart;
            out_gpu->ptr += nextSlot * increment;
            nextSlot++;
        }
    } g_srvDescAlloc;

    void SrvAllocInit(ID3D12Device* dev, ID3D12DescriptorHeap* h)
    {
        g_srvDescAlloc.cpuStart = h->GetCPUDescriptorHandleForHeapStart();
        g_srvDescAlloc.gpuStart = h->GetGPUDescriptorHandleForHeapStart();
        g_srvDescAlloc.increment = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        g_srvDescAlloc.nextSlot = 0;
    }

    void SrvAllocFree(D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {}

    static std::atomic<bool> g_hooked{ false };

    void ForceOSCursorVisible(bool wantVisible)
    {
        static bool last = !wantVisible;
        if (wantVisible == last) return;
        last = wantVisible;
        CURSORINFO ci{ sizeof(ci) };
        if (!GetCursorInfo(&ci)) return;
        const bool isVisible = (ci.flags & CURSOR_SHOWING) != 0;
        if (wantVisible && !isVisible) while (ShowCursor(TRUE) < 0);
        else if (!wantVisible && isVisible) while (ShowCursor(FALSE) >= 0);
    }

    void DrawTriangleCursor()
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 mp = ImGui::GetIO().MousePos;
        if (!LavenderHook::Globals::show_menu && LavenderHook::Globals::window_handle)
        {
            POINT p;
            if (GetCursorPos(&p) && ScreenToClient(LavenderHook::Globals::window_handle, &p))
                mp = ImVec2((float)p.x, (float)p.y);
        }
        ImVec2 a = { floorf(mp.x) + 0.5f, floorf(mp.y) + 0.5f };
        ImVec2 b = { a.x + 22.0f, a.y + 8.0f };
        ImVec2 c = { a.x + 8.0f, a.y + 22.0f };
        dl->AddTriangleFilled(a, b, c, IM_COL32(255, 255, 255, 255));
        dl->AddTriangle(a, b, c, IM_COL32(0, 0, 0, 255), 2.5f);
    }

} // namespace

// ---- ExecuteCommandLists hook - captures game queue ----
static void __stdcall HookedExecuteCommandLists(
    ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists)
{
    if (!g_commandQueue && g_afterFirstPresent)
    {
        ID3D12Device* queueDevice = nullptr;
        if (SUCCEEDED(queue->GetDevice(IID_PPV_ARGS(&queueDevice))))
        {
            if (!g_device)
                g_device = queueDevice;

            if (queueDevice == g_device)
            {
                D3D12_COMMAND_QUEUE_DESC desc = queue->GetDesc();
                if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
                {
                    queue->AddRef();
                    g_commandQueue = queue;
                }
            }

            if (queueDevice && queueDevice != g_device)
                queueDevice->Release();
        }
    }
    g_afterFirstPresent = false;

    LavenderHook::Hooks::Present12::original_executecommandlists(queue, NumCommandLists, ppCommandLists);
}

// ---- Public API ----

bool LavenderHook::Hooks::Present12::HasGameCommandQueue()
{
    return g_commandQueue != nullptr;
}

void LavenderHook::Hooks::Present12::SignalFirstPresent()
{
    g_afterFirstPresent = true;
}

bool LavenderHook::Hooks::Present12::IsDX12SwapChain(IDXGISwapChain* swap)
{
    if (!swap) return false;
    ID3D12Device* testDevice = nullptr;
    HRESULT hr = swap->GetDevice(IID_PPV_ARGS(&testDevice));
    if (SUCCEEDED(hr) && testDevice) { testDevice->Release(); return true; }
    return false;
}

bool LavenderHook::Hooks::Present12::EnsureImGui12(IDXGISwapChain* swap)
{
    if (g_initialized) return true;

    g_afterFirstPresent = true;

    if (!g_commandQueue)
        return false;

    if (FAILED(swap->GetDevice(IID_PPV_ARGS(&g_device))))
        return false;

    IDXGISwapChain3* sc3 = nullptr;
    if (FAILED(swap->QueryInterface(IID_PPV_ARGS(&sc3))))
        return false;

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(sc3->GetDesc(&desc))) { sc3->Release(); return false; }
    g_bufferCount = desc.BufferCount;
    sc3->Release();

    // Create RTV heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC hd = {};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hd.NumDescriptors = g_bufferCount;
        if (FAILED(g_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&g_rtvHeap))))
            return false;
    }

    // Create SRV heap (shader visible)
    {
        D3D12_DESCRIPTOR_HEAP_DESC hd = {};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        hd.NumDescriptors = 64;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(g_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&g_srvHeap))))
            return false;
    }

    // Frame contexts - one per back buffer
    g_frameContexts = new FrameContext[g_bufferCount];
    for (UINT i = 0; i < g_bufferCount; i++)
    {
        if (FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frameContexts[i].allocator))))
            return false;
    }

    // Create RTVs for each back buffer
    UINT rtvSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < g_bufferCount; i++)
    {
        ID3D12Resource* back = nullptr;
        if (FAILED(sc3 ? sc3->GetBuffer(i, IID_PPV_ARGS(&back)) : swap->GetBuffer(i, IID_PPV_ARGS(&back))))
            return false;
        g_device->CreateRenderTargetView(back, nullptr, rtvHandle);
        g_frameContexts[i].renderTarget = back;
        g_frameContexts[i].rtvHandle = rtvHandle;
        rtvHandle.ptr += rtvSize;
    }

    // ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplWin32_Init(desc.OutputWindow);

    // Use new InitInfo API with game's command queue
    SrvAllocInit(g_device, g_srvHeap);
    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = g_device;
    initInfo.CommandQueue = g_commandQueue;
    initInfo.NumFramesInFlight = g_bufferCount;
    initInfo.RTVFormat = desc.BufferDesc.Format;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = g_srvHeap;
    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu) {
        *out_cpu = g_srvDescAlloc.cpuStart;
        out_cpu->ptr += g_srvDescAlloc.nextSlot * g_srvDescAlloc.increment;
        *out_gpu = g_srvDescAlloc.gpuStart;
        out_gpu->ptr += g_srvDescAlloc.nextSlot * g_srvDescAlloc.increment;
        g_srvDescAlloc.nextSlot++;
    };
    initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {};
    if (!ImGui_ImplDX12_Init(&initInfo))
        return false;

    // Texture loader
    TextureLoader::Initialize(GraphicsBackend::DirectX12, g_device, g_commandQueue);
    TextureLoaderDX12_Init();

    TextureLoader::SetDX12SrvAllocator(
        g_srvDescAlloc.cpuStart.ptr,
        g_srvDescAlloc.gpuStart.ptr,
        g_srvDescAlloc.increment,
        &g_srvDescAlloc.nextSlot,
        64
    );

    // Fence
    if (FAILED(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_overlayFence))))
        return false;
    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    g_overlayFenceValue = 0;

    if (!gui) gui = new GUI();
    LavenderHook::Hooks::WndProc::Hook();

    static bool ui_registered = false;
    if (!ui_registered) { RegisterUIWindows(); ui_registered = true; }

    g_initialized = true;
    return true;
}

void LavenderHook::Hooks::Present12::RenderUIFrame12(IDXGISwapChain* swap)
{
    if (!g_initialized || !g_commandQueue) return;

    if (g_device && FAILED(g_device->GetDeviceRemovedReason()))
        return;

    IDXGISwapChain3* sc3 = nullptr;
    if (FAILED(swap->QueryInterface(IID_PPV_ARGS(&sc3))))
        return;
    UINT frameIdx = sc3->GetCurrentBackBufferIndex();
    sc3->Release();

    if (frameIdx >= g_bufferCount) return;
    FrameContext& ctx = g_frameContexts[frameIdx];

    if (g_overlayFence->GetCompletedValue() < g_overlayFenceValue)
    {
        g_overlayFence->SetEventOnCompletion(g_overlayFenceValue, g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, 2000);
    }

    ctx.allocator->Reset();

    if (!g_commandList)
        g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, ctx.allocator, nullptr, IID_PPV_ARGS(&g_commandList));
    else
        g_commandList->Reset(ctx.allocator, nullptr);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    LavenderHook::Input::Update();
    LavenderHook::UI::PlayAll();
    if (gui) gui->RenderOverlay();
    UIRegistry::Get().Update();
    UIRegistry::Get().Render();
    LavenderHook::UI::Widgets::RenderMenuSelectorButton(LavenderHook::Globals::show_menu);
    LavenderHook::UI::Widgets::RenderHoldToKillButton(LavenderHook::Globals::show_menu);
    if (LavenderHook::Globals::show_menu && gui) gui->Render();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    io.MouseDrawCursor = false;
    ForceOSCursorVisible(false);
    if (LavenderHook::Globals::show_menu || LavenderHook::Globals::show_triangle_when_menu_hidden)
        DrawTriangleCursor();

    ImGui::Render();

    // PRESENT -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = ctx.renderTarget;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_commandList->ResourceBarrier(1, &barrier);

    g_commandList->OMSetRenderTargets(1, &ctx.rtvHandle, FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[] = { g_srvHeap };
    g_commandList->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_commandList);

    // RENDER_TARGET -> PRESENT
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_commandList->ResourceBarrier(1, &barrier);

    g_commandList->Close();

    // Execute on game's queue
    ID3D12CommandList* cmdLists[] = { g_commandList };
    g_commandQueue->ExecuteCommandLists(1, cmdLists);

    // Signal our fence
    g_commandQueue->Signal(g_overlayFence, ++g_overlayFenceValue);
}

void LavenderHook::Hooks::Present12::OnResizeBuffers12()
{
    if (g_initialized && ImGui::GetCurrentContext())
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (g_commandList) { g_commandList->Release(); g_commandList = nullptr; }
        if (g_rtvHeap) { g_rtvHeap->Release(); g_rtvHeap = nullptr; }
        if (g_srvHeap) { g_srvHeap->Release(); g_srvHeap = nullptr; }

        for (UINT i = 0; i < g_bufferCount; i++)
        {
            if (g_frameContexts[i].renderTarget) { g_frameContexts[i].renderTarget->Release(); g_frameContexts[i].renderTarget = nullptr; }
            if (g_frameContexts[i].allocator) { g_frameContexts[i].allocator->Release(); g_frameContexts[i].allocator = nullptr; }
        }
        delete[] g_frameContexts;
        g_frameContexts = nullptr;
        g_bufferCount = 0;
        g_initialized = false;
    }
}

bool LavenderHook::Hooks::Present12::Hook()
{
    if (g_hooked.load()) return true;

    const auto mh = MH_Initialize();
    if (mh != MH_OK && mh != MH_ERROR_ALREADY_INITIALIZED) return false;

    ID3D12Device* tmpDevice = nullptr;
    if (SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&tmpDevice))))
    {
        ID3D12CommandQueue* tmpQueue = nullptr;
        D3D12_COMMAND_QUEUE_DESC qd = {};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (SUCCEEDED(tmpDevice->CreateCommandQueue(&qd, IID_PPV_ARGS(&tmpQueue))))
        {
            void** vtbl = *(void***)tmpQueue;
            auto exec_ptr = (ExecuteCommandLists_t)vtbl[10];
            if (MH_CreateHook((LPVOID)exec_ptr, &HookedExecuteCommandLists, (LPVOID*)&original_executecommandlists) == MH_OK)
                MH_EnableHook((LPVOID)exec_ptr);
            tmpQueue->Release();
        }
        tmpDevice->Release();
    }

    g_hooked.store(true);
    return true;
}

void LavenderHook::Hooks::Present12::Unhook()
{
    LavenderHook::Log::Write("DX12 Unhook: Start");

    // Wait for GPU to finish all work
    LavenderHook::Log::Write("DX12 Unhook: Waiting for GPU...");
    if (g_commandQueue && g_overlayFence && g_fenceEvent)
    {
        g_commandQueue->Signal(g_overlayFence, ++g_overlayFenceValue);
        if (g_overlayFence->GetCompletedValue() < g_overlayFenceValue)
        {
            g_overlayFence->SetEventOnCompletion(g_overlayFenceValue, g_fenceEvent);
            WaitForSingleObject(g_fenceEvent, 2000);
        }
        Sleep(100);
    }
    LavenderHook::Log::Write("DX12 Unhook: GPU wait done.");

    if (ImGui::GetCurrentContext())
    {
        LavenderHook::Log::Write("DX12 Unhook: Shutting down ImGui...");
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        LavenderHook::Log::Write("DX12 Unhook: ImGui shut down.");
    }

    if (g_commandList) { LavenderHook::Log::Write("DX12 Unhook: Releasing commandList..."); g_commandList->Release(); g_commandList = nullptr; }
    if (g_rtvHeap) { LavenderHook::Log::Write("DX12 Unhook: Releasing rtvHeap..."); g_rtvHeap->Release(); g_rtvHeap = nullptr; }
    if (g_srvHeap) { LavenderHook::Log::Write("DX12 Unhook: Releasing srvHeap..."); g_srvHeap->Release(); g_srvHeap = nullptr; }
    if (g_overlayFence) { LavenderHook::Log::Write("DX12 Unhook: Releasing fence..."); g_overlayFence->Release(); g_overlayFence = nullptr; }
    if (g_fenceEvent) { LavenderHook::Log::Write("DX12 Unhook: Closing fenceEvent..."); CloseHandle(g_fenceEvent); g_fenceEvent = nullptr; }

    if (g_frameContexts)
    {
        LavenderHook::Log::Write("DX12 Unhook: Releasing frame contexts...");
        for (UINT i = 0; i < g_bufferCount; i++)
        {
            if (g_frameContexts[i].renderTarget) g_frameContexts[i].renderTarget->Release();
            if (g_frameContexts[i].allocator) g_frameContexts[i].allocator->Release();
        }
        delete[] g_frameContexts;
        g_frameContexts = nullptr;
    }

    if (g_commandQueue) { LavenderHook::Log::Write("DX12 Unhook: Releasing commandQueue..."); g_commandQueue->Release(); g_commandQueue = nullptr; }
    if (g_device) { LavenderHook::Log::Write("DX12 Unhook: Releasing device..."); g_device->Release(); g_device = nullptr; }

    g_initialized = false;
    g_hooked.store(false);

    LavenderHook::Log::Write("DX12 Unhook: Complete.");
}
