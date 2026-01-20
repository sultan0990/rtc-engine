# RTC Core Library - Networking, RTP/RTCP, ICE
cmake_minimum_required(VERSION 3.20)

# Source files
set(RTC_CORE_SOURCES
    src/udp_socket.cpp
    src/rtp_packet.cpp
    src/rtcp_packet.cpp
    src/rtp_pacer.cpp
    src/stun_client.cpp
    src/turn_client.cpp
    src/ice_agent.cpp
    src/lock_free_queue.cpp
)

# Header files
set(RTC_CORE_HEADERS
    include/rtc/udp_socket.h
    include/rtc/rtp_packet.h
    include/rtc/rtcp_packet.h
    include/rtc/rtp_pacer.h
    include/rtc/stun_client.h
    include/rtc/turn_client.h
    include/rtc/ice_agent.h
    include/rtc/lock_free_queue.h
)

# Create library
add_library(rtc_core STATIC ${RTC_CORE_SOURCES} ${RTC_CORE_HEADERS})

# Include directories
target_include_directories(rtc_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(rtc_core PRIVATE ws2_32)
elseif(UNIX)
    target_link_libraries(rtc_core PRIVATE pthread)
endif()

# Enable C++20 features
target_compile_features(rtc_core PUBLIC cxx_std_20)
