cmake_minimum_required(VERSION 3.16)
project(kiloader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Threading support
find_package(Threads REQUIRED)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(CAPSTONE REQUIRED capstone)
pkg_check_modules(LZ4 REQUIRED liblz4)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    /opt/homebrew/include
    ${CAPSTONE_INCLUDE_DIRS}
    ${LZ4_INCLUDE_DIRS}
)

# Library directories
link_directories(
    /opt/homebrew/lib
    /opt/homebrew/Cellar/capstone/5.0.6/lib
    /opt/homebrew/opt/lz4/lib
    ${CAPSTONE_LIBRARY_DIRS}
    ${LZ4_LIBRARY_DIRS}
)

# Source files
set(SOURCES
    src/main.cpp
    src/nso_loader.cpp
    src/disassembler.cpp
    src/analyzer.cpp
    src/function_finder.cpp
    src/xref_analyzer.cpp
    src/pseudocode.cpp
    src/string_table.cpp
)

# Create executable
add_executable(kiloader ${SOURCES})

# Link libraries
target_link_libraries(kiloader
    ${CAPSTONE_LIBRARIES}
    ${LZ4_LIBRARIES}
    Threads::Threads
)

# Install
install(TARGETS kiloader DESTINATION bin)
