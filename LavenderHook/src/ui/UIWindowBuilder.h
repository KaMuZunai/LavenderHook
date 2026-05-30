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
    };



    struct UIItem {
        UIItemType type;
        const char* label = nullptr;

        bool* toggle = nullptr;

        int* hotkeyVK = nullptr;
        int hotkeyShadow = 0;
        int hotkeyIndex = -1;

        float arrowAnim = 0.0f; // 0 = closed, 1 = open
        float dropdownAnim = 0.0f; // 0 = closed, 1 = open
        float dropdownFade = 0.0f; // seconds
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
        float tooltipFade = 0.0f; // 0..1
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
        float m_headerAnim = 1.0f;   // 0 = collapsed, 1 = expanded


        ImTextureID m_headerIcon = 0;

        float m_lastContentHeight = 0.0f;
    };

} // namespace LavenderHook::UI
