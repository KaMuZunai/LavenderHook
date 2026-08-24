#pragma once
#include <vector>
#include <functional>
#include "../imgui/imgui.h"

#include "components/LavenderHotkey.h"
#include "components/LavenderFadeOut.h"

namespace LavenderHook::UI {

    struct UITiming {
        const char* label;
        int* valueMs;
        int minMs;
        int maxMs;
        bool asSeconds = false;
        bool asIntInput = false;
        bool isHotkey = false;
        bool isCheckbox = false;
        bool* checkboxValue = nullptr;
        int* hotkeyVK = nullptr;
        LavenderHook::UI::Lavender::Hotkey hotkey;

        float* sliderFloat = nullptr;
        float sliderMin = 0.0f;
        float sliderMax = 0.0f;
        const char* sliderFmt = "%.2f";
    };


    enum class UIItemType {
        Toggle,
        ToggleDropdown,
        SliderInt,
        SliderFloat,
        Button,
        ColorEdit3,
        ColorEdit4,
        Combo,
        InputText,
        InputInt,
        Separator,
        Text,
        ProgressBar,
        Checkbox,
        Radio,
        Spacer,
    };



    struct UIItem {
        UIItemType type;
        const char* label = nullptr;

        bool* toggle = nullptr;

        int* hotkeyVK = nullptr;
        int hotkeyShadow = 0;
        int hotkeyIndex = -1;

        float arrowAnim = 0.0f;
        float dropdownAnim = 0.0f;
        float dropdownFade = 0.0f;
        float colorAnim = 0.0f;

        mutable float layoutHeight = 0.0f;


        bool dropdownOpen = false;
        bool dropdownOpenNext = false;

        std::vector<UITiming> timings;

        int* sliderInt = nullptr;
        int min = 0;
        int max = 0;

        float* sliderFloat = nullptr;
        float minF = 0.0f;
        float maxF = 0.0f;

        std::function<void()> onClick;
        const char* description = nullptr;
        float tooltipFade = 0.0f;

        // ColorEdit
        float* color3 = nullptr;
        float* color4 = nullptr;

        // Combo
        const char** comboItems = nullptr;
        int comboCount = 0;
        int* comboCurrent = nullptr;

        // InputText
        char* textBuf = nullptr;
        int textBufSize = 0;

        // InputInt
        int* inputIntValue = nullptr;
        int inputIntMin = 0;
        int inputIntMax = 0;

        // Text
        const char* textContent = nullptr;

        // ProgressBar
        float* progressValue = nullptr;
        float progressMin = 0.0f;
        float progressMax = 1.0f;
        const char* progressFmt = nullptr;

        // Checkbox
        bool* checkboxValue = nullptr;

        // Radio
        int* radioValue = nullptr;
        const char** radioLabels = nullptr;
        int radioCount = 0;
    };


    class UIWindowBuilder {
    public:
        explicit UIWindowBuilder(const char* title);

        UIWindowBuilder& SetWidth(float w);

        UIWindowBuilder& AddToggle(
            const char* label,
            bool* value,
            int* hotkeyVK = nullptr
        );

        UIWindowBuilder& AddToggleDropdown(
            const char* label,
            bool* value,
            int* hotkeyVK = nullptr
        );

        UIWindowBuilder& AddDropdownTiming(
            const char* label,
            int* valueMs,
            int minMs,
            int maxMs
        );

        UIWindowBuilder& AddDropdownTimingSeconds(
            const char* label,
            int* valueMs,
            int minMs,
            int maxMs
        );

        UIWindowBuilder& AddDropdownButton(
            const char* label,
            int* hotkeyVK
        );

        UIWindowBuilder& AddSlider(
            const char* label,
            int* value,
            int min,
            int max
        );

        UIWindowBuilder& AddSliderFloat(
            const char* label,
            float* value,
            float min,
            float max
        );

        UIWindowBuilder& AddDropdownCheckbox(
            const char* label,
            bool* value
        );

        UIWindowBuilder& AddDropdownIntInput(
            const char* label,
            int* value,
            int min,
            int max
        );

        UIWindowBuilder& AddDropdownSliderFloat(
            const char* label,
            float* value,
            float min,
            float max,
            const char* fmt = "%.2f"
        );

        UIWindowBuilder& AddButton(
            const char* label,
            std::function<void()> onClick
        );

        UIWindowBuilder& AddItemDescription(const char* description);

        // New element types
        UIWindowBuilder& AddColorEdit3(const char* label, float* value);
        UIWindowBuilder& AddColorEdit4(const char* label, float* value);
        UIWindowBuilder& AddCombo(const char* label, int* current, const char** items, int count);
        UIWindowBuilder& AddInputText(const char* label, char* buf, int bufSize);
        UIWindowBuilder& AddInputInt(const char* label, int* value, int min = 0, int max = 0);
        UIWindowBuilder& AddSeparator();
        UIWindowBuilder& AddText(const char* text);
        UIWindowBuilder& AddProgressBar(const char* label, float* value, float min = 0.0f, float max = 1.0f, const char* fmt = nullptr);
        UIWindowBuilder& AddCheckbox(const char* label, bool* value);
        UIWindowBuilder& AddRadio(const char* label, int* value, const char** items, int count);
        UIWindowBuilder& AddSpacer(float h = 4.0f);

        void Render(bool wantVisible);
        UIWindowBuilder& SetHeaderIcon(ImTextureID icon);

    private:
        float ComputeHeight() const;

    private:
        const char* m_title = "";
        float m_width = 270.0f;

        std::vector<UIItem> m_items;
        std::vector<LavenderHook::UI::Lavender::Hotkey> m_hotkeys;

        LavenderHook::UI::LavenderFadeOut m_fade;

        bool m_headerOpen = true;
        float m_headerArrowAnim = 1.0f;
        float m_headerAnim = 1.0f;


        ImTextureID m_headerIcon = 0;

        float m_lastContentHeight = 0.0f;
    };

} // namespace LavenderHook::UI
