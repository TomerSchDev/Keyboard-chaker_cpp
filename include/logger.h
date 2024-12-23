#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include "log_config.h"

namespace Logger
{
    inline std::wofstream &GetLogFile()
    {
        static std::wofstream logFile;
        if (!logFile.is_open() && LOG_TO_FILE)
        {
            std::filesystem::create_directories(L"logs");
            logFile.open(LOG_FILE_PATH, std::ios::app);
        }
        return logFile;
    }

    inline std::wstring GetTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::wstringstream ss;
        std::tm tm;
        localtime_s(&tm, &time);
        ss << std::put_time(&tm, L"%Y-%m-%d %H:%M:%S")
           << L'.' << std::setfill(L'0') << std::setw(3) << ms.count();
        return ss.str();
    }

    inline const wchar_t *GetLevelString(int level)
    {
        switch (level)
        {
        case LOG_INF:
            return L"INF";
        case LOG_WRN:
            return L"WRN";
        case LOG_ERR:
            return L"ERR";
        default:
            return L"???";
        }
    }

    inline void LogMessage(const wchar_t *funcName, const std::wstring &message, int level)
    {
        if (level < MIN_LOG_LEVEL)
            return;

        std::wstring timestamp = GetTimestamp();
        std::wstring levelStr = GetLevelString(level);
        std::wstring fullMessage = timestamp + L" [" + levelStr + L"] [" + funcName + L"] " + message + L"\n";

        if (LOG_TO_CONSOLE)
        {
            std::wcout << fullMessage;
        }

        if (LOG_TO_FILE)
        {
            GetLogFile() << fullMessage;
            GetLogFile().flush();
        }
    }
}

// Function entry/exit macros
#define FUNCTION_START                                            \
    const wchar_t *__function_name = __FUNCTIONW__;              \
    Logger::LogMessage(__function_name, L"Started", LOG_INF);    \
    struct LogFunctionExit                                       \
    {                                                            \
        const wchar_t *func;                                     \
        LogFunctionExit(const wchar_t *f) : func(f) {}           \
        ~LogFunctionExit()                                       \
        {                                                        \
            Logger::LogMessage(func, L"Ended", LOG_INF);         \
        }                                                        \
    } __log_exit(__function_name)

// Single logging macro that takes level as first parameter
#define LOG(level, msg) Logger::LogMessage(__FUNCTIONW__, msg, level)
