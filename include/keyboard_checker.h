#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>
#include "logger.h"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAYICON 1
#define ID_TRAYMENU_EXIT 1001

union ModifierFlags
{
    struct
    {
        unsigned char shift : 1;
        unsigned char ctrl : 1;
        unsigned char alt : 1;
        unsigned char win : 1;
        unsigned char unused : 4;
    } bits;
    unsigned char value;

    ModifierFlags() : value(0) {}

    bool operator==(const ModifierFlags& other) const
    {
        return value == other.value;
    }

    // Update flag based on virtual key code
    void UpdateFromKey(DWORD vkCode, bool isPressed)
    {
        switch (vkCode)
        {
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
            bits.shift = isPressed ? 1 : 0;
            break;
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
            bits.ctrl = isPressed ? 1 : 0;
            break;
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
            bits.alt = isPressed ? 1 : 0;
            break;
        case VK_LWIN:
        case VK_RWIN:
            bits.win = isPressed ? 1 : 0;
            break;
        }
    }

    // Convert to keyboard state array for WinAPI
    void ToKeyboardState(BYTE keyState[256]) const
    {
        if (bits.shift)
            keyState[VK_SHIFT] = 0x80;
        if (bits.ctrl)
            keyState[VK_CONTROL] = 0x80;
        if (bits.alt)
            keyState[VK_MENU] = 0x80;
        if (bits.win)
            keyState[VK_LWIN] = 0x80;
    }
};

struct KeyPressInfo
{
    DWORD vkCode;
    ModifierFlags mods;

    KeyPressInfo(DWORD code) : vkCode(code), mods() {}
    KeyPressInfo(DWORD code, ModifierFlags modifiers) : vkCode(code), mods(modifiers) {}

    // Equality operator for finding and removing keys
    bool operator==(const KeyPressInfo &other) const
    {
        return vkCode == other.vkCode && mods == other.mods;
    }
};

class KeyboardChecker
{
private:
    static KeyboardChecker *s_instance;
    HWND m_mainWindow;
    HWND m_popup;
    NOTIFYICONDATA m_trayIcon;
    bool m_isRunning;
    HHOOK m_keyboardHook;
    std::vector<KeyPressInfo> m_pressedKeys;
    std::wstring m_currentText;  // Add buffer for current text
    size_t m_minTextLength;
    std::vector<HKL> m_availableLayouts;
    std::unordered_map<HKL, std::wstring> m_layoutNames;
    ModifierFlags m_currentModifiers;

    // Helper functions
    bool IsValidInLayout(const std::wstring &text, HKL layout);
    bool InitializeWindow();
    bool InitializeTrayIcon();
    void InitializeLayouts();
    void UpdateText();
    void OnKeyDown(DWORD vkCode);
    void OnKeyUp(DWORD vkCode);
    std::wstring GetLayoutName(HKL layout);
    wchar_t GetCharForKey(DWORD vk, HKL layout);
    std::wstring GetTextFromKeys(const std::vector<KeyPressInfo> &keys, HKL layout);
    void UpdatePopup(const std::wstring &currentText, const std::unordered_map<HKL, std::wstring> &conversions);
    void CleanupTrayIcon();
    void ShowTrayMenu();
    void CleanupKeyboardHook();
    bool IsModifierKey(DWORD vkCode);
    void UpdateModifierState(DWORD vkCode, bool keyDown);
    static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
    KeyboardChecker();
    ~KeyboardChecker();
    bool Start();
    void Stop();

    // Custom window messages
    static const UINT WM_UPDATE_TEXT = WM_USER + 2;
    static const UINT WM_CHECK_LAYOUT = WM_USER + 3;

    // Window class name
    static constexpr const wchar_t *WINDOW_CLASS_NAME = L"KeyboardChecker";
};
