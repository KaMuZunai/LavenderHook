#pragma once
#include <functional>
#include <vector>

namespace LavenderHook::UI
{
    void Enqueue(std::function<void()> fn);

    int  PlayAll();

    int  PendingApprox();
}
