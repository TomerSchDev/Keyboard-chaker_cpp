#define UNICODE
#define _UNICODE
#include "../include/keyboard_checker.h"

int main() {
    FUNCTION_START;
    
    KeyboardChecker checker;
    checker.Start();
    
    return 0;
}
