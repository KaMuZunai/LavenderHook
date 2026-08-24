#pragma once
#include <Windows.h>

namespace LavenderHook::Hooks::OpenGL
{
    using SwapBuffers_t = BOOL(WINAPI*)(HDC hdc);

    bool Hook();
    void Unhook();

    extern SwapBuffers_t original_swapbuffers;
}
