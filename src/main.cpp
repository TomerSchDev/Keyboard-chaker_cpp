#define UNICODE
#define _UNICODE
#include "../include/keyboard_checker.h"
#include <logger.h>
#include "keyboard_checker.h"
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    KeyboardChecker* checker = KeyboardChecker::GetInstance();
    if (!checker)
    {
        LOG(ERR, L"Failed to get keyboard checker instance");
        return 1;
    }

    if (!checker->Start())
    {
        LOG(ERR, L"keyboard checker ended in failure");
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    KeyboardChecker::DeleteInstance();
    return (int)msg.wParam;
}
