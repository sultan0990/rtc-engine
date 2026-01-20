# RTC Video Library - H.264/VP8 codec, capture, frame processing
cmake_minimum_required(VERSION 3.20)

# Source files
set(RTC_VIDEO_SOURCES
    src/video_codec.cpp
    src/video_capture.cpp
    src/frame_buffer.cpp
    src/bitrate_controller.cpp
    src/video_stream.cpp
)

# Header files
set(RTC_VIDEO_HEADERS
    include/rtc/video/video_codec.h
    include/rtc/video/video_capture.h
    include/rtc/video/frame_buffer.h
    include/rtc/video/bitrate_controller.h
    include/rtc/video/video_stream.h
)

# Create library
add_library(rtc_video STATIC ${RTC_VIDEO_SOURCES} ${RTC_VIDEO_HEADERS})

# Include directories
target_include_directories(rtc_video PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link to core library
target_link_libraries(rtc_video PUBLIC rtc_core)

# Platform-specific
if(UNIX AND NOT APPLE)
    # Linux: link pthread for threading
    target_link_libraries(rtc_video PRIVATE pthread)
endif()

# Optional: Find and link FFmpeg
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(FFMPEG IMPORTED_TARGET libavcodec libavformat libavutil libswscale)
# if(FFMPEG_FOUND)
#     target_link_libraries(rtc_video PRIVATE PkgConfig::FFMPEG)
#     target_compile_definitions(rtc_video PRIVATE HAVE_FFMPEG=1)
# endif()

target_compile_features(rtc_video PUBLIC cxx_std_20)
