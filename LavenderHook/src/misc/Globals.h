#pragma once

#include <windows.h>
#include <atomic>
#include <string>

class CodeEditor;

namespace LavenderHook::Globals
{
    inline uintptr_t base_address = 0;
    inline size_t    module_size = 0;

    // Window info
    inline HWND window_handle = nullptr;
    inline int window_width = 0;
    inline int window_height = 0;
    inline bool menu_animating = false;
    inline std::string window_title;

    // UI toggles
    inline bool show_menu = false;
    inline bool show_console = false;
    inline bool show_menu_logo = true;
    inline bool show_info_overlay = true;
    inline bool show_debug_window = true;
    inline bool show_performance_overlay = true;
    inline bool show_menu_selector_window = false;
    inline bool show_profiles_window = false;

    // Performance Overlay Settings
    inline bool show_perf_fps = true;
    inline bool show_perf_ram = true;
    inline bool show_perf_cpu = true;
    inline bool show_perf_gpu = true;

    // Info Overlay
    inline bool show_ping = true;
    inline bool show_server = true;

    // Keep custom triangle cursor visible even when the menu is hidden
    inline bool show_triangle_when_menu_hidden = false;

    // UI scale multiplier
    inline float menu_scale = 1.0f;

    // Sound volume percentage
    inline int sound_volume = 100;
	inline bool mute_buttons = false;

    // Logging
    inline bool enable_logging = true;

    // DLL module handle
    inline HMODULE dll_module = nullptr;

    // Texture Capture
    inline bool texture_capture_enabled = false;

    // Fullscreen Borderless (one-shot toggle, self-disabling)
    inline bool fullscreen_borderless = false;

    // Simulate unfocused state when menu is open (FocusShim)
    inline bool simulate_unfocused = true;

    // Debug / Renderer toggles
    inline bool debug_wireframe = false;
    inline bool debug_no_fog = false;
    inline bool debug_freeze = false;

    // Game speed
    inline bool game_speed_enabled = false;
    inline float game_speed = 1.0f;

    // Theme: false = old (simple overlay), true = polished (high overlay)
    inline bool use_polished_overlay = true;
}
