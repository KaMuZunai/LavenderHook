#include "MacroWindowsShared.h"
#include "../input/InputAutomation.h"
#include "../misc/Globals.h"
#include "../misc/logmonitor/LogMonitor.h"
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

namespace LavenderHook::UI::Windows {

    static bool g_inited = false;
    static bool g_dirty = false;
    static const char* MacroActionTypeNames[] = {
        "Key Down",
        "Key Up",
        "Mouse Left Down",
        "Mouse Left Up",
        "Mouse Right Down",
        "Mouse Right Up",
        "Mouse Middle Down",
        "Mouse Middle Up",
        "Wait (ms)",
        "Activate State",
        "Deactivate State",
        "If Min Mana Activate",
        "If Min Mana Deactivate",
        "If Max Mana Activate",
        "If Max Mana Deactivate",
        "On Wave Activate State",
        "On Wave Deactivate State",
    };

    bool g_editorOpen = false;
    int g_selectedMacro = -1;
    std::vector<std::unique_ptr<MacroDefinition>> g_macros;

    static const char* MacroActionTypeToString(MacroActionType type)
    {
        return MacroActionTypeNames[(int)type];
    }

    static bool IsMouseAction(MacroActionType type)
    {
        return type == MacroActionType::MouseLeftDown || type == MacroActionType::MouseLeftUp ||
            type == MacroActionType::MouseRightDown || type == MacroActionType::MouseRightUp ||
            type == MacroActionType::MouseMiddleDown || type == MacroActionType::MouseMiddleUp;
    }

    static const char* MouseActionLabel(MacroActionType type)
    {
        switch (type)
        {
        case MacroActionType::MouseLeftDown: return "Mouse Left Down";
        case MacroActionType::MouseLeftUp: return "Mouse Left Up";
        case MacroActionType::MouseRightDown: return "Mouse Right Down";
        case MacroActionType::MouseRightUp: return "Mouse Right Up";
        case MacroActionType::MouseMiddleDown: return "Mouse Middle Down";
        case MacroActionType::MouseMiddleUp: return "Mouse Middle Up";
        default: return "Mouse Action";
        }
    }

    static std::string GetBaseConfigDir()
    {
        char* app = nullptr;
        size_t len = 0;
        std::string dir = ".";
        if (_dupenv_s(&app, &len, "APPDATA") == 0 && app)
        {
            dir = app;
            free(app);
        }
        dir += "\\LavenderHook";
        fs::create_directories(dir);
        return dir;
    }

    static fs::path GetMacroFolder()
    {
        fs::path dir = GetBaseConfigDir();
        dir /= "Macros";
        fs::create_directories(dir);
        return dir;
    }

    static std::string SanitizeFileName(const std::string& name)
    {
        std::string result;
        for (char c : name)
        {
            if ((c >= '0' && c <= '9') ||
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                c == '_' || c == '-')
            {
                result.push_back(c);
            }
            else if (c == ' ' || c == '.' || c == ',')
            {
                result.push_back('_');
            }
        }
        if (result.empty())
            result = "macro";
        return result;
    }

    static std::string MakeUniqueFileName(const std::string& base)
    {
        auto dir = GetMacroFolder();
        std::string cleanBase = SanitizeFileName(base);
        fs::path path = dir / (cleanBase + ".macro");
        int index = 1;
        while (fs::exists(path))
        {
            path = dir / (cleanBase + "_" + std::to_string(index++) + ".macro");
        }
        return path.stem().string();
    }

    static fs::path MakeMacroPath(const std::string& fileName)
    {
        auto dir = GetMacroFolder();
        return dir / (fileName + ".macro");
    }

