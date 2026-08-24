#pragma once
#include "../imgui/imgui.h"

struct Texture
{
    ImTextureID id = 0;
    int width = 0;
    int height = 0;

    bool IsValid() const { return id != 0; } 
};
