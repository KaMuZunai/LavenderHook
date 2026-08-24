#include "TextureLoader.h"
#include <cstdint>
#include <cstdio>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <d3d11.h>
#include <d3d9.h>

#ifdef _WIN32
#include <Windows.h>
#include <gl/GL.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#endif

// Function pointers for DX12 path (set by DX12 hook code via SetDX12Functions)
namespace TextureLoader
{
    uint64_t (*DX12_LoadTextureFn)(void* device, void* commandQueue, const void* pixels, int w, int h) = nullptr;
    void     (*DX12_FreeTextureFn)(uint64_t id) = nullptr;
    void     (*DX12_SetSrvAllocatorFn)(uint64_t cpuStartPtr, uint64_t gpuStartPtr,
                                       uint32_t incrementSize, uint32_t* sharedNextSlot,
                                       uint32_t maxDescriptors) = nullptr;
}

namespace
{
    GraphicsBackend g_backend{};
    void* g_device = nullptr;
    void* g_commandQueue = nullptr;
    bool g_initialized = false;
}

void TextureLoader::SetDX12Functions(
    uint64_t (*loadFn)(void*, void*, const void*, int, int),
    void     (*freeFn)(uint64_t),
    void     (*srvAllocFn)(uint64_t, uint64_t, uint32_t, uint32_t*, uint32_t))
{
    DX12_LoadTextureFn = loadFn;
    DX12_FreeTextureFn = freeFn;
    DX12_SetSrvAllocatorFn = srvAllocFn;
}

void TextureLoader::Initialize(GraphicsBackend backend, void* device, void* commandQueue)
{
    g_backend = backend;
    g_device = device;
    g_commandQueue = commandQueue;
    g_initialized = true;
}

void TextureLoader::SetDX12SrvAllocator(uint64_t cpuStartPtr, uint64_t gpuStartPtr,
    uint32_t incrementSize, uint32_t* sharedNextSlot, uint32_t maxDescriptors)
{
    if (DX12_SetSrvAllocatorFn)
        DX12_SetSrvAllocatorFn(cpuStartPtr, gpuStartPtr, incrementSize, sharedNextSlot, maxDescriptors);
}

bool TextureLoader::IsInitialized()
{
    return g_initialized;
}

Texture TextureLoader::LoadFromFile(const char* path)
{
    Texture result{};
    if (!path || !g_initialized)
        return result;

    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") != 0 || !f)
        return result;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len > 0)
    {
        std::vector<unsigned char> buf(len);
        if (fread(buf.data(), 1, len, f) == (size_t)len)
            result = LoadFromMemory(buf.data(), buf.size());
    }

    fclose(f);
    return result;
}

Texture TextureLoader::LoadFromMemory(const void* data, size_t size)
{
    Texture result{};
    if (!g_initialized || !data || size == 0)
        return result;

    int w, h, channels;
    unsigned char* pixels = stbi_load_from_memory(
        (const unsigned char*)data,
        (int)size,
        &w, &h, &channels,
        4
    );

    if (!pixels)
        return result;

    if (g_backend == GraphicsBackend::DirectX11)
    {
        auto* device = (ID3D11Device*)g_device;

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = pixels;
        init.SysMemPitch = w * 4;

        ID3D11Texture2D* tex = nullptr;
        if (FAILED(device->CreateTexture2D(&desc, &init, &tex)))
        {
            stbi_image_free(pixels);
            return result;
        }

        ID3D11ShaderResourceView* srv = nullptr;
        device->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();

        result.id = (ImTextureID)(uintptr_t)srv;
        result.width = w;
        result.height = h;
    }
    else if (g_backend == GraphicsBackend::DirectX9)
    {
        auto* device = (IDirect3DDevice9*)g_device;

        IDirect3DTexture9* tex = nullptr;
        if (FAILED(device->CreateTexture(
            w, h, 1, D3DUSAGE_DYNAMIC,
            D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr)))
        {
            stbi_image_free(pixels);
            return result;
        }

        D3DLOCKED_RECT locked;
        if (SUCCEEDED(tex->LockRect(0, &locked, nullptr, 0)))
        {
            for (int y = 0; y < h; y++)
            {
                const BYTE* src = pixels + y * w * 4;
                BYTE* dst = (BYTE*)locked.pBits + y * locked.Pitch;
                for (int x = 0; x < w; x++)
                {
                    dst[x * 4 + 0] = src[x * 4 + 2];
                    dst[x * 4 + 1] = src[x * 4 + 1];
                    dst[x * 4 + 2] = src[x * 4 + 0];
                    dst[x * 4 + 3] = src[x * 4 + 3];
                }
            }
            tex->UnlockRect(0);
        }

        result.id = (ImTextureID)(uintptr_t)tex;
        result.width = w;
        result.height = h;
    }
    else if (g_backend == GraphicsBackend::DirectX12)
    {
        if (DX12_LoadTextureFn)
        {
            uint64_t handle = DX12_LoadTextureFn(g_device, g_commandQueue, pixels, w, h);
            if (handle)
            {
                result.id = (ImTextureID)handle;
                result.width = w;
                result.height = h;
            }
        }
    }
    else if (g_backend == GraphicsBackend::OpenGL)
    {
        GLuint tex_id = 0;
        glGenTextures(1, &tex_id);
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);

        result.id = (ImTextureID)(uintptr_t)tex_id;
        result.width = w;
        result.height = h;
    }

    stbi_image_free(pixels);
    return result;
}

void TextureLoader::Free(Texture& texture)
{
    if (!texture.IsValid())
        return;

    if (g_backend == GraphicsBackend::DirectX11)
    {
        auto* srv = (ID3D11ShaderResourceView*)(uintptr_t)texture.id;
        srv->Release();
    }
    else if (g_backend == GraphicsBackend::DirectX9)
    {
        auto* tex = (IDirect3DTexture9*)(uintptr_t)texture.id;
        tex->Release();
    }
    else if (g_backend == GraphicsBackend::DirectX12)
    {
        if (DX12_FreeTextureFn)
            DX12_FreeTextureFn((uint64_t)texture.id);
    }
    else if (g_backend == GraphicsBackend::OpenGL)
    {
        GLuint tex_id = (GLuint)(uintptr_t)texture.id;
        glDeleteTextures(1, &tex_id);
    }

    texture.id = 0;
    texture.width = 0;
    texture.height = 0;
}