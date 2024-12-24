#define UNICODE
#define _UNICODE
#include "../include/keyboard_checker.h"
#include <algorithm>
#include <sstream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

KeyboardChecker *KeyboardChecker::s_instance = nullptr;

KeyboardChecker::KeyboardChecker()
    : m_minTextLength(3), m_isRunning(false), m_mainWindow(nullptr), m_popup(nullptr), m_keyboardHook(nullptr)
{
    FUNCTION_START;

    ZeroMemory(&m_trayIcon, sizeof(m_trayIcon));
    s_instance = this;
    InitializeLayouts();
}

KeyboardChecker::~KeyboardChecker()
{
    FUNCTION_START;

    if (m_keyboardHook)
    {
        UnhookWindowsHookEx(m_keyboardHook);
    }
    if (m_popup)
    {
        DestroyWindow(m_popup);
    }
    if (m_mainWindow)
    {
        CleanupTrayIcon();
        DestroyWindow(m_mainWindow);
    }
    s_instance = nullptr;
}

void KeyboardChecker::InitializeLayouts()
{
    FUNCTION_START;

    // Get the number of keyboard layouts
    int layoutCount = GetKeyboardLayoutList(0, NULL);
    LOG(LOG_INF, L"Found " + std::to_wstring(layoutCount) + L" keyboard layouts");

    // Get all keyboard layouts
    std::vector<HKL> layouts(layoutCount);
    GetKeyboardLayoutList(layoutCount, layouts.data());

    for (HKL layout : layouts)
    {
        // Get the keyboard layout name
        wchar_t layoutName[KL_NAMELENGTH];
        GetKeyboardLayoutName(layoutName);

        // Get language ID info
        WORD primaryLangID = PRIMARYLANGID(LOWORD((DWORD_PTR)layout));
        WORD subLangID = SUBLANGID(LOWORD((DWORD_PTR)layout));

        LOG(LOG_INF, L"Processing layout with ID: 0x" + std::to_wstring((DWORD_PTR)layout));
        LOG(LOG_INF, L"Registered layout with keyboard name: " + std::wstring(layoutName) +
            L", Primary Language ID: 0x" + std::to_wstring(primaryLangID) +
            L", Sub Language ID: 0x" + std::to_wstring(subLangID));

        m_availableLayouts.push_back(layout);
        ActivateKeyboardLayout(layout, 0); // Load the layout
    }

    LOG(LOG_INF, L"Total layouts registered: " + std::to_wstring(m_availableLayouts.size()));
    LOG(LOG_INF, L"Registered layouts:");
    for (HKL layout : m_availableLayouts)
    {
        LOG(LOG_INF, L"  Layout 0x" + std::to_wstring((DWORD_PTR)layout));
    }
}

bool KeyboardChecker::InitializeWindow()
{
    FUNCTION_START;

    // Create window class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"KeyboardCheckerClass";

    if (!RegisterClassEx(&wc))
    {
        LOG(LOG_ERR, L"Failed to register window class. Error: " + std::to_wstring(GetLastError()));
        return false;
    }

    // Create message-only window
    m_mainWindow = CreateWindowEx(
        0,
        L"KeyboardCheckerClass",
        L"KeyboardChecker",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        NULL,
        GetModuleHandle(NULL),
        this);

    if (m_mainWindow == NULL)
    {
        LOG(LOG_ERR, L"Failed to create window. Error: " + std::to_wstring(GetLastError()));
        return false;
    }

    return true;
}

bool KeyboardChecker::InitializeTrayIcon()
{
    FUNCTION_START;

    m_trayIcon.cbSize = sizeof(NOTIFYICONDATA);
    m_trayIcon.hWnd = m_mainWindow;
    m_trayIcon.uID = 1;
    m_trayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_trayIcon.uCallbackMessage = WM_TRAYICON;
    m_trayIcon.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(m_trayIcon.szTip, L"Keyboard Language Checker");

    if (!Shell_NotifyIcon(NIM_ADD, &m_trayIcon))
    {
        LOG(LOG_ERR, L"Failed to add tray icon. Error: " + std::to_wstring(GetLastError()));
        return false;
    }

    return true;
}

void KeyboardChecker::CleanupTrayIcon()
{
    FUNCTION_START;

    Shell_NotifyIcon(NIM_DELETE, &m_trayIcon);
    if (m_trayIcon.hIcon)
    {
        DestroyIcon(m_trayIcon.hIcon);
    }
}

