#pragma once
#include <string>

namespace LavenderHook::Webhook {
    extern std::string url;
    extern std::string user_id;
    extern std::string map_finished_msg;
    extern std::string core_destroyed_msg;
    extern bool map_finished_enabled;
    extern bool core_destroyed_enabled;

    void Update();
}
