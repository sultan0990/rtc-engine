# RTC Video Library - H.264/VP8 codec, capture, frame processing
cmake_minimum_required(VERSION 3.20)

# Placeholder sources (will be implemented in Phase 3)
set(RTC_VIDEO_SOURCES
    src/video_placeholder.cpp
)

# Create library
add_library(rtc_video STATIC ${RTC_VIDEO_SOURCES})

# Include directories
target_include_directories(rtc_video PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link to core library
target_link_libraries(rtc_video PUBLIC rtc_core)

# Future dependencies (Phase 3):
# find_package(FFmpeg REQUIRED COMPONENTS avcodec avformat avutil)
# target_link_libraries(rtc_video PRIVATE FFmpeg::avcodec FFmpeg::avformat FFmpeg::avutil)

target_compile_features(rtc_video PUBLIC cxx_std_20)