void KeyboardChecker::ShowTrayMenu()
{
    FUNCTION_START;

    POINT pt;
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    AppendMenu(menu, MF_STRING, ID_TRAYMENU_EXIT, L"Exit");

    // Required to make the menu disappear when clicking outside
    SetForegroundWindow(m_mainWindow);

    TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN,
                   pt.x, pt.y, 0, m_mainWindow, nullptr);

    PostMessage(m_mainWindow, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

bool KeyboardChecker::Start()
{
    FUNCTION_START;

    if (m_isRunning)
    {
        LOG(LOG_WRN, L"Already running");
        return true;
    }

    if (!InitializeWindow())
    {
        LOG(LOG_ERR, L"Failed to initialize window");
        return false;
    }

    if (!InitializeTrayIcon())
    {
        LOG(LOG_ERR, L"Failed to initialize tray icon");
        return false;
    }

    // Set up keyboard hook
    m_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (!m_keyboardHook)
    {
        LOG(LOG_ERR, L"Failed to set keyboard hook. Error: " + std::to_wstring(GetLastError()));
        return false;
    }
    LOG(LOG_INF, L"Keyboard hook set successfully");

    m_isRunning = true;
    LOG(LOG_INF, L"Started successfully");

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    m_isRunning = false;
    LOG(LOG_INF, L"Message loop ended");
    return true;
}

void KeyboardChecker::Stop()
{
    FUNCTION_START;

    if (!m_isRunning)
    {
        LOG(LOG_WRN, L"Already stopped");
        return;
    }

    CleanupKeyboardHook();
    CleanupTrayIcon();

    if (m_mainWindow)
    {
        DestroyWindow(m_mainWindow);
        m_mainWindow = NULL;
    }

    if (m_popup)
    {
        DestroyWindow(m_popup);
        m_popup = NULL;
    }

    m_isRunning = false;
}

void KeyboardChecker::CleanupKeyboardHook()
{
    FUNCTION_START;
    if (m_keyboardHook)
    {
        if (UnhookWindowsHookEx(m_keyboardHook))
        {
            LOG(LOG_INF, L"Keyboard hook removed successfully");
        }
        else
        {
            LOG(LOG_ERR, L"Failed to remove keyboard hook. Error: " + std::to_wstring(GetLastError()));
        }
        m_keyboardHook = NULL;
    }
}

std::wstring KeyboardChecker::GetTextFromKeys(const std::vector<KeyPressInfo> &keys, HKL layout)
{
    FUNCTION_START;
    std::wstring result;

    for (const auto &keyPress : keys)
    {
        // Create keyboard state array
        BYTE keyState[256] = {0};

        // Set modifier states from the bit flags
        keyPress.mods.ToKeyboardState(keyState);

        // Get the character
        wchar_t buff[5] = {0};
        int ret = ToUnicodeEx(keyPress.vkCode, 0, keyState, buff, 4, 0, layout);
        if (ret > 0)
        {
            result += buff;
        }
    }

    LOG(LOG_INF, L"Generated text: " + result);
    return result;
}

bool KeyboardChecker::IsValidInLayout(const std::wstring &text, HKL layout)
{
    FUNCTION_START;
    LANGID langId = LOWORD(layout);
    LOG(LOG_INF, L"Checking if text '" + text + L"' is valid in layout 0x" +
                     std::to_wstring(langId));

    if (text.empty())
    {
        return true;
    }

    bool isHebrewLayout = (PRIMARYLANGID(langId) == LANG_HEBREW);

    // For Hebrew layout, check if all characters are Hebrew
    if (isHebrewLayout)
    {
        for (wchar_t ch : text)
        {
            if (ch != L' ' && ch != L',' && ch != L'.' && ch != L'/' && ch != L'-')
            {
                if (ch < 0x05D0 || ch > 0x05EA)
                {
                    return false;
                }
            }
        }
        return true;
    }

    // For other layouts, consider it valid if we could map the keys to characters
    return true;
}

void KeyboardChecker::UpdateText()
{
    FUNCTION_START;
    HKL currentLayout = GetKeyboardLayout(0);
    LOG(LOG_INF, L"Current keyboard layout ID: 0x" + std::to_wstring((DWORD_PTR)currentLayout));
    
    std::wstring layoutName = GetLayoutName(currentLayout);
    LOG(LOG_INF, L"Current keyboard layout: " + layoutName);

    if (m_currentText.length() >= m_minTextLength)
    {
        LOG(LOG_INF, L"Current text: " + m_currentText);
        std::vector<std::pair<HKL, std::wstring>> conversions;
        
        // Try converting the text with other layouts
        for (const auto& layout : m_availableLayouts)
        {
            if (layout != currentLayout)
            {
                std::wstring convertedText;
                bool hasConversion = false;
                
                // For each character in the text, try to find its virtual key code
                // in the current layout and convert it using the target layout
                for (wchar_t ch : m_currentText)
                {
                    // Skip spaces and punctuation
                    if (iswspace(ch) || iswpunct(ch))
                    {
                        convertedText += ch;
                        continue;
                    }
                    
                    // Get the virtual key code for this character in the current layout
                    SHORT vk = VkKeyScanEx(ch, currentLayout);
                    if (vk == -1)
                    {
                        // Try getting the virtual key code from the target layout
                        vk = VkKeyScanEx(ch, layout);
                        if (vk == -1)
                        {
                            LOG(LOG_ERR, L"Failed to get virtual key code for character: " + std::wstring(1, ch));
                            continue;
                        }
                    }
                    
                    // Extract just the virtual key code (lower byte)
                    BYTE virtualKey = LOBYTE(vk);
                    
                    // Get the character that would be produced by this key in the target layout
                    wchar_t convertedChar = GetCharForKey(virtualKey, layout);
                    if (convertedChar != 0)
                    {
                        convertedText += convertedChar;
                        if (convertedChar != ch)
                        {
                            hasConversion = true;
                        }
                    }
                }
                
                if (!convertedText.empty() && hasConversion)
                {
                    LOG(LOG_INF, L"Found conversion in layout 0x" + 
                        std::to_wstring((DWORD_PTR)layout) + L": " + convertedText);
                    conversions.push_back({layout, convertedText});
                }
            }
        }
        
        LOG(LOG_INF, L"Found " + std::to_wstring(conversions.size()) + L" conversions");
        
        // Log conversions
        for (const auto& conversion : conversions)
        {
            std::wstring layoutName = GetLayoutName(conversion.first);
            LOG(LOG_INF, L"Conversion in " + layoutName + L": " + conversion.second);
        }

        // If we found any conversions, show them in the popup
        if (!conversions.empty())
        {
            std::unordered_map<HKL, std::wstring> convMap;
            for (const auto& conv : conversions)
            {
                convMap[conv.first] = conv.second;
            }
            UpdatePopup(m_currentText, convMap);
        }
    }
}

std::wstring KeyboardChecker::GetLayoutName(HKL layout)
{
    FUNCTION_START;

    if (layout == NULL)
    {
        LOG(LOG_ERR, L"Layout handle is NULL");
        return L"Unknown Layout";
    }

    // Get locale info
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
    if (LCIDToLocaleName(MAKELCID(LOWORD(layout), SORT_DEFAULT),
                         localeName,
                         LOCALE_NAME_MAX_LENGTH,
                         0) == 0)
    {
        LOG(LOG_ERR, L"Failed to get locale name. Error: " + std::to_wstring(GetLastError()));
        return std::wstring(L"Layout 0x") + std::to_wstring((DWORD)(DWORD_PTR)layout & 0xFFFF);
    }

    std::wstring result(localeName);
    LOG(LOG_INF, L"Successfully got layout name: " + result);
    return result;
}

wchar_t KeyboardChecker::GetCharForKey(DWORD vk, HKL layout)
{
    FUNCTION_START;

    if (layout == NULL)
    {
        LOG(LOG_ERR, L"Layout handle is NULL");
        return 0;
    }

    // Get the current keyboard state
    BYTE keyState[256] = {0};
    if (!GetKeyboardState(keyState))
    {
        LOG(LOG_ERR, L"Failed to get keyboard state");
        return 0;
    }

    // Set the key as pressed in the state array
    keyState[vk] = 0x80;

    // Get scan code for the virtual key
    UINT scanCode = MapVirtualKeyEx(vk, MAPVK_VK_TO_VSC, layout);
    if (scanCode == 0)
    {
        LOG(LOG_ERR, L"Failed to get scan code for virtual key: 0x" + std::to_wstring(vk));
        return 0;
    }

    wchar_t outChar[5] = {0};
    int result = ToUnicodeEx(vk, scanCode, keyState, outChar, 5, 0, layout);
    
    if (result <= 0)
    {
        // Try with a different scan code mapping
        scanCode = MapVirtualKeyEx(vk, MAPVK_VK_TO_VSC_EX, layout);
        if (scanCode != 0)
        {
            result = ToUnicodeEx(vk, scanCode, keyState, outChar, 5, 0, layout);
        }
    }

    if (result <= 0)
    {
        LOG(LOG_ERR, L"ToUnicodeEx failed with result: " + std::to_wstring(result) + 
            L" for VK: 0x" + std::to_wstring(vk) + 
            L", Layout: 0x" + std::to_wstring((DWORD_PTR)layout));
        return 0;
    }

    LOG(LOG_INF, L"Successfully got char for key: " + std::wstring(outChar, result) + 
        L" (VK: 0x" + std::to_wstring(vk) + 
        L", Layout: 0x" + std::to_wstring((DWORD_PTR)layout) + L")");
    return outChar[0];
}

void KeyboardChecker::UpdatePopup(const std::wstring &currentText,
                                  const std::unordered_map<HKL, std::wstring> &conversions)
{
    FUNCTION_START;

    if (!m_popup || conversions.empty())
    {
        return;
    }

    std::wstring message = L"Did you mean to type:\n";
    for (const auto &[layout, text] : conversions)
    {
        message += text + L"\n";
    }

    RECT rect;
    GetWindowRect(m_popup, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    // Get the cursor position for the popup
    POINT cursorPos;
    GetCursorPos(&cursorPos);

    // Update the window text
    SetWindowText(m_popup, message.c_str());

    // Move the popup near the cursor
    SetWindowPos(m_popup, HWND_TOPMOST,
                 cursorPos.x, cursorPos.y + 20,
                 width, height,
                 SWP_NOSIZE | SWP_SHOWWINDOW);

    UpdateWindow(m_popup);
}

LRESULT CALLBACK KeyboardChecker::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    FUNCTION_START;

    if (s_instance)
    {
        switch (msg)
        {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP)
            {
                s_instance->ShowTrayMenu();
            }
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_TRAYMENU_EXIT)
            {
                PostQuitMessage(0);
            }
            return 0;

        case WM_UPDATE_TEXT:
            s_instance->UpdateText();
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK KeyboardChecker::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    FUNCTION_START;

    if (nCode != HC_ACTION || !s_instance)
    {
        LOG(LOG_INF, L"Skipping hook, nCode: " + std::to_wstring(nCode));
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    try
    {
        KBDLLHOOKSTRUCT *kbd = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if (!kbd)
        {
            LOG(LOG_ERR, L"Invalid keyboard hook data");
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }

        LOG(LOG_INF, L"Key event - wParam: 0x" + std::to_wstring(wParam) + L", vkCode: 0x" + std::to_wstring(kbd->vkCode));

        switch (wParam)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            s_instance->OnKeyDown(kbd->vkCode);
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            s_instance->OnKeyUp(kbd->vkCode);
            break;
        }
    }
    catch (const std::exception &e)
    {
        LOG(LOG_ERR, L"Exception in keyboard hook: " + std::wstring(L"Exception in keyboard hook"));
    }
    catch (...)
    {
        LOG(LOG_ERR, L"Unknown exception in keyboard hook");
    }

    LOG(LOG_INF, L"Hook processed successfully");
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void KeyboardChecker::UpdateModifierState(DWORD vkCode, bool keyDown)
{
    m_currentModifiers.UpdateFromKey(vkCode, keyDown);
}

void KeyboardChecker::OnKeyDown(DWORD vkCode)
{
    FUNCTION_START;
    LOG(LOG_INF, L"Key down: 0x" + std::to_wstring(vkCode));

    // Update modifier state
    UpdateModifierState(vkCode, true);

    // Add non-modifier keys to pressed keys
    if (!IsModifierKey(vkCode))
    {
        KeyPressInfo newKey(vkCode, m_currentModifiers);
        m_pressedKeys.push_back(newKey);
        LOG(LOG_INF, L"Added key 0x" + std::to_wstring(vkCode) + L" with modifiers " + std::to_wstring(m_currentModifiers.value));
        
        // Get text for the new key and append it
        std::vector<KeyPressInfo> keys = {newKey};
        std::wstring newText = GetTextFromKeys(keys, GetKeyboardLayout(0));
        if (!newText.empty())
        {
            m_currentText += newText;
            LOG(LOG_INF, L"Current text buffer: " + m_currentText);
        }
        
        UpdateText();
    }
}

void KeyboardChecker::OnKeyUp(DWORD vkCode)
{
    FUNCTION_START;
    LOG(LOG_INF, L"Key up: 0x" + std::to_wstring(vkCode));

    // Update modifier state
    UpdateModifierState(vkCode, false);

    // Remove non-modifier keys
    if (!IsModifierKey(vkCode))
    {
        KeyPressInfo keyToRemove(vkCode, m_currentModifiers);
        m_pressedKeys.erase(
            std::remove(m_pressedKeys.begin(), m_pressedKeys.end(), keyToRemove),
            m_pressedKeys.end());
        LOG(LOG_INF, L"Removed key 0x" + std::to_wstring(vkCode));
        UpdateText();
    }
}

bool KeyboardChecker::IsModifierKey(DWORD vkCode)
{
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
