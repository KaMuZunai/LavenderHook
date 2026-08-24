#pragma once
#include <string>

namespace LavenderHook::UI {

    enum class ToastType {
        Info,
        Success,
        Warning,
        Error
    };

    void ShowToast(const char* title, const char* message, ToastType type = ToastType::Info, int durationMs = 4000);

}
