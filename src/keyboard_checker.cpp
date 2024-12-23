#define UNICODE
#define _UNICODE
#include "../include/keyboard_checker.h"
#include <algorithm>
#include <sstream>

KeyboardChecker* KeyboardChecker::s_instance = nullptr;

KeyboardChecker::KeyboardChecker() 
    : m_minTextLength(3)
    , m_isRunning(false)
    , m_mainWindow(nullptr)
    , m_popup(nullptr)
    , m_keyboardHook(nullptr)
{
    FUNCTION_START;
    s_instance = this;
    InitializeLayouts();
}

KeyboardChecker::~KeyboardChecker() {
    FUNCTION_START;
    Stop();
    s_instance = nullptr;
}

void KeyboardChecker::InitializeLayouts() {
    FUNCTION_START;
    
    // Get the number of keyboard layouts
    int layoutCount = GetKeyboardLayoutList(0, nullptr);
    if (layoutCount == 0) {
        LOG(LOG_ERR, L"No keyboard layouts found");
        return;
    }

    // Get all keyboard layouts
    std::vector<HKL> layouts(layoutCount);
    GetKeyboardLayoutList(layoutCount, layouts.data());

    // Store layouts and their names
    for (HKL layout : layouts) {
        wchar_t layoutName[KL_NAMELENGTH];
        if (GetKeyboardLayoutNameW(layoutName)) {
            m_availableLayouts.push_back(layout);
            m_layoutNames[layout] = layoutName;
            LOG(LOG_INF, L"Registered layout: " + std::wstring(layoutName));
        }
    }
}

bool KeyboardChecker::InitializeWindow() {
    FUNCTION_START;

    // Register window class
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"KeyboardChecker";

    if (!RegisterClassExW(&wc)) {
        LOG(LOG_ERR, L"Failed to register window class");
        return false;
    }

    // Create hidden main window
    m_mainWindow = CreateWindowExW(
        0,
        L"KeyboardChecker",
        L"Keyboard Checker",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );

    if (!m_mainWindow) {
        LOG(LOG_ERR, L"Failed to create main window");
        return false;
    }

    // Install keyboard hook
    m_keyboardHook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandleW(nullptr),
        0
    );

    if (!m_keyboardHook) {
        LOG(LOG_ERR, L"Failed to install keyboard hook");
        return false;
    }

    return true;
}

