cmake_minimum_required(VERSION 3.10)
project(keyboard_checker)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Windows subsystem
if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
endif()

# Include FetchContent for downloading dependencies
include(FetchContent)

# Add spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# Add source files
file(GLOB SOURCES "src/*.cpp")

# Add header files
include_directories(include)

# Create executable
add_executable(keyboard_checker ${SOURCES})

# Link spdlog
target_link_libraries(keyboard_checker PRIVATE spdlog::spdlog)
