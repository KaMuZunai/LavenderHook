#pragma once
#include <d3d9.h>

namespace TextureCapture
{
    bool Hook(LPDIRECT3DDEVICE9 device);
    void Unhook();
    void DumpQueuedTextures(LPDIRECT3DDEVICE9 device);
    void SetOutputDirectory(const char* path);
    bool IsEnabled();
    void SetEnabled(bool enabled);
}