    static bool SaveMacroFile(const MacroDefinition& macro)
    {
        if (macro.fileName.empty())
            return false;

        auto path = MakeMacroPath(macro.fileName);
        std::ofstream f(path, std::ios::trunc);
        if (!f)
            return false;

        f << "name=" << macro.name << "\n";
        f << "enabled=" << (macro.enabled ? 1 : 0) << "\n";
        f << "hidden=" << (macro.hidden ? 1 : 0) << "\n";
        f << "order=" << macro.order << "\n";
        f << "hotkey=" << macro.hotkeyVK << "\n";
        f << "state_count=" << macro.states.size() << "\n";

        for (int si = 0; si < (int)macro.states.size(); ++si)
        {
            auto& state = *macro.states[si];
            f << "state." << si << ".name=" << state.name << "\n";
            f << "state." << si << ".active=" << (state.active ? 1 : 0) << "\n";
            f << "state." << si << ".loop=" << (state.loop ? 1 : 0) << "\n";
            f << "state." << si << ".action_count=" << state.actions.size() << "\n";
            for (int ai = 0; ai < (int)state.actions.size(); ++ai)
            {
                auto& action = *state.actions[ai];
                f << "state." << si << ".action." << ai << ".type=" << MacroActionTypeToString(action.type) << "\n";
                if (action.type == MacroActionType::KeyDown || action.type == MacroActionType::KeyUp)
                    f << "state." << si << ".action." << ai << ".vk=" << action.vk << "\n";
                if (action.type == MacroActionType::WaitMs)
                    f << "state." << si << ".action." << ai << ".waitMs=" << action.waitMs << "\n";
                if (action.type == MacroActionType::ActivateState || action.type == MacroActionType::DeactivateState ||
                    action.type == MacroActionType::IfMinManaActivate || action.type == MacroActionType::IfMinManaDeactivate ||
                    action.type == MacroActionType::IfMaxManaActivate || action.type == MacroActionType::IfMaxManaDeactivate)
                    f << "state." << si << ".action." << ai << ".targetState=" << action.targetState << "\n";
                if (action.type == MacroActionType::WaveIntervalActivate || action.type == MacroActionType::WaveIntervalDeactivate)
                {
                    f << "state." << si << ".action." << ai << ".targetState=" << action.targetState << "\n";
                    f << "state." << si << ".action." << ai << ".waveInterval=" << action.waveInterval << "\n";
                }
                if (action.type == MacroActionType::IfMinManaActivate || action.type == MacroActionType::IfMinManaDeactivate ||
                    action.type == MacroActionType::IfMaxManaActivate || action.type == MacroActionType::IfMaxManaDeactivate)
                    f << "state." << si << ".action." << ai << ".threshold=" << action.threshold << "\n";
            }
        }

        return true;
    }

    static MacroActionType ParseMacroActionType(const std::string& value)
    {
        for (int i = 0; i < (int)std::size(MacroActionTypeNames); ++i)
        {
            if (value == MacroActionTypeNames[i])
                return (MacroActionType)i;
        }
        return MacroActionType::WaitMs;
    }

    static void ResetStateRuntime(MacroState& state)
    {
        state.currentAction = 0;
        state.waiting = false;
        state.running = state.active;
        for (auto& action : state.actions)
        {
            action->waveLastTriggered = 0;
            action->waveWaiting = false;
            action->waveDetectedTime = {};
        }
    }

