#define UNICODE
#define _UNICODE
#include "../include/keyboard_checker.h"
#include <algorithm>
#include <sstream>
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "logger.h"

// Static helper function for string conversion
static std::wstring ToWString(const std::string &str)
{
    if (str.empty())
        return L"";

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    if (size_needed <= 0)
        return L"";

    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size_needed);
    return result;
}

// Helper function to check if a character is Hebrew
static bool IsHebrewChar(wchar_t ch)
{
    return (ch >= 0x0590 && ch <= 0x05FF);  // Hebrew Unicode range
}

// Helper function to get layout primary language ID
static WORD GetLayoutPrimaryLangID(HKL layout)
{
    return PRIMARYLANGID(LOWORD(layout));
}

// Helper function to check if a layout is Hebrew
static bool IsHebrewLayout(HKL layout)
{
    return GetLayoutPrimaryLangID(layout) == LANG_HEBREW;
}

KeyboardChecker *KeyboardChecker::s_instance = nullptr;

KeyboardChecker::KeyboardChecker()
    : m_keyboardHook(NULL),
      m_hwnd(NULL),
      m_textWindow(NULL),
      m_minTextLength(3),
      m_isShiftPressed(false),
      m_isCtrlPressed(false),
      m_isAltPressed(false),
      m_isRunning(false)
{
    LOG(INF, L"Initializing KeyboardChecker");
    ZeroMemory(&m_notifyIconData, sizeof(m_notifyIconData));
    s_instance = this;
    InitializeLayouts();
}

KeyboardChecker::~KeyboardChecker()
{
    LOG(INF, L"Destroying KeyboardChecker");
    Stop();
    s_instance = nullptr;
}

KeyboardChecker *KeyboardChecker::GetInstance()
{
    if (!s_instance)
    {
        s_instance = new KeyboardChecker();
    }
    return s_instance;
}

