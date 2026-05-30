#pragma once

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx11.h"
#include "../imgui/imgui_impl_win32.h"

#include "../misc/Globals.h"

extern float WINDOW_BORDER_SIZE;

// Triggers the one-time startup tooltip (menu controls + version).
void DisplayStartupToolTip();

class GUI
{
public:
    GUI();

    void Render();
    void RenderOverlay();
};

extern GUI* gui;
