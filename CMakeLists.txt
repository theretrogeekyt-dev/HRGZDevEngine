cmake_minimum_required(VERSION 3.12)
project(HRGZDevEngine_DOOM C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Common include directories
include_directories(
    src/doom
    src/hal/common
)

# Core DOOM engine sources
file(GLOB CORE_SOURCES
    src/doom/*.c
    src/hal/common/*.c
)

# Common compiler options
if(MSVC)
    add_compile_options(/W3 /wd4996 /wd4244 /wd4267)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wno-parentheses -O2)
endif()

#------------------------------------------------------------------------------
# 1. Native Windows Win32 Driver (Zero dependencies, pure GDI + WinMM)
#------------------------------------------------------------------------------
if(WIN32)
    file(GLOB WIN32_HAL_SOURCES src/hal/win32/*.c)
    add_executable(doom_win32 ${CORE_SOURCES} ${WIN32_HAL_SOURCES})
    target_link_libraries(doom_win32 gdi32 winmm)
    set_target_properties(doom_win32 PROPERTIES OUTPUT_NAME "doom")
endif()

#------------------------------------------------------------------------------
# 2. Automated Test Runner (Headless, verified with test IWADs)
#------------------------------------------------------------------------------
file(GLOB TEST_HAL_SOURCES src/hal/test/*.c)
add_executable(doom_test ${CORE_SOURCES} ${TEST_HAL_SOURCES})
if(UNIX AND NOT APPLE)
    target_link_libraries(doom_test m)
endif()

#------------------------------------------------------------------------------
# 3. SDL2 Cross-Platform Driver (Optional)
#------------------------------------------------------------------------------
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    file(GLOB SDL_HAL_SOURCES src/hal/sdl/*.c)
    add_executable(doom_sdl ${CORE_SOURCES} ${SDL_HAL_SOURCES})
    if(TARGET SDL2::SDL2)
        target_link_libraries(doom_sdl SDL2::SDL2)
    else()
        target_include_directories(doom_sdl PRIVATE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(doom_sdl ${SDL2_LIBRARIES})
    endif()
    if(UNIX AND NOT APPLE)
        target_link_libraries(doom_sdl m)
    endif()
    message(STATUS "SDL2 DOOM target enabled.")
else()
    message(STATUS "SDL2 not found; doom_sdl target skipped. Native Win32 and Test targets available.")
endif()

#------------------------------------------------------------------------------
# 4. Native macOS Cocoa Driver (Zero dependencies, pure Cocoa + AudioToolbox)
#------------------------------------------------------------------------------
if(APPLE)
    enable_language(OBJC)
    file(GLOB MAC_HAL_SOURCES
        src/hal/mac/*.c
        src/hal/mac/*.m
    )
    add_executable(doom_mac ${CORE_SOURCES} ${MAC_HAL_SOURCES})
    target_link_libraries(doom_mac
        "-framework Cocoa"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
        "-framework Carbon"
    )
    set_target_properties(doom_mac PROPERTIES OUTPUT_NAME "doom")
endif()


