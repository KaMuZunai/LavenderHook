#include "CrashHandler.h"
#include "Globals.h"
#include "FileLog.h"
#include "../config/ConfigManager.h"

#include <windows.h>
#include <dbghelp.h>
#include <minidumpapiset.h>
#include <cstdio>
#include <ctime>

#pragma comment(lib, "dbghelp.lib")

namespace LavenderHook {

namespace {

    LPTOP_LEVEL_EXCEPTION_FILTER g_prevFilter = nullptr;
    HMODULE g_hModule = nullptr;

    const char* ExceptionCodeToString(DWORD code)
    {
        switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:     return "ACCESS_VIOLATION";
        case EXCEPTION_BREAKPOINT:           return "BREAKPOINT";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:   return "FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_OVERFLOW:         return "FLT_OVERFLOW";
        case EXCEPTION_FLT_UNDERFLOW:        return "FLT_UNDERFLOW";
        case EXCEPTION_GUARD_PAGE:           return "GUARD_PAGE";
        case EXCEPTION_ILLEGAL_INSTRUCTION:  return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:        return "IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:   return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:         return "INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION:  return "INVALID_DISPOSITION";
        case EXCEPTION_INVALID_HANDLE:       return "INVALID_HANDLE";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION:     return "PRIV_INSTRUCTION";
        case EXCEPTION_SINGLE_STEP:          return "SINGLE_STEP";
        case EXCEPTION_STACK_OVERFLOW:       return "STACK_OVERFLOW";
        default:                             return "UNKNOWN";
        }
    }

    void WriteMiniDump(EXCEPTION_POINTERS* ep, const char* crashDir)
    {
        SYSTEMTIME st;
        GetLocalTime(&st);

        char dumpPath[MAX_PATH];
        snprintf(dumpPath, sizeof(dumpPath),
            "%s\\Crash_%04d%02d%02d_%02d%02d%02d.dmp",
            crashDir,
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond);

        HANDLE hFile = CreateFileA(dumpPath, GENERIC_WRITE, 0, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return;

        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;

        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            MiniDumpWithDataSegs,
            &mei,
            nullptr,
            nullptr);

        CloseHandle(hFile);

        LavenderHook::Log::Write(("Crash dump saved: " + std::string(dumpPath)).c_str());
    }

    LONG CALLBACK CrashFilter(EXCEPTION_POINTERS* ep)
    {
        DWORD code = ep->ExceptionRecord->ExceptionCode;
        void* addr = ep->ExceptionRecord->ExceptionAddress;

        char buf[512];
        snprintf(buf, sizeof(buf),
            "CRASH: %s (0x%08X) at %p",
            ExceptionCodeToString(code), code, addr);
        LavenderHook::Log::Write(buf);

        std::string crashDir = Config::GetBaseDir() + "\\Crashes";
        CreateDirectoryA(crashDir.c_str(), nullptr);
        WriteMiniDump(ep, crashDir.c_str());

        return EXCEPTION_CONTINUE_SEARCH;
    }

} // namespace

    void CrashHandler::Install()
    {
        g_hModule = GetModuleHandleA("LavenderHook.dll");
        g_prevFilter = SetUnhandledExceptionFilter(CrashFilter);
        Log::Write("Crash handler installed");
    }

    void CrashHandler::Uninstall()
    {
        SetUnhandledExceptionFilter(g_prevFilter);
        g_prevFilter = nullptr;
        Log::Write("Crash handler uninstalled");
    }

}
