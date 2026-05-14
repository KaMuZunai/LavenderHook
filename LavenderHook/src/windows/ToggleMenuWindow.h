#pragma once
#include "../imgui/imgui.h"

// Theme globals
extern ImVec4 MAIN_RED;
extern ImVec4 MID_RED;
extern ImVec4 DARK_RED;

bool LoadTheme();
void SaveTheme();
void ApplyThemeToImGui();
void LoadMenuSettings();
void LoadPerfSettings();

namespace LavenderHook {
    namespace UI {
        namespace Windows {

            void RenderMenuSelectorWindow(bool wantVisible);

        } // Windows
    } // UI
} // LavenderHook

void InitMenuScale();