void KeyboardChecker::DeleteInstance()
{
    if (s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

void KeyboardChecker::InitializeLayouts()
{
    LOG(INF, L"Initializing keyboard layouts");
    
    // Get the number of keyboard layouts
    int layoutCount = GetKeyboardLayoutList(0, NULL);
    if (layoutCount > 0)
    {
        // Get all layouts
        std::vector<HKL> layouts(layoutCount);
        GetKeyboardLayoutList(layoutCount, layouts.data());
        
        for (HKL layout : layouts)
        {
            // Get layout info
            wchar_t layoutName[KL_NAMELENGTH];
            if (GetKeyboardLayoutName(layoutName))
            {
                m_availableLayouts.push_back(layout);
                
                std::wstring langName = GetLayoutName(layout);
                LOG(DBG, L"Found keyboard layout: " + langName + 
                    L" (0x" + std::to_wstring((DWORD_PTR)layout) + L")" +
                    L" Primary Lang ID: 0x" + std::to_wstring(GetLayoutPrimaryLangID(layout)));
                
                if (IsHebrewLayout(layout))
                {
                    LOG(INF, L"Hebrew layout detected");
                }
            }
        }
    }
    else
    {
        LOG(ERR, L"No keyboard layouts found");
    }
}

bool KeyboardChecker::InitializeWindow()
{
    LOG(INF, L"Initializing window");

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"KeyboardCheckerClass";

    if (!RegisterClassExW(&wc))
    {
        LOG(ERR, L"Failed to register window class");
        return false;
    }

    // Create main window
    m_hwnd = CreateWindowExW(
        0,
        L"KeyboardCheckerClass",
        L"Keyboard Checker",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 300,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL);

    if (!m_hwnd)
    {
        LOG(ERR, L"Failed to create main window");
        return false;
    }

    // Create text display window
    m_textWindow = CreateWindowExW(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        10, 10,
        380, 280,
        m_hwnd,
        NULL,
        GetModuleHandle(NULL),
        NULL);

    if (!m_textWindow)
    {
        LOG(ERR, L"Failed to create text window");
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
        return false;
    }

    // Show the windows
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    return true;
}

void KeyboardChecker::UpdateText(const std::wstring &text)
{
    LOG(DBG, L"Updating text: " + text);

    // Get current layout name
    wchar_t layoutName[KL_NAMELENGTH];
    if (!GetKeyboardLayoutNameW(layoutName))
    {
        LOG(ERR, L"Failed to get keyboard layout name");
        return;
    }
    LOG(DBG, L"Current keyboard layout: " + std::wstring(layoutName));

    try
    {
        std::vector<std::wstring> conversions;
        HKL currentLayout = GetKeyboardLayout(0);

        if (text.empty())
        {
            return;
        }

        LOG(DBG, L"Inside the if statement");
        for (const auto &key : m_pressedKeys)
        {
            wchar_t converted = GetCharForKey(key.vkCode, currentLayout);
            if (converted != 0)
            {
                std::wstring convertedStr(1, converted);
                LOG(DBG, L"Converted key 0x" + std::to_wstring(key.vkCode) + L" to: " + convertedStr);
                conversions.push_back(convertedStr);
            }
        }

        LOG(DBG, L"Found " + std::to_wstring(conversions.size()) + L" conversions");
        if (!conversions.empty())
        {
            std::wstring convertedText;
            for (const auto &conv : conversions)
            {
                convertedText += conv;
            }
            LOG(DBG, L"Setting text to: " + convertedText);

            // Use UTF-16 for text display
            if (m_textWindow && IsWindow(m_textWindow))
            {
                if (!SetWindowTextW(m_textWindow, convertedText.c_str()))
                {
                    DWORD error = GetLastError();
                    LOG(ERR, L"Failed to set window text. Error: " + std::to_wstring(error));
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        LOG(ERR, L"Exception during conversion: " + ToWString(e.what()));
    }
}

bool KeyboardChecker::Start()
{
    LOG(INF, L"Starting KeyboardChecker");

    // Initialize logger with 10MB max size
    Logger::Instance().Initialize(L"logs/keyboard_checker.log", 10 * 1024 * 1024);

    if (m_isRunning)
    {
        LOG(WRN, L"Already running");
        return false;
    }

    if (!InitializeWindow())
    {
        LOG(ERR, L"Failed to initialize window");
        return false;
    }

    // Set up keyboard hook
    m_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (!m_keyboardHook)
    {
        LOG(ERR, L"Failed to set keyboard hook. Error: " + std::to_wstring(GetLastError()));
        return false;
    }
    LOG(INF, L"Keyboard hook set successfully");

    m_isRunning = true;
    LOG(INF, L"Started successfully");

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    m_isRunning = false;
    LOG(INF, L"Message loop ended");
    return true;
}

void KeyboardChecker::Stop()
{
    LOG(INF, L"Stopping KeyboardChecker");
    if (!m_isRunning)
    {
        LOG(WRN, L"Already stopped");
        return;
    }

    CleanupKeyboardHook();

    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
    }

    if (m_textWindow)
    {
        DestroyWindow(m_textWindow);
        m_textWindow = NULL;
    }

    m_isRunning = false;
}

void KeyboardChecker::CleanupKeyboardHook()
{
    LOG(DBG, L"Cleaning up keyboard hook");
    if (m_keyboardHook)
    {
        if (UnhookWindowsHookEx(m_keyboardHook))
        {
            LOG(INF, L"Keyboard hook removed successfully");
        }
        else
        {
            LOG(ERR, L"Failed to remove keyboard hook. Error: " + std::to_wstring(GetLastError()));
        }
        m_keyboardHook = NULL;
    }
}

std::wstring KeyboardChecker::GetTextFromKeys(const std::vector<KeyPressInfo> &keys, HKL layout)
{
    LOG(DBG, L"Getting text from keys");
    std::wstring text;
    for (const auto &key : keys)
    {
        wchar_t ch = GetCharForKey(key.vkCode, layout);
        if (ch != 0)
        {
            text += ch;
        }
    }
    LOG(DBG, L"Generated text: " + text);
    return text;
}

bool KeyboardChecker::IsValidInLayout(const std::wstring &text, HKL layout)
{
    if (text.empty())
    {
        LOG(DBG, L"Text is empty");
        return true;
    }

    LOG(DBG, L"Checking if text '" + text + L"' is valid in layout 0x" + 
        std::to_wstring((DWORD_PTR)layout) + L" (Primary: 0x" + 
        std::to_wstring(GetLayoutPrimaryLangID(layout)) + L")");

    bool isHebrewLayout = IsHebrewLayout(layout);
    if (isHebrewLayout)
    {
        LOG(DBG, L"Checking Hebrew layout");
        // For Hebrew layout, we accept both Hebrew and ASCII characters
        for (wchar_t ch : text)
        {
            if (!IsHebrewChar(ch) && ch > 0x7F && ch != L' ')  // Allow spaces and ASCII
            {
                LOG(WRN, L"Character '" + std::wstring(1, ch) + 
                    L"' (0x" + std::to_wstring(ch) + L") is not valid in Hebrew layout");
                return false;
            }
        }
    }
    else
    {
        LOG(DBG, L"Checking English layout");
        // For English layout, we only accept ASCII characters
        for (wchar_t ch : text)
        {
            if (ch > 0x7F && ch != L' ')  // Allow spaces
            {
                LOG(WRN, L"Character '" + std::wstring(1, ch) + 
                    L"' (0x" + std::to_wstring(ch) + L") is not valid in English layout");
                return false;
            }
        }
    }

    LOG(DBG, L"All characters are valid in layout");
    return true;
}

LRESULT CALLBACK KeyboardChecker::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    LOG(DBG, L"Low-level keyboard hook");
    if (nCode < 0)
    {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    KeyboardChecker *instance = KeyboardChecker::GetInstance();
    if (!instance)
    {
        LOG(ERR, L"KeyboardChecker instance is null");
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    KBDLLHOOKSTRUCT *hookStruct = (KBDLLHOOKSTRUCT *)lParam;
    if (!hookStruct)
    {
        LOG(ERR, L"Hook struct is null");
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    switch (wParam)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        instance->OnKeyDown(hookStruct->vkCode);
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        instance->OnKeyUp(hookStruct->vkCode);
        break;
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK KeyboardChecker::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LOG(DBG, L"Window procedure");
    KeyboardChecker *instance = KeyboardChecker::GetInstance();
    if (!instance)
    {
        LOG(ERR, L"KeyboardChecker instance is null");
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    switch (message)
    {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP)
        {
            instance->ShowTrayMenu();
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAYMENU_EXIT)
        {
            PostQuitMessage(0);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

void KeyboardChecker::UpdateModifierState(DWORD vkCode, bool keyDown)
{
    LOG(DBG, L"Updating modifier state");
    m_currentModifiers.UpdateFromKey(vkCode, keyDown);
}

void KeyboardChecker::OnKeyDown(DWORD vkCode)
{
    LOG(DBG, L"Key down: 0x" + std::to_wstring(vkCode));

    if (IsModifierKey(vkCode))
    {
        UpdateModifierState(vkCode, true);
        return;
    }

    // Add the key to pressed keys if not already present
    KeyPressInfo newKey(vkCode, m_currentModifiers);
    auto it = std::find(m_pressedKeys.begin(), m_pressedKeys.end(), newKey);
    if (it == m_pressedKeys.end())
    {
        m_pressedKeys.push_back(newKey);
        std::wstring newText = GetTextFromKeys(m_pressedKeys, GetKeyboardLayout(0));
        UpdateText(newText);
    }
}

void KeyboardChecker::OnKeyUp(DWORD vkCode)
{
    LOG(DBG, L"Key up: 0x" + std::to_wstring(vkCode));

    if (IsModifierKey(vkCode))
    {
        UpdateModifierState(vkCode, false);
        return;
    }

    // Remove the key from pressed keys
    KeyPressInfo keyToRemove(vkCode, m_currentModifiers);
    auto it = std::find(m_pressedKeys.begin(), m_pressedKeys.end(), keyToRemove);
    if (it != m_pressedKeys.end())
    {
        m_pressedKeys.erase(it);
        std::wstring newText = GetTextFromKeys(m_pressedKeys, GetKeyboardLayout(0));
        UpdateText(newText);
    }
}

bool KeyboardChecker::IsModifierKey(DWORD vkCode)
{
    LOG(DBG, L"Checking if key is modifier");
    switch (vkCode)
    {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
        return true;
    default:
        return false;
    }
}

std::wstring KeyboardChecker::GetLayoutName(HKL layout)
{
    LOG(DBG, L"Getting layout name");
    if (layout == NULL)
    {
        LOG(ERR, L"Layout handle is NULL");
        return L"Unknown Layout";
    }

    // Get locale info
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
    if (LCIDToLocaleName(MAKELCID(LOWORD(layout), SORT_DEFAULT),
                         localeName,
                         LOCALE_NAME_MAX_LENGTH,
                         0) == 0)
    {
        LOG(ERR, L"Failed to get locale name. Error: " + std::to_wstring(GetLastError()));
        return std::wstring(L"Layout 0x") + std::to_wstring((DWORD)(DWORD_PTR)layout & 0xFFFF);
    }

    std::wstring result(localeName);
    LOG(DBG, L"Successfully got layout name: " + result);
    return result;
}

wchar_t KeyboardChecker::GetCharForKey(DWORD vkCode, HKL layout)
{
    LOG(DBG, L"Getting char for VK: 0x" + std::to_wstring(vkCode) + 
        L" in layout: 0x" + std::to_wstring((DWORD_PTR)layout) +
        L" Primary Lang ID: 0x" + std::to_wstring(GetLayoutPrimaryLangID(layout)));

    // Handle special keys
    if (vkCode == VK_SPACE)
    {
        LOG(DBG, L"Space key detected");
        return L' ';
    }

    // Skip processing for control keys
    if (vkCode == VK_RETURN || vkCode == VK_TAB || vkCode == VK_BACK ||
        vkCode == VK_DELETE || vkCode == VK_ESCAPE ||
        (vkCode >= VK_F1 && vkCode <= VK_F24))
    {
        LOG(DBG, L"Control key detected");
        return 0;
    }

    // Check for invalid virtual key code
    if (vkCode > 0xFF && vkCode != VK_OEM_COMMA && vkCode != VK_OEM_PERIOD && vkCode != VK_OEM_MINUS)
    {
        LOG(ERR, L"Invalid virtual key code: 0x" + std::to_wstring(vkCode));
        return 0;
    }

    // Get keyboard state
    BYTE keyboardState[256] = {};
    if (!GetKeyboardState(keyboardState))
    {
        LOG(ERR, L"Failed to get keyboard state");
        return 0;
    }

    // Set shift state based on our modifier tracking
    if (m_currentModifiers.shift)
    {
        keyboardState[VK_SHIFT] = 0x80;
    }

    // Get scan code
    UINT scanCode = MapVirtualKeyEx(vkCode & 0xFF, MAPVK_VK_TO_VSC, layout);
    if (scanCode == 0)
    {
        LOG(ERR, L"Failed to map virtual key to scan code");
        return 0;
    }

    LOG(DBG, L"Mapped to scan code: 0x" + std::to_wstring(scanCode));

    // Create buffer for the result
    wchar_t result[32] = {};

    int ret = ToUnicodeEx(vkCode & 0xFF, scanCode, keyboardState, result, 32, 0, layout);
    LOG(DBG, L"ToUnicodeEx returned: " + std::to_wstring(ret));

    if (ret > 0)
    {
        // Check if we got a Hebrew character
        if (IsHebrewChar(result[0]))
        {
            LOG(DBG, L"Hebrew character detected: " + std::wstring(1, result[0]));
        }
        
        std::wstring converted(result, ret);
        LOG(DBG, L"Converted to character(s): " + converted);
        
        return result[0];
    }
    else if (ret == -1)
    {
        // Clear dead key state
        wchar_t deadKeyResult[32] = {};
        ToUnicodeEx(vkCode & 0xFF, scanCode, keyboardState, deadKeyResult, 32, 0, layout);
    }

    LOG(ERR, L"Failed to convert key to character");
    return 0;
}

void KeyboardChecker::ShowTrayMenu()
{
    LOG(DBG, L"Showing tray menu");
    POINT pt;
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    AppendMenu(menu, MF_STRING, ID_TRAYMENU_EXIT, L"Exit");

    // Required to make the menu disappear when clicking outside
    SetForegroundWindow(m_hwnd);

    TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN,
                   pt.x, pt.y, 0, m_hwnd, nullptr);

    PostMessage(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}
