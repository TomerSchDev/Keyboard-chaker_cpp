# Keyboard Language Checker

A Windows application that detects keyboard layout mismatches and suggests correct text conversions in real-time.

## Features

- Real-time keyboard input monitoring
- Automatic detection of keyboard layouts
- Text conversion between different layouts
- Pop-up suggestions for text in wrong layouts
- Comprehensive logging system

## Requirements

- Windows OS
- CMake 3.10 or higher
- C++17 compatible compiler
- Visual Studio 2019 or higher (recommended)

## Building

1. Create a build directory:
```bash
mkdir build
cd build
```

2. Generate build files:
```bash
cmake ..
```

3. Build the project:
```bash
cmake --build . --config Release
```

## Usage

1. Run the executable `keyboard_checker.exe`
2. Type text normally in any application
3. If the program detects that your text might be in the wrong keyboard layout, it will show a popup with suggestions
4. The popup will show your text converted to other available keyboard layouts

## Project Structure

- `src/` - Source files
  - `keyboard_checker.cpp` - Main implementation
  - `main.cpp` - Entry point
- `include/` - Header files
  - `keyboard_checker.h` - Main class definition
  - `logger.h` - Logging system
  - `log_config.h` - Logging configuration

## Logging

The application includes a comprehensive logging system that:
- Logs to both console and file
- Supports multiple log levels (INF, WRN, ERR)
- Automatically logs function entry/exit
- Stores logs in `logs/keyboard_checker.log`
