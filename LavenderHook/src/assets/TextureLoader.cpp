#include "TextureLoader.h"
#include <cstdint>
#include <cstdio>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// DX11
#include <d3d11.h>

namespace
{
    GraphicsBackend g_backend{};
    void* g_device = nullptr;
}

void TextureLoader::Initialize(GraphicsBackend backend, void* device)
{
    g_backend = backend;
    g_device = device;
}

bool TextureLoader::IsInitialized()
{
    return g_device != nullptr;
}

Texture TextureLoader::LoadFromFile(const char* path)
{
    Texture result{};
    if (!path || !g_device)
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
    if (!g_device || !data || size == 0)
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

    // DX11
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

    stbi_image_free(pixels);
    return result;
}

void TextureLoader::Free(Texture& texture)
{
    if (texture.IsValid() && g_backend == GraphicsBackend::DirectX11)
    {
        auto* srv = (ID3D11ShaderResourceView*)(uintptr_t)texture.id;
        srv->Release();
        texture.id = 0;
        texture.width = 0;
        texture.height = 0;
    }
}
