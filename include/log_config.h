#pragma once
enum LogLevel
{
    DBG = -1,  // Debug - very detailed info
    INF,       // Info - important state changes
    WRN,       // Warning - potential issues
    ERR        // Error - actual problems
};

// Minimum log level to output
#define MIN_LOG_LEVEL INF

// Default log file path and size
#define DEFAULT_LOG_PATH L"logs/keyboard_checker.log"
#define DEFAULT_MAX_LOG_SIZE (1 * 1024 * 1024)  // 1MB

// Output configuration
#define LOG_TO_CONSOLE false  // Disable console logging
#define LOG_TO_FILE true     // Keep file logging
