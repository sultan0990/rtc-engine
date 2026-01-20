# RTC Server Library - SFU + MCU for multi-party conferencing
cmake_minimum_required(VERSION 3.20)

# Source files
set(RTC_SERVER_SOURCES
    src/rtp_forwarder.cpp
    src/room_manager.cpp
    src/subscription_manager.cpp
    src/sfu_server.cpp
    src/audio_mixer.cpp
    src/video_compositor.cpp
    src/conference_bridge.cpp
)

# Header files
set(RTC_SERVER_HEADERS
    include/rtc/server/rtp_forwarder.h
    include/rtc/server/room_manager.h
    include/rtc/server/subscription_manager.h
    include/rtc/server/sfu_server.h
    include/rtc/server/audio_mixer.h
    include/rtc/server/video_compositor.h
    include/rtc/server/conference_bridge.h
)

# Create library
add_library(rtc_server STATIC ${RTC_SERVER_SOURCES} ${RTC_SERVER_HEADERS})

# Include directories
target_include_directories(rtc_server PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link to core and video libraries
target_link_libraries(rtc_server PUBLIC rtc_core rtc_video)

# Platform-specific
if(UNIX AND NOT APPLE)
    target_link_libraries(rtc_server PRIVATE pthread)
endif()

target_compile_features(rtc_server PUBLIC cxx_std_20)
