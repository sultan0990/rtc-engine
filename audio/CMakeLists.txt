# RTC Audio Library - Opus codec, AEC, AGC, Jitter buffer
cmake_minimum_required(VERSION 3.20)

# Placeholder sources (will be implemented in Phase 2)
set(RTC_AUDIO_SOURCES
    src/audio_placeholder.cpp
)

# Create library
add_library(rtc_audio STATIC ${RTC_AUDIO_SOURCES})

# Include directories
target_include_directories(rtc_audio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link to core library
target_link_libraries(rtc_audio PUBLIC rtc_core)

# Future dependencies (Phase 2):
# find_package(Opus REQUIRED)
# target_link_libraries(rtc_audio PRIVATE Opus::opus)

target_compile_features(rtc_audio PUBLIC cxx_std_20)
