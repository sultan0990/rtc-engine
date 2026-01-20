# Real‑Time Voice & Video Conferencing Engine

## Overview
A high‑performance, headless RTC engine written in modern C++ (C++20) that provides low‑latency audio/video streaming, NAT traversal, and optional SFU/MCU server capabilities.

## Build Instructions
```bash
# Clone the repository
git clone <repo-url>
cd rtc-engine

# Build (Linux/macOS)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Directory Layout
- `core/` – networking, ICE, RTP/RTCP handling
- `audio/` – Opus integration, echo cancellation, jitter buffer
- `video/` – H.264/VP8 encoding, adaptive bitrate, frame reordering
- `server/` – SFU forwarder, metrics, scaling utilities

## License
MIT License (see LICENSE file).
