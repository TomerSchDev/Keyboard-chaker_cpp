#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "logger.h"

// Constants
const UINT WM_TRAYICON = WM_USER + 1;
const UINT ID_TRAYMENU_EXIT = 1001;

struct ModifierFlags {
    bool shift : 1;
    bool ctrl : 1;
    bool alt : 1;

    ModifierFlags() : shift(false), ctrl(false), alt(false) {}

    void UpdateFromKey(DWORD vkCode, bool keyDown) {
        switch (vkCode) {
            case VK_SHIFT:
            case VK_LSHIFT:
            case VK_RSHIFT:
                shift = keyDown;
                break;
            case VK_CONTROL:
            case VK_LCONTROL:
            case VK_RCONTROL:
                ctrl = keyDown;
                break;
            case VK_MENU:
            case VK_LMENU:
            case VK_RMENU:
                alt = keyDown;
                break;
        }
    }
};

struct KeyPressInfo {
    DWORD vkCode;
    ModifierFlags modifiers;

    KeyPressInfo(DWORD code, const ModifierFlags& mods)
        : vkCode(code), modifiers(mods) {}

    bool operator==(const KeyPressInfo& other) const {
        return vkCode == other.vkCode &&
               modifiers.shift == other.modifiers.shift &&
               modifiers.ctrl == other.modifiers.ctrl &&
               modifiers.alt == other.modifiers.alt;
    }
};

class KeyboardChecker {
protected:
    KeyboardChecker();
    
private:
    static KeyboardChecker* s_instance;
    HHOOK m_keyboardHook;
    HWND m_hwnd;
    HWND m_textWindow;
    HWND m_popup;
    NOTIFYICONDATA m_notifyIconData;
    std::vector<KeyPressInfo> m_pressedKeys;
    std::wstring m_currentText;
    size_t m_minTextLength;
    std::vector<HKL> m_availableLayouts;
    ModifierFlags m_currentModifiers;
    bool m_isShiftPressed;
    bool m_isCtrlPressed;
    bool m_isAltPressed;
    bool m_isRunning;

    ~KeyboardChecker();
    
    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    
    void OnKeyDown(DWORD vkCode);
    void OnKeyUp(DWORD vkCode);
    void UpdateText(const std::wstring& text);
    void UpdateModifierState(DWORD vkCode, bool isKeyDown);
    bool IsModifierKey(DWORD vkCode);
    std::wstring GetTextFromKeys(const std::vector<KeyPressInfo>& keys, HKL layout);
    wchar_t GetCharForKey(DWORD vkCode, HKL layout);
    std::wstring GetLayoutName(HKL layout);
    void ShowTrayMenu();
    void UpdatePopup(const std::wstring& currentText, const std::unordered_map<HKL, std::wstring>& conversions);
    void InitializeLayouts();
    void CleanupKeyboardHook();
    bool IsValidInLayout(const std::wstring& text, HKL layout);
    
    bool InitializeWindow();

public:
    static KeyboardChecker* GetInstance();
    static void DeleteInstance();
    
    bool Start();
    void Stop();
};
