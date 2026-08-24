#pragma once
#include "Texture.h"
#include <cstddef>
#include <cstdint>

enum class GraphicsBackend
{
    DirectX11,
    DirectX9,
    DirectX12,
    OpenGL
};

namespace TextureLoader
{
    void Initialize(GraphicsBackend backend, void* device, void* commandQueue = nullptr);
    bool IsInitialized();

    // For DX12: set the SRV descriptor allocator state
    // cpuStartPtr/gpuStartPtr are the byte addresses of the first allocatable descriptor
    // sharedNextSlot is a pointer to the slot counter (shared with ImGui's allocator)
    void SetDX12SrvAllocator(uint64_t cpuStartPtr, uint64_t gpuStartPtr,
                             uint32_t incrementSize, uint32_t* sharedNextSlot,
                             uint32_t maxDescriptors);

    // Set function pointers for DX12 texture lifecycle (implemented in TextureLoader_DX12.cpp)
    void SetDX12Functions(
        uint64_t (*loadFn)(void* device, void* commandQueue, const void* pixels, int w, int h),
        void     (*freeFn)(uint64_t id),
        void     (*srvAllocFn)(uint64_t cpuStartPtr, uint64_t gpuStartPtr,
                               uint32_t incrementSize, uint32_t* sharedNextSlot,
                               uint32_t maxDescriptors)
    );

    Texture LoadFromFile(const char* path);
    Texture LoadFromMemory(const void* data, size_t size);

    void Free(Texture& texture);
}