#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <mutex>
#include "log_config.h"
#include <iostream>

inline std::wstring LogLevelToString(LogLevel level)
{
    switch (level)
    {
    case DBG:
        return L"DBG";
    case INF:
        return L"INF";
    case WRN:
        return L"WRN";
    case ERR:
        return L"ERR";
    default:
        return L"UNK";
    }
}

class Logger
{
public:
    static Logger &Instance()
    {
        static Logger instance;
        return instance;
    }

    void Initialize(const std::wstring &logFile = DEFAULT_LOG_PATH, size_t maxSizeBytes = DEFAULT_MAX_LOG_SIZE)
    {
        m_logFile = logFile;
        m_maxSizeBytes = maxSizeBytes;

        // Create logs directory if it doesn't exist
        std::filesystem::create_directories(std::filesystem::path(logFile).parent_path());

        // Add session separator
        std::wofstream logStream(m_logFile.c_str(), std::ios::app);
        if (logStream.is_open())
        {
            logStream << L"\n\n";
            logStream << L"=====================================\n";
            logStream << L"=== New Session Started ===\n";
            logStream << L"=====================================\n\n";
            logStream.close();
        }

        CheckAndRotateLog();
    }

    void Log(LogLevel level, const std::wstring &message, const wchar_t *file, int line, const wchar_t *funcName)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::wstringstream ss;
        std::tm tm;
        localtime_s(&tm, &time);

        ss << std::put_time(&tm, L"%Y-%m-%d %H:%M:%S.")
           << std::setfill(L'0') << std::setw(3)
           << std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch())
                      .count() %
                  1000;

        ss << L" [" << LogLevelToString(level) << L"] ";

        // Convert file path to wstring and get filename only
        std::filesystem::path filePath(file);
        std::wstring fileName = filePath.filename().wstring();
        ss << L"[" << fileName << L":" << funcName << L"] [" << line << L"] " << message;

        if (LOG_TO_CONSOLE)
        {
            std::wcout << ss.str() << std::endl;
        }

        if (LOG_TO_FILE)
        {
            std::wofstream logStream(m_logFile.c_str(), std::ios::app);
            if (logStream.is_open())
            {
                logStream << ss.str() << std::endl;
                logStream.close();

                CheckAndRotateLog();
            }
        }
    }

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    void CheckAndRotateLog()
    {
        try
        {
            std::filesystem::path logPath(m_logFile);
            if (!std::filesystem::exists(logPath))
            {
                return;
            }

            size_t currentSize = std::filesystem::file_size(logPath);
            if (currentSize > m_maxSizeBytes)
            {
                // Read the entire file
                std::wifstream inFile(m_logFile.c_str());
                std::wstring content((std::istreambuf_iterator<wchar_t>(inFile)),
                                     std::istreambuf_iterator<wchar_t>());
                inFile.close();

                // Find the position after the second newest session marker
                size_t lastSession = content.rfind(L"=== New Session Started ===");
                if (lastSession != std::wstring::npos)
                {
                    lastSession = content.rfind(L"=== New Session Started ===", lastSession - 1);
                }

                // If we found at least two sessions, truncate to keep only the newest
                if (lastSession != std::wstring::npos)
                {
                    content = content.substr(lastSession);
                    std::wofstream outFile(m_logFile.c_str(), std::ios::trunc);
                    outFile << content;
                    outFile.close();
                }
            }
        }
        catch (const std::exception &)
        {
            // Log rotation failed, but we'll continue anyway
        }
    }

    std::wstring m_logFile;
    size_t m_maxSizeBytes;
    std::mutex m_mutex;
};

// Helper function to convert char* to wstring
inline std::wstring ToWString(const char* str)
{
    if (!str)
    {
        return L"";
    }

    size_t size = strlen(str) + 1;
    std::wstring wstr(size, L'\0');
    mbstowcs(&wstr[0], str, size);
    return wstr;
}

// Function entry/exit macros
#define FUNCTION_START \
    Logger::Instance().Log(INF, L"Started", ToWString(__FILE__).c_str(), __LINE__, ToWString(__FUNCTION__).c_str())

#define FUNCTION_END \
    Logger::Instance().Log(INF, L"Ended", ToWString(__FILE__).c_str(), __LINE__, ToWString(__FUNCTION__).c_str())

// Logging macro
#define LOG(level, message) \
    Logger::Instance().Log(level, message, ToWString(__FILE__).c_str(), __LINE__, ToWString(__FUNCTION__).c_str())
