#include "MacroEditorWindow.h"
#include "MacroWindowsShared.h"
#include "MacroManagerWindow.h"
#include "../assets/UITextures.h"
#include "../input/InputAutomation.h"
#include "../input/VkTable.h"
#include "../misc/Globals.h"
#include "../sound/SoundPlayer.h"
#include "../ui/components/LavenderFadeOut.h"
#include "../ui/components/LavenderHotkey.h"
#include "../imgui/imgui.h"
#include <Windows.h>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <vector>

namespace LavenderHook::UI::Windows {

    static LavenderHook::UI::LavenderFadeOut g_fade;
    static bool g_showExecutionInfo = true;
    static bool g_showHiddenMacros = false;
    static char g_newMacroName[128] = "New Macro";

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

    static bool IsMouseAction(MacroActionType type)
    {
        return type == MacroActionType::MouseLeftDown || type == MacroActionType::MouseLeftUp ||
            type == MacroActionType::MouseRightDown || type == MacroActionType::MouseRightUp ||
            type == MacroActionType::MouseMiddleDown || type == MacroActionType::MouseMiddleUp;
    }

    static void ResetState(MacroState& state)
    {
        state.currentAction = 0;
        state.waiting = false;
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

    static bool RenderActionEditor(MacroAction& action, MacroDefinition& macro, int actionIndex)
    {
        ImGui::PushID(actionIndex);
        bool remove = false;
        int typeIndex = (int)action.type;
        const float actionFieldWidth = 440.0f;
        ImGui::SetNextItemWidth(actionFieldWidth);
        if (ImGui::Combo("Type##action", &typeIndex, MacroActionTypeNames, IM_ARRAYSIZE(MacroActionTypeNames)))
        {
            action.type = (MacroActionType)typeIndex;
            MarkMacrosDirty();
        }

        if (action.type == MacroActionType::KeyDown || action.type == MacroActionType::KeyUp)
        {
            if (!action.hotkey)
            {
                action.hotkey = std::make_unique<LavenderHook::UI::Lavender::Hotkey>();
                action.hotkey->keyVK = &action.vk;
            }
            ImGui::TextUnformatted("Key"); ImGui::SameLine();
            if (action.hotkey->Render(ImVec2(160, ImGui::GetFrameHeight())))
                MarkMacrosDirty();
        }
        else if (IsMouseAction(action.type))
        {
            ImGui::TextUnformatted(MouseActionLabel(action.type));
        }
        else if (action.type == MacroActionType::WaitMs)
        {
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::InputInt("Wait ms", &action.waitMs))
            {
                if (action.waitMs < 0)
                    action.waitMs = 0;
                MarkMacrosDirty();
            }
        }
        else
        {
            if (macro.states.empty())
            {
                ImGui::TextDisabled("Add a state first to target.");
            }
            else
            {
                if (action.targetState < 0 || action.targetState >= (int)macro.states.size())
                    action.targetState = 0;

                std::vector<const char*> names;
                names.reserve(macro.states.size());
                for (auto& s : macro.states)
                    names.push_back(s->name.c_str());

                ImGui::SetNextItemWidth(300.0f);
                if (ImGui::Combo("Target State", &action.targetState, names.data(), (int)names.size()))
                {
                    MarkMacrosDirty();
                }
            }

            if (action.type == MacroActionType::IfMinManaActivate || action.type == MacroActionType::IfMinManaDeactivate ||
                action.type == MacroActionType::IfMaxManaActivate || action.type == MacroActionType::IfMaxManaDeactivate)
            {
                ImGui::SetNextItemWidth(180.0f);
                if (ImGui::InputInt("Threshold", &action.threshold))
                    MarkMacrosDirty();
                ImGui::SameLine(0, 4);
                ImGui::Text("%%");
            }

            if (action.type == MacroActionType::WaveIntervalActivate || action.type == MacroActionType::WaveIntervalDeactivate)
            {
                if (action.waveInterval < 1) action.waveInterval = 5;
                ImGui::SetNextItemWidth(100.0f);
                if (ImGui::InputInt("Every N waves", &action.waveInterval))
                {
                    if (action.waveInterval < 1) action.waveInterval = 1;
                    MarkMacrosDirty();
                }
            }
        }

        ImGui::SameLine(0, 4);
        if (ImGui::Button("^##action", ImVec2(32, 0)) && actionIndex > 0)
        {
            if (macro.selectedState >= 0 && macro.selectedState < (int)macro.states.size())
            {
                std::swap(macro.states[macro.selectedState]->actions[actionIndex], macro.states[macro.selectedState]->actions[actionIndex - 1]);
                MarkMacrosDirty();
            }
        }
        ImGui::SameLine(0, 4);
        if (ImGui::Button("v##action", ImVec2(32, 0)) && actionIndex + 1 < (int)macro.states[macro.selectedState]->actions.size())
        {
            if (macro.selectedState >= 0 && macro.selectedState < (int)macro.states.size())
            {
                std::swap(macro.states[macro.selectedState]->actions[actionIndex], macro.states[macro.selectedState]->actions[actionIndex + 1]);
                MarkMacrosDirty();
            }
        }
        ImGui::SameLine(0, 4);
        if (ImGui::Button("Remove##action", ImVec2(96, 0)))
        {
            remove = true;
            MarkMacrosDirty();
        }

        ImGui::PopID();
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 8));
        return remove;
    }

    static void RenderMacroEditorHeader(MacroDefinition& macro)
    {
        ImGui::Text("Macro Name");
        char nameBuf[128];
        std::strncpy(nameBuf, macro.name.c_str(), sizeof(nameBuf));
        if (ImGui::InputText("##macro_name", nameBuf, sizeof(nameBuf)))
        {
            macro.name = nameBuf;
            MarkMacrosDirty();
        }

        if (ImGui::Checkbox("Enabled", &macro.enabled))
            MarkMacrosDirty();
        ImGui::SameLine();
        if (ImGui::Checkbox("Hidden", &macro.hidden))
            MarkMacrosDirty();

        ImGui::Separator();
        ImGui::Text("Macro Hotkey: %s", LavenderHook::UI::Lavender::VkToString(macro.hotkeyVK));
        ImGui::Separator();
    }

    static void RenderMacroStatesPanel(MacroDefinition& macro)
    {
        ImGui::Text("States");
        ImGui::Separator();

        for (int si = 0; si < (int)macro.states.size(); ++si)
        {
            auto& state = *macro.states[si];
            ImGui::PushID(si);
            if (ImGui::Selectable(state.name.c_str(), macro.selectedState == si, 0, ImVec2(0, ImGui::GetFrameHeight())))
                macro.selectedState = si;
            ImGui::PopID();
        }
        ImGui::Separator();

        if (ImGui::Button("Add State"))
        {
            auto state = std::make_unique<MacroState>();
            state->name = "New State";
            macro.states.emplace_back(std::move(state));
            macro.selectedState = (int)macro.states.size() - 1;
            MarkMacrosDirty();
        }
        ImGui::SameLine();
        if (macro.selectedState >= 0 && macro.selectedState < (int)macro.states.size())
        {
            if (ImGui::Button("Delete State"))
            {
                macro.states.erase(macro.states.begin() + macro.selectedState);
                macro.selectedState = std::clamp(macro.selectedState, 0, (int)macro.states.size() - 1);
                MarkMacrosDirty();
            }
        }

        if (macro.selectedState >= 0 && macro.selectedState < (int)macro.states.size())
        {
            auto& state = *macro.states[macro.selectedState];
            char stateNameBuf[128];
            std::strncpy(stateNameBuf, state.name.c_str(), sizeof(stateNameBuf));
            if (ImGui::InputText("State Name", stateNameBuf, sizeof(stateNameBuf)))
            {
                state.name = stateNameBuf;
                MarkMacrosDirty();
            }
            if (ImGui::Checkbox("Active", &state.active))
            {
                MarkMacrosDirty();
                if (macro.enabled)
                    state.running = state.active;
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Loop", &state.loop))
                MarkMacrosDirty();

        }
    }

    static void RenderMacroActionPanel(MacroDefinition& macro)
    {
        ImGui::Text("Actions");
        ImGui::Separator();

        if (macro.selectedState < 0 || macro.selectedState >= (int)macro.states.size())
        {
            ImGui::TextDisabled("Select a state to edit actions.");
            return;
        }

        auto& state = *macro.states[macro.selectedState];
        for (int ai = 0; ai < (int)state.actions.size(); ++ai)
        {
            if (RenderActionEditor(*state.actions[ai], macro, ai))
            {
                state.actions.erase(state.actions.begin() + ai);
                --ai;
                MarkMacrosDirty();
            }
        }

        if (ImGui::Button("Add Action"))
            ImGui::OpenPopup("AddActionPopup");

        if (ImGui::BeginPopup("AddActionPopup"))
        {
            for (int i = 0; i < IM_ARRAYSIZE(MacroActionTypeNames); ++i)
            {
                if (ImGui::Selectable(MacroActionTypeNames[i]))
                {
                    auto action = std::make_unique<MacroAction>();
                    action->type = (MacroActionType)i;
                    action->targetState = macro.selectedState;
                    if (action->type == MacroActionType::KeyDown || action->type == MacroActionType::KeyUp)
                    {
                        action->hotkey = std::make_unique<LavenderHook::UI::Lavender::Hotkey>();
                        action->hotkey->keyVK = &action->vk;
                    }
                    state.actions.emplace_back(std::move(action));
                    MarkMacrosDirty();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
    }

    void MacroEditorWindow::Init()
    {
        InitMacros();
    }

    void MacroEditorWindow::Render(bool wantVisible)
    {
        Init();
        g_fade.Tick(wantVisible);
        if (!g_fade.ShouldRender())
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_fade.Alpha());
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.12f, 0.95f));

        if (!g_editorOpen)
        {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoTitleBar;

        if (!ImGui::Begin("##MacroEditor", nullptr, flags))
        {
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            return;
        }

        ImGui::TextUnformatted("Macro Editor");
        ImGui::SameLine(ImGui::GetWindowWidth() - 48.0f);
        if (ImGui::Button("X", ImVec2(36, 0)))
            g_editorOpen = false;

        ImGui::Separator();

        if (g_selectedMacro >= 0 && g_selectedMacro < (int)g_macros.size())
            RenderMacroEditorHeader(*g_macros[g_selectedMacro]);

        ImGui::Columns(3, "macro_editor_columns", true);
        ImGui::SetColumnWidth(0, 260.0f);
        ImGui::SetColumnWidth(1, 360.0f);

        if (ImGui::BeginChild("##editor_macro_list", ImVec2(0, 0), true))
        {
            ImGui::Text("Macros");
            ImGui::Separator();

            for (int i = 0; i < (int)g_macros.size(); ++i)
            {
                auto& macro = *g_macros[i];
                if (macro.hidden)
                    continue;

                ImGui::PushID(i);
                if (ImGui::Selectable(macro.name.c_str(), g_selectedMacro == i, 0, ImVec2(0, ImGui::GetFrameHeight())))
                    g_selectedMacro = i;

                if (ImGui::BeginPopupContextItem("macro_context"))
                {
                    if (ImGui::MenuItem("Delete Macro"))
                    {
                        DeleteMacro(i);
                        ImGui::CloseCurrentPopup();
                        ImGui::PopID();
                        break;
                    }
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            ImGui::Separator();
            if (ImGui::Button("Create Macro"))
            {
                CreateMacro(g_newMacroName[0] ? g_newMacroName : "New Macro");
            }
            ImGui::InputText("##new_macro_name", g_newMacroName, sizeof(g_newMacroName));
            ImGui::Separator();
            if (g_selectedMacro >= 0 && g_selectedMacro < (int)g_macros.size())
            {
                if (ImGui::Button("^##macro_order") && g_selectedMacro > 0)
                {
                    std::swap(g_macros[g_selectedMacro], g_macros[g_selectedMacro - 1]);
                    MacroManagerWindow::SwapRowStates(g_selectedMacro, g_selectedMacro - 1);
                    --g_selectedMacro;
                    FixMacroOrder();
                    MarkMacrosDirty();
                }
                ImGui::SameLine();
                if (ImGui::Button("v##macro_order") && g_selectedMacro + 1 < (int)g_macros.size())
                {
                    std::swap(g_macros[g_selectedMacro], g_macros[g_selectedMacro + 1]);
                    MacroManagerWindow::SwapRowStates(g_selectedMacro, g_selectedMacro + 1);
                    ++g_selectedMacro;
                    FixMacroOrder();
                    MarkMacrosDirty();
                }
            }
            ImGui::Separator();
            if (ImGui::Checkbox("Show Hidden Macros", &g_showHiddenMacros)) {}
            ImGui::Separator();

            if (g_showHiddenMacros)
            {
                ImGui::Text("Hidden");
                ImGui::Separator();
                for (int i = 0; i < (int)g_macros.size(); ++i)
                {
                    auto& macro = *g_macros[i];
                    if (!macro.hidden)
                        continue;
                    ImGui::PushID(i);
                    if (ImGui::Selectable(macro.name.c_str(), g_selectedMacro == i, 0, ImVec2(0, ImGui::GetFrameHeight())))
                        g_selectedMacro = i;
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
        }

        ImGui::NextColumn();
        if (ImGui::BeginChild("##editor_state_list", ImVec2(0, 0), true))
        {
            if (g_selectedMacro >= 0 && g_selectedMacro < (int)g_macros.size())
            {
                RenderMacroStatesPanel(*g_macros[g_selectedMacro]);
            }
            else
            {
                ImGui::TextDisabled("Select a macro from the left panel.");
            }
            ImGui::EndChild();
        }

        ImGui::NextColumn();
        if (ImGui::BeginChild("##editor_state_editor", ImVec2(0, 0), true))
        {
            if (g_selectedMacro >= 0 && g_selectedMacro < (int)g_macros.size())
            {
                RenderMacroActionPanel(*g_macros[g_selectedMacro]);
            }
            else
            {
                ImGui::TextDisabled("Select a macro and state to edit actions.");
            }
            ImGui::EndChild();
        }
        ImGui::Columns(1);
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    void MacroEditorWindow::UpdateActions()
    {
        SaveDirtyMacros();
    }

} // namespace LavenderHook::UI::Windows
