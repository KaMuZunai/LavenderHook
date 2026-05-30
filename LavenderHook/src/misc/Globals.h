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

    // UI toggles
    inline bool show_menu = false;
    inline bool show_console = false;
    inline bool show_info_overlay = true;
    inline bool show_general_window = true;
    inline bool show_misc_window = true;
    inline bool show_buffing_window = true;
    inline bool show_gamepad_window = false;
    inline bool show_paragon_level_window = false;
    inline bool show_macro_window = false;
    inline bool show_performance_overlay = true;
    inline bool show_wave_window = false;
    inline bool show_travel_window = true;
    inline bool show_menu_selector_window = false;
    inline bool show_menu_logo = true;
    inline bool show_profiles_window = false;
    inline bool show_wiki_window = false;

    // Performance Overlay Settings
    inline bool show_perf_fps = true;
    inline bool show_perf_ram = true;
    inline bool show_perf_cpu = true;
    inline bool show_perf_gpu = true;

    // Theme
    inline bool use_polished_overlay = false;

    // Stop on Fail
	inline bool stop_on_fail = true;

    // Stop at wave 5 interval
    inline bool stop_at_wave_5 = false;
    
    // Keep custom triangle cursor visible even when the menu is hidden
    inline bool show_triangle_when_menu_hidden = false;

    // video overlay
    inline bool show_process_overlay_on_hide = false;

    // UI scale multiplier
    inline float menu_scale = 1.0f;

    // Sound volume percentage
    inline int sound_volume = 100;
	inline bool mute_buttons = false;
	inline bool mute_fail = false;

    // Focus shim
    inline bool simulate_unfocused = true;

    // Request for chopped Minimize to clear its toggle after regaining focus
    inline bool minimize_auto_clear_requested = false;

    // Set while the game window is hidden
    inline bool window_hidden = false;

    // XInput queries
    inline thread_local bool xinput_our_query = false;

    // DLL module handle
    inline HMODULE dll_module = nullptr;
    // Tray callback message id
    inline UINT tray_callback_message = WM_APP + 100;

    // Enemy / boss HUD data
    inline std::atomic<int>   enemies_alive{ 0 };
    inline std::atomic<int>   enemies_max{ 0 };
    inline std::atomic<bool>  boss_phase_active{ false };
    inline std::atomic<float> boss_health_current{ 0.f };
    inline std::atomic<float> boss_health_max{ 0.f };

    // Player mana data
    inline std::atomic<float> mana_current{ 0.f };
    inline std::atomic<float> mana_max{ 0.f };

    // Wave Timer
    inline std::atomic<int>   wave_time{ 0 };

    // Chat
    inline std::atomic<bool>  chat_hook_active{ false };
}
