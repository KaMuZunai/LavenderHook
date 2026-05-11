#pragma once

#include "../aobhooks/EnemiesAliveHook.h"
#include "../aobhooks/WaveDataHook.h"
#include "../aobhooks/TimeHook.h"
#include "../aobhooks/EnemiesMaxFallbackHook.h"
#include "../aobhooks/ManaHook.h"
#include "../aobhooks/ChatHook.h"

namespace LavenderHook::Memory {

    inline bool Initialize()
    {
        const bool a = EnemiesAliveHook::Initialize();
        const bool b = WaveDataHook::Initialize();
        const bool c = TimeHook::Initialize();
        const bool d = EnemiesMaxFallbackHook::Initialize();
        const bool e = ManaHook::Initialize();
        const bool f = ChatHook::Initialize();
        if (f) Globals::chat_hook_active.store(true);
        return a || b || c || d || e || f;
    }

    inline void Shutdown()
    {
        EnemiesAliveHook::Shutdown();
        WaveDataHook::Shutdown();
        TimeHook::Shutdown();
        EnemiesMaxFallbackHook::Shutdown();
        ManaHook::Shutdown();
        ChatHook::Shutdown();
    }

    // Call once per frame to update polled globals (e.g. Globals::mana_max).
    inline void Tick()
    {
        ManaHook::Tick();
        ChatHook::Tick();
    }

} // namespace LavenderHook::Memory