    static bool LoadMacroFile(const fs::path& path, MacroDefinition& macro)
    {
        std::ifstream f(path);
        if (!f)
            return false;

        std::unordered_map<std::string, std::string> map;
        std::string line;
        while (std::getline(f, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            auto eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            map[line.substr(0, eq)] = line.substr(eq + 1);
        }

        if (map.find("name") == map.end())
            return false;

        macro.name = map["name"];
        macro.enabled = map["enabled"] == "1";
        macro.hidden = map["hidden"] == "1";
        macro.order = map.find("order") != map.end() ? std::atoi(map["order"].c_str()) : -1;
        macro.hotkeyVK = std::atoi(map["hotkey"].c_str());
        macro.hotkey.keyVK = &macro.hotkeyVK;
        macro.fileName = path.stem().string();

        int stateCount = std::atoi(map["state_count"].c_str());
        for (int si = 0; si < stateCount; ++si)
        {
            auto state = std::make_unique<MacroState>();
            state->name = map["state." + std::to_string(si) + ".name"];
            state->active = map["state." + std::to_string(si) + ".active"] == "1";
            state->loop = map["state." + std::to_string(si) + ".loop"] == "1";
            int actionCount = std::atoi(map["state." + std::to_string(si) + ".action_count"].c_str());
            for (int ai = 0; ai < actionCount; ++ai)
            {
                auto action = std::make_unique<MacroAction>();
                action->type = ParseMacroActionType(map["state." + std::to_string(si) + ".action." + std::to_string(ai) + ".type"]);
                action->vk = std::atoi(map["state." + std::to_string(si) + ".action." + std::to_string(ai) + ".vk"].c_str());
                action->waitMs = std::atoi(map["state." + std::to_string(si) + ".action." + std::to_string(ai) + ".waitMs"].c_str());
                action->targetState = std::atoi(map["state." + std::to_string(si) + ".action." + std::to_string(ai) + ".targetState"].c_str());
                action->threshold = std::atoi(map["state." + std::to_string(si) + ".action." + std::to_string(ai) + ".threshold"].c_str());
                action->waveInterval = std::atoi(map["state." + std::to_string(si) + ".action." + std::to_string(ai) + ".waveInterval"].c_str());
                state->actions.emplace_back(std::move(action));
            }
            macro.states.emplace_back(std::move(state));
        }

        return true;
    }

    void InitMacros()
    {
        if (g_inited)
            return;

        g_macros.clear();
        auto dir = GetMacroFolder();
        if (!fs::exists(dir))
            return;

        int nextOrder = 0;
        for (auto& entry : fs::directory_iterator(dir))
        {
            if (entry.path().extension() != ".macro")
                continue;
            MacroDefinition macro;
            if (LoadMacroFile(entry.path(), macro))
            {
                if (macro.order < 0)
                    macro.order = nextOrder++;
                else
                    nextOrder = std::max(nextOrder, macro.order + 1);
                g_macros.emplace_back(std::make_unique<MacroDefinition>(std::move(macro)));
            }
        }

        std::stable_sort(g_macros.begin(), g_macros.end(), [](const std::unique_ptr<MacroDefinition>& a, const std::unique_ptr<MacroDefinition>& b) {
            return a->order < b->order;
        });

        if (g_selectedMacro < 0 && !g_macros.empty())
            g_selectedMacro = 0;

        g_inited = true;
    }

    void SaveAllMacros()
    {
        for (auto& macro : g_macros)
        {
            if (macro->fileName.empty())
                macro->fileName = MakeUniqueFileName(macro->name);
            SaveMacroFile(*macro);
        }
        g_dirty = false;
    }

    void SaveDirtyMacros()
    {
        if (g_dirty)
            SaveAllMacros();
    }

    void MarkMacrosDirty()
    {
        g_dirty = true;
        SaveDirtyMacros();
    }

    static void ExecuteMacroAction(const MacroAction& action)
    {
        if (!LavenderHook::Input::AutomationAllowed())
            return;

        switch (action.type)
        {
        case MacroActionType::KeyDown:
            LavenderHook::Input::PressDownVK((WORD)action.vk);
            break;
        case MacroActionType::KeyUp:
            LavenderHook::Input::PressUpVK((WORD)action.vk);
            break;
        case MacroActionType::MouseLeftDown:
        case MacroActionType::MouseLeftUp:
        case MacroActionType::MouseRightDown:
        case MacroActionType::MouseRightUp:
        case MacroActionType::MouseMiddleDown:
        case MacroActionType::MouseMiddleUp:
        {
            INPUT input{};
            input.type = INPUT_MOUSE;
            switch (action.type)
            {
            case MacroActionType::MouseLeftDown: input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; break;
            case MacroActionType::MouseLeftUp: input.mi.dwFlags = MOUSEEVENTF_LEFTUP; break;
            case MacroActionType::MouseRightDown: input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN; break;
            case MacroActionType::MouseRightUp: input.mi.dwFlags = MOUSEEVENTF_RIGHTUP; break;
            case MacroActionType::MouseMiddleDown: input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN; break;
            case MacroActionType::MouseMiddleUp: input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP; break;
            default: return;
            }
            SendInput(1, &input, sizeof(input));
            break;
        }
        default:
            break;
        }
    }

    void UpdateMacroExecution()
    {
        auto now = std::chrono::steady_clock::now();
        for (auto& macro : g_macros)
        {
            if (!macro->enabled)
            {
                macro->running = false;
                for (auto& statePtr : macro->states)
                    statePtr->running = false;
                continue;
            }

            if (!macro->running)
            {
                macro->running = true;
                for (auto& statePtr : macro->states)
                    ResetStateRuntime(*statePtr);
            }

            // Wave interval action checking (for all running states)
            for (auto& statePtr : macro->states)
            {
                auto& state = *statePtr;
                if (!state.running) continue;
                for (auto& actionPtr : state.actions)
                {
                    auto& action = *actionPtr;
                    if (action.type != MacroActionType::WaveIntervalActivate &&
                        action.type != MacroActionType::WaveIntervalDeactivate)
                        continue;
                    if (action.waveInterval <= 0) continue;

                    int currentWave = LavenderHook::LogMonitor::GetCurrentWave();
                    bool onInterval = currentWave > 0 && currentWave % action.waveInterval == 0;

                    if (onInterval && currentWave != action.waveLastTriggered && !action.waveWaiting)
                    {
                        action.waveLastTriggered = currentWave;
                        action.waveWaiting = true;
                        action.waveDetectedTime = now;
                    }

                    if (!onInterval)
                        action.waveLastTriggered = 0;

                    if (action.waveWaiting)
                    {
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - action.waveDetectedTime).count();
                        if (elapsed >= 10)
                        {
                            action.waveWaiting = false;
                            if (action.targetState >= 0 && action.targetState < (int)macro->states.size())
                            {
                                auto& target = *macro->states[action.targetState];
                                if (action.type == MacroActionType::WaveIntervalActivate)
                                {
                                    target.running = true;
                                    target.currentAction = 0;
                                    target.waiting = false;
                                }
                                else
                                {
                                    target.running = false;
                                }
                            }
                        }
                    }
                }
            }

            for (auto& statePtr : macro->states)
            {
                auto& state = *statePtr;

                if (!state.running || state.actions.empty())
                    continue;

                if (state.currentAction >= (int)state.actions.size())
                {
                    if (state.loop)
                        state.currentAction = 0;
                    else
                    {
                        state.running = false;
                        state.currentAction = 0;
                        state.waiting = false;
                        continue;
                    }
                }

                auto& action = *state.actions[state.currentAction];
                if (action.type == MacroActionType::WaitMs)
                {
                    if (!state.waiting)
                    {
                        state.waiting = true;
                        state.waitStart = now;
                    }
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.waitStart).count();
                    if (elapsed >= action.waitMs)
                    {
                        state.waiting = false;
                        state.currentAction++;
                    }
                    continue;
                }

                if (action.type == MacroActionType::ActivateState || action.type == MacroActionType::DeactivateState)
                {
                    if (action.targetState >= 0 && action.targetState < (int)macro->states.size())
                    {
                        auto& target = *macro->states[action.targetState];
                        target.running = (action.type == MacroActionType::ActivateState);
                        if (target.running)
                            target.currentAction = 0;
                    }
                    state.currentAction++;
                    continue;
                }

                if (action.type == MacroActionType::IfMinManaActivate || action.type == MacroActionType::IfMinManaDeactivate ||
                    action.type == MacroActionType::IfMaxManaActivate || action.type == MacroActionType::IfMaxManaDeactivate)
                {
                    if (action.targetState >= 0 && action.targetState < (int)macro->states.size())
                    {
                        float mana = LavenderHook::Globals::mana_current.load(std::memory_order_relaxed);
                        bool condition = false;
                        if (action.type == MacroActionType::IfMinManaActivate || action.type == MacroActionType::IfMinManaDeactivate)
                            condition = mana <= (float)action.threshold;
                        else
                            condition = mana >= (float)action.threshold;

                        if (condition)
                        {
                            auto& target = *macro->states[action.targetState];
                            if (action.type == MacroActionType::IfMinManaActivate || action.type == MacroActionType::IfMaxManaActivate)
                            {
                                target.running = true;
                                target.currentAction = 0;
                            }
                            else
                            {
                                target.running = false;
                            }
                        }
                    }
                    state.currentAction++;
                    continue;
                }

                ExecuteMacroAction(action);
                state.currentAction++;
            }
        }
    }

    void FixMacroOrder()
    {
        for (int i = 0; i < (int)g_macros.size(); ++i)
        {
            g_macros[i]->order = i;
        }
    }

    MacroDefinition& CreateMacro(const std::string& name)
    {
        auto macro = std::make_unique<MacroDefinition>();
        macro->name = name.empty() ? "New Macro" : name;
        macro->enabled = false;
        macro->hidden = false;
        macro->hotkeyVK = 0;
        macro->hotkey.keyVK = &macro->hotkeyVK;
        macro->order = (int)g_macros.size();
        macro->fileName = MakeUniqueFileName(macro->name);
        g_macros.emplace_back(std::move(macro));
        g_selectedMacro = (int)g_macros.size() - 1;
        g_dirty = true;
        SaveAllMacros();
        return *g_macros.back();
    }

    void DeleteMacro(int index)
    {
        if (index < 0 || index >= (int)g_macros.size())
            return;
        if (!g_macros[index]->fileName.empty())
        {
            fs::remove(MakeMacroPath(g_macros[index]->fileName));
        }
        g_macros.erase(g_macros.begin() + index);
        FixMacroOrder();
        if (g_selectedMacro >= (int)g_macros.size())
            g_selectedMacro = (int)g_macros.size() - 1;
        g_dirty = true;
        SaveAllMacros();
    }

    MacroDefinition* GetSelectedMacro()
    {
        if (g_selectedMacro < 0 || g_selectedMacro >= (int)g_macros.size())
            return nullptr;
        return g_macros[g_selectedMacro].get();
    }

} // namespace LavenderHook::UI::Windows
