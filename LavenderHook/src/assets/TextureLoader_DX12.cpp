#include "TextureLoader.h"
#include <d3d12.h>
#include <cstdint>
#include <unordered_map>

namespace
{
    // SRV descriptor allocator state (shared with ImGui backend)
    D3D12_CPU_DESCRIPTOR_HANDLE g_srvCpuStart = {};
    D3D12_GPU_DESCRIPTOR_HANDLE g_srvGpuStart = {};
    UINT g_srvIncrement = 0;
    UINT* g_srvNextSlot = nullptr;
    UINT g_srvMaxDescs = 0;

    // Upload sync
    ID3D12Fence* g_uploadFence = nullptr;
    HANDLE g_uploadEvent = nullptr;
    UINT64 g_uploadFenceVal = 0;

    // Map from GPU descriptor handle to texture resource, so we can free it later
    std::unordered_map<UINT64, ID3D12Resource*> g_textureMap;
}

static uint64_t DX12_LoadTextureImpl(void* devicePtr, void* queuePtr, const void* pixels, int w, int h)
{
    if (!pixels || w <= 0 || h <= 0 || !g_srvNextSlot || !devicePtr || !queuePtr)
        return 0;

    if (*g_srvNextSlot >= g_srvMaxDescs)
        return 0;

    auto* device = (ID3D12Device*)devicePtr;
    auto* queue = (ID3D12CommandQueue*)queuePtr;

    UINT slot = (*g_srvNextSlot)++;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = g_srvCpuStart;
    cpuHandle.ptr += slot * g_srvIncrement;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = g_srvGpuStart;
    gpuHandle.ptr += slot * g_srvIncrement;

    D3D12_HEAP_PROPERTIES defaultProps = {};
    defaultProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = (UINT64)w;
    texDesc.Height = (UINT)h;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ID3D12Resource* texture = nullptr;
    if (FAILED(device->CreateCommittedResource(&defaultProps, D3D12_HEAP_FLAG_NONE,
        &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture))))
        return 0;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    device->CreateShaderResourceView(texture, &srvDesc, cpuHandle);

    // Row pitch must be 256-byte aligned
    UINT rowPitch = (w * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    UINT64 uploadSize = (UINT64)rowPitch * h;

    D3D12_HEAP_PROPERTIES uploadProps = {};
    uploadProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = uploadSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.SampleDesc.Count = 1;

    ID3D12Resource* uploadBuffer = nullptr;
    if (SUCCEEDED(device->CreateCommittedResource(&uploadProps, D3D12_HEAP_FLAG_NONE,
        &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer))))
    {
        void* mapped = nullptr;
        D3D12_RANGE readRange = { 0, 0 };
        if (SUCCEEDED(uploadBuffer->Map(0, &readRange, &mapped)))
        {
            for (int y = 0; y < h; y++)
                memcpy((BYTE*)mapped + y * rowPitch, (const BYTE*)pixels + y * w * 4, w * 4);
            uploadBuffer->Unmap(0, nullptr);

            ID3D12CommandAllocator* alloc = nullptr;
            ID3D12GraphicsCommandList* cmdList = nullptr;
            if (SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc))) &&
                SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&cmdList))))
            {
                D3D12_TEXTURE_COPY_LOCATION src = {};
                src.pResource = uploadBuffer;
                src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                src.PlacedFootprint.Footprint.Width = w;
                src.PlacedFootprint.Footprint.Height = h;
                src.PlacedFootprint.Footprint.Depth = 1;
                src.PlacedFootprint.Footprint.RowPitch = rowPitch;

                D3D12_TEXTURE_COPY_LOCATION dst = {};
                dst.pResource = texture;
                dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dst.SubresourceIndex = 0;

                cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = texture;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                cmdList->ResourceBarrier(1, &barrier);

                cmdList->Close();

                ID3D12CommandList* lists[] = { cmdList };
                queue->ExecuteCommandLists(1, lists);

                if (!g_uploadFence)
                {
                    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_uploadFence));
                    g_uploadEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
                }
                g_uploadFenceVal++;
                queue->Signal(g_uploadFence, g_uploadFenceVal);
                g_uploadFence->SetEventOnCompletion(g_uploadFenceVal, g_uploadEvent);
                WaitForSingleObject(g_uploadEvent, 5000);

                cmdList->Release();
            }
            if (alloc)
                alloc->Release();
        }
        uploadBuffer->Release();
    }

    g_textureMap[gpuHandle.ptr] = texture;
    return gpuHandle.ptr;
}

static void DX12_FreeTextureImpl(uint64_t id)
{
    auto it = g_textureMap.find(id);
    if (it != g_textureMap.end())
    {
        if (it->second)
            it->second->Release();
        g_textureMap.erase(it);
    }
}

static void DX12_SetSrvAllocatorImpl(uint64_t cpuStartPtr, uint64_t gpuStartPtr,
    uint32_t incrementSize, uint32_t* sharedNextSlot, uint32_t maxDescriptors)
{
    g_srvCpuStart.ptr = (SIZE_T)cpuStartPtr;
    g_srvGpuStart.ptr = gpuStartPtr;
    g_srvIncrement = incrementSize;
    g_srvNextSlot = sharedNextSlot;
    g_srvMaxDescs = maxDescriptors;
}

void TextureLoaderDX12_Init()
{
    TextureLoader::SetDX12Functions(
        DX12_LoadTextureImpl,
        DX12_FreeTextureImpl,
        DX12_SetSrvAllocatorImpl
    );
}