void KeyboardChecker::Start() {
    FUNCTION_START;
    
    if (m_isRunning) {
        LOG(LOG_WRN, L"Already running");
        return;
    }

    if (!InitializeWindow()) {
        LOG(LOG_ERR, L"Failed to initialize");
        return;
    }

    m_isRunning = true;
    LOG(LOG_INF, L"Started successfully");

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void KeyboardChecker::Stop() {
    FUNCTION_START;
    
    if (!m_isRunning) {
        return;
    }

    if (m_keyboardHook) {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
    }

    if (m_popup) {
        DestroyWindow(m_popup);
        m_popup = nullptr;
    }

    if (m_mainWindow) {
        DestroyWindow(m_mainWindow);
        m_mainWindow = nullptr;
    }

    m_isRunning = false;
}

bool KeyboardChecker::IsValidInLayout(const std::wstring& text, HKL layout) {
    FUNCTION_START;
    
    // Check if all characters can be typed in this layout
    for (wchar_t ch : text) {
        bool found = false;
        // Check all virtual keys
        for (UINT vk = 0; vk < 256; vk++) {
            wchar_t layoutChar = GetCharForKey(vk, layout);
            if (layoutChar == ch) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

wchar_t KeyboardChecker::GetCharForKey(UINT vk, HKL layout) {
    BYTE keyState[256] = {0};
    wchar_t buff[2] = {0};
    
    // Check without shift
    if (ToUnicodeEx(vk, MapVirtualKeyEx(vk, MAPVK_VK_TO_VSC, layout), 
                    keyState, buff, 2, 0, layout) == 1) {
        return buff[0];
    }
    
    // Check with shift
    keyState[VK_SHIFT] = 0x80;
    if (ToUnicodeEx(vk, MapVirtualKeyEx(vk, MAPVK_VK_TO_VSC, layout), 
                    keyState, buff, 2, 0, layout) == 1) {
        return buff[0];
    }
    
    return 0;
}

std::wstring KeyboardChecker::ConvertText(const std::wstring& text, HKL fromLayout, HKL toLayout) {
    FUNCTION_START;
    
    std::wstring result;
    for (wchar_t ch : text) {
        // Find the virtual key and shift state that produces this character in fromLayout
        bool found = false;
        for (UINT vk = 0; vk < 256 && !found; vk++) {
            // Try without shift
            BYTE keyState[256] = {0};
            wchar_t buff[2] = {0};
            
            if (ToUnicodeEx(vk, MapVirtualKeyEx(vk, MAPVK_VK_TO_VSC, fromLayout), 
                           keyState, buff, 2, 0, fromLayout) == 1 && buff[0] == ch) {
                // Get corresponding character in toLayout
                ToUnicodeEx(vk, MapVirtualKeyEx(vk, MAPVK_VK_TO_VSC, toLayout), 
                           keyState, buff, 2, 0, toLayout);
                result += buff[0];
                found = true;
                break;
            }
            
            // Try with shift
            keyState[VK_SHIFT] = 0x80;
            if (ToUnicodeEx(vk, MapVirtualKeyEx(vk, MAPVK_VK_TO_VSC, fromLayout), 
                           keyState, buff, 2, 0, fromLayout) == 1 && buff[0] == ch) {
                // Get corresponding character in toLayout
                ToUnicodeEx(vk, MapVirtualKeyEx(vk, MAPVK_VK_TO_VSC, toLayout), 
                           keyState, buff, 2, 0, toLayout);
                result += buff[0];
                found = true;
                break;
            }
        }
        
        if (!found) {
            result += ch;  // Keep original character if no mapping found
        }
    }
    
    return result;
}

void KeyboardChecker::UpdatePopup(const std::wstring& currentText, 
                                const std::unordered_map<HKL, std::wstring>& conversions) {
    FUNCTION_START;
    
    // Create popup text
    std::wstringstream ss;
    ss << L"Current text: " << currentText << L"\n\nSuggestions:\n";
    for (const auto& [layout, text] : conversions) {
        ss << m_layoutNames[layout] << L": " << text << L"\n";
    }
    
    // Get cursor position
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    
    // Create or update popup window
    if (!m_popup) {
        // Register popup window class
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"KeyboardCheckerPopup";
        RegisterClassExW(&wc);
        
        // Create popup window
        m_popup = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"KeyboardCheckerPopup",
            L"Keyboard Checker",
            WS_POPUP | WS_BORDER,
            cursorPos.x, cursorPos.y + 20,
            300, 200,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr
        );
    }
    
    // Show popup
    SetWindowTextW(m_popup, ss.str().c_str());
    SetWindowPos(m_popup, HWND_TOPMOST,
                cursorPos.x, cursorPos.y + 20,
                300, 200,
                SWP_SHOWWINDOW);
}

LRESULT CALLBACK KeyboardChecker::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (s_instance) {
        switch (msg) {
            case WM_UPDATE_TEXT: {
                // Update current text
                s_instance->m_currentText = reinterpret_cast<wchar_t*>(lParam);
                
                // Check if text is long enough
                if (s_instance->m_currentText.length() >= s_instance->m_minTextLength) {
                    PostMessageW(hwnd, WM_CHECK_LAYOUT, 0, 0);
                } else if (s_instance->m_popup) {
                    ShowWindow(s_instance->m_popup, SW_HIDE);
                }
                return 0;
            }
            
            case WM_CHECK_LAYOUT: {
                // Get current keyboard layout
                HKL currentLayout = GetKeyboardLayout(0);
                
                // Check text in all layouts
                std::unordered_map<HKL, std::wstring> conversions;
                for (HKL layout : s_instance->m_availableLayouts) {
                    if (layout != currentLayout) {
                        std::wstring converted = s_instance->ConvertText(
                            s_instance->m_currentText, currentLayout, layout);
                        if (s_instance->IsValidInLayout(converted, layout)) {
                            conversions[layout] = converted;
                        }
                    }
                }
                
                // Update popup if we found any valid conversions
                if (!conversions.empty()) {
                    s_instance->UpdatePopup(s_instance->m_currentText, conversions);
                } else if (s_instance->m_popup) {
                    ShowWindow(s_instance->m_popup, SW_HIDE);
                }
                return 0;
            }
            
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK KeyboardChecker::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance) {
        KBDLLHOOKSTRUCT* kbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            // Get character for this key
            BYTE keyState[256] = {0};
            GetKeyboardState(keyState);
            
            wchar_t buff[2] = {0};
            if (ToUnicodeEx(kbd->vkCode, kbd->scanCode, keyState, buff, 2, 0, GetKeyboardLayout(0)) > 0) {
                // Update text buffer
                if (kbd->vkCode == VK_BACK && !s_instance->m_currentText.empty()) {
                    s_instance->m_currentText.pop_back();
                } else if (iswprint(buff[0])) {
                    s_instance->m_currentText += buff[0];
                }
                
                // Post message to main window to update
                PostMessageW(s_instance->m_mainWindow, WM_UPDATE_TEXT, 
                          0, reinterpret_cast<LPARAM>(s_instance->m_currentText.c_str()));
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
