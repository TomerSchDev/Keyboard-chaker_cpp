#define UNICODE
#define _UNICODE
#include "../include/keyboard_checker.h"

int main()
{
    LOG(LOG_INF, L"Started");

    KeyboardChecker checker;
    if (!checker.Start())
    {
        LOG(LOG_ERR, L"keyboard checker ended in failure");
        return 1;
    }
    LOG(LOG_INF, L"Ended");
    return 0;
}
