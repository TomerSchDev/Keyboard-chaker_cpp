#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <set>
#include "logger.h"

class KeyboardChecker {
public:
    KeyboardChecker();
    ~KeyboardChecker();

    void Start();
    void Stop();

private:
    void InitializeLayouts();
    bool InitializeWindow();
    bool IsValidInLayout(const std::wstring& text, HKL layout);
    wchar_t GetCharForKey(UINT vk, HKL layout);
    std::wstring ConvertText(const std::wstring& text, HKL fromLayout, HKL toLayout);
    void UpdatePopup(const std::wstring& currentText, 
                    const std::unordered_map<HKL, std::wstring>& conversions);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static KeyboardChecker* s_instance;

    // Layout information
    std::vector<HKL> m_availableLayouts;              // Available keyboard layouts
    std::unordered_map<HKL, std::wstring> m_layoutNames; // Layout names by handle
    
    // Text state
    std::wstring m_currentText;  // Current input text buffer
    size_t m_minTextLength;      // Minimum text length before showing suggestions
    bool m_isRunning;            // Program running state
    
    // Window handles
    HWND m_mainWindow;    // Hidden main window handle
    HWND m_popup;         // Popup window for suggestions
    HHOOK m_keyboardHook; // Keyboard hook handle
    
    // Window class name
    static constexpr const wchar_t* WINDOW_CLASS_NAME = L"KeyboardCheckerWindow";

    // Custom window messages
    static const UINT WM_UPDATE_TEXT = WM_USER + 1;    // Message to update text
    static const UINT WM_CHECK_LAYOUT = WM_USER + 2;   // Message to check layout
};
