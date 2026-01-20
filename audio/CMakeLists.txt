# RTC Audio Library - Opus codec, AEC, AGC, Jitter buffer
cmake_minimum_required(VERSION 3.20)

# Source files
set(RTC_AUDIO_SOURCES
    src/opus_codec.cpp
    src/audio_capture.cpp
    src/audio_processing.cpp
    src/jitter_buffer.cpp
    src/audio_stream.cpp
)

# Header files
set(RTC_AUDIO_HEADERS
    include/rtc/audio/opus_codec.h
    include/rtc/audio/audio_capture.h
    include/rtc/audio/audio_processing.h
    include/rtc/audio/jitter_buffer.h
    include/rtc/audio/audio_stream.h
)

# Create library
add_library(rtc_audio STATIC ${RTC_AUDIO_SOURCES} ${RTC_AUDIO_HEADERS})

# Include directories
target_include_directories(rtc_audio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link to core library
target_link_libraries(rtc_audio PUBLIC rtc_core)

# Optional: Find and link Opus
# find_package(Opus)
# if(Opus_FOUND)
#     target_link_libraries(rtc_audio PRIVATE Opus::opus)
#     target_compile_definitions(rtc_audio PRIVATE HAVE_OPUS=1)
# endif()

# Optional: Find and link PortAudio
# find_package(portaudio)
# if(portaudio_FOUND)
#     target_link_libraries(rtc_audio PRIVATE portaudio)
#     target_compile_definitions(rtc_audio PRIVATE HAVE_PORTAUDIO=1)
# endif()

target_compile_features(rtc_audio PUBLIC cxx_std_20)
