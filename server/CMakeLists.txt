# RTC Server - SFU/MCU media server
cmake_minimum_required(VERSION 3.20)

# Placeholder sources (will be implemented in Phase 4)
set(RTC_SERVER_SOURCES
    src/server_placeholder.cpp
)

# Create library
add_library(rtc_server STATIC ${RTC_SERVER_SOURCES})

# Include directories
target_include_directories(rtc_server PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link to other RTC libraries
target_link_libraries(rtc_server PUBLIC rtc_core rtc_audio rtc_video)

# Future dependencies (Phase 4):
# - Prometheus client for metrics
# - Redis client for session state

target_compile_features(rtc_server PUBLIC cxx_std_20)
