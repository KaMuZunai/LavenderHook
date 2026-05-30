#pragma once
#include "../../imgui/imgui.h"

namespace LavenderHook::UI::Windows {

    class ImageDragWindow {
    public:
        static void Init();
        static void Render(bool visible);

    private:
        static void LoadTexture();

        static ImTextureID s_texture;
        static ImVec2 s_pos;
        static ImVec2 s_size;
        static bool s_loaded;
    };

}
