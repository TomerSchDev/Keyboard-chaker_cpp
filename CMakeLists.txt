cmake_minimum_required(VERSION 3.10)
project(keyboard_checker)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
file(GLOB SOURCES "src/*.cpp")

# Add header files
include_directories(include)

# Create executable
add_executable(keyboard_checker ${SOURCES})
