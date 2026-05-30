#pragma once
#include "../../ui/components/LavenderHotkey.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace LavenderHook::UI::Windows {

    enum class MacroActionType
    {
        KeyDown,
        KeyUp,
        MouseLeftDown,
        MouseLeftUp,
        MouseRightDown,
        MouseRightUp,
        MouseMiddleDown,
        MouseMiddleUp,
        WaitMs,
        ActivateState,
        DeactivateState,
        IfMinManaActivate,
        IfMinManaDeactivate,
        IfMaxManaActivate,
        IfMaxManaDeactivate,
        WaveIntervalActivate,
        WaveIntervalDeactivate,
    };

    struct MacroAction
    {
        MacroActionType type = MacroActionType::WaitMs;
        int vk = 0;
        int waitMs = 100;
        int targetState = -1;
        int threshold = 0;
        int waveInterval = 0;
        std::unique_ptr<LavenderHook::UI::Lavender::Hotkey> hotkey;

        // Runtime state (not persisted)
        int waveLastTriggered = 0;
        bool waveWaiting = false;
        std::chrono::steady_clock::time_point waveDetectedTime{};
    };

    struct MacroState
    {
        std::string name = "New State";
        bool active = false;
        bool loop = false;
        bool running = false;
        int currentAction = 0;
        bool waiting = false;
        std::chrono::steady_clock::time_point waitStart;
        std::vector<std::unique_ptr<MacroAction>> actions;
    };

    struct MacroDefinition
    {
        std::string name = "New Macro";
        bool enabled = false;
        bool hidden = false;
        bool running = false;
        int order = 0;
        int hotkeyVK = 0;
        std::string fileName;
        LavenderHook::UI::Lavender::Hotkey hotkey;
        std::vector<std::unique_ptr<MacroState>> states;
        int selectedState = -1;
    };

    extern bool g_editorOpen;
    extern int g_selectedMacro;
    extern std::vector<std::unique_ptr<MacroDefinition>> g_macros;

    void InitMacros();
    void SaveAllMacros();
    void SaveDirtyMacros();
    void MarkMacrosDirty();
    void UpdateMacroExecution();
    void FixMacroOrder();
    MacroDefinition& CreateMacro(const std::string& name);
    void DeleteMacro(int index);
    MacroDefinition* GetSelectedMacro();
}
