#pragma once
#include "../../imgui/imgui.h"

namespace LavenderHook::UI::Lavender
{
    bool RenderWindowHeader(
        const char* title,
        ImTextureID headerIcon,
        ImTextureID arrowIcon,
        float width,
        float fadeAlpha,
        bool& headerOpen,
        float& headerAnim,
        float& arrowAnim
    );
}
