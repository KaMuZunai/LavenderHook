#pragma once

#include <Windows.h>
#include <iostream>
#include <vector>

#include "../../imgui/imgui.h"
#include "../../imgui/imgui_impl_dx11.h"
#include "../../imgui/imgui_impl_win32.h"

class LavenderConsole
{
public:
    static LavenderConsole& GetInstance()
    {
        static LavenderConsole console;
        return console;
    }

    void Render(bool wantVisible);
    void Log(std::string line);

private:
    LavenderConsole()
    {
        this->buffer = {};
    }

    std::vector<std::string> buffer;
};
