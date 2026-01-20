cmake_minimum_required(VERSION 3.20)
project(RTC_Engine LANGUAGES CXX)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optionally enable warnings as errors
option(ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
if(ENABLE_WARNINGS_AS_ERRORS)
    if(MSVC)
        add_compile_options(/W4 /WX)
    else()
        add_compile_options(-Wall -Wextra -Werror)
    endif()
endif()

# Add subdirectories
add_subdirectory(core)
add_subdirectory(audio)
add_subdirectory(video)
add_subdirectory(server)

# Export compile commands for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
