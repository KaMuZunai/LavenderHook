#pragma once
#include <windows.h>
#include <cstdio>
#include <ctime>
#include <string>
#include "../config/ConfigManager.h"
#include "Globals.h"

namespace LavenderHook::Log {

    inline void Write(const char* msg)
    {
        if (!Globals::enable_logging)
            return;

        static HANDLE s_file = INVALID_HANDLE_VALUE;
        if (s_file == INVALID_HANDLE_VALUE)
        {
            std::string dir = Config::GetBaseDir();
            CreateDirectoryA(dir.c_str(), nullptr);
            std::string fullPath = dir + "\\LavenderHook.log";
            // Create fresh log each time (truncate existing)
            s_file = CreateFileA(fullPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        }
        if (s_file != INVALID_HANDLE_VALUE)
        {
            char buf[1024];
            SYSTEMTIME st;
            GetLocalTime(&st);
            int len = snprintf(buf, sizeof(buf),
                "[%02u:%02u:%02u.%03u] %s\n",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
            DWORD written;
            WriteFile(s_file, buf, len, &written, nullptr);
        }
    }

}
