/**
 * @file video_capture.cpp
 * @brief Video capture using V4L2 (Linux stub)
 */

#include "rtc/video/video_capture.h"

#include "rtc/video/video_codec.h"


// Uncomment for Linux V4L2:
// #include <fcntl.h>
// #include <linux/videodev2.h>
// #include <sys/ioctl.h>
// #include <sys/mman.h>
// #include <unistd.h>

#include <thread>

namespace rtc
{
namespace video
{

struct VideoCapture::Impl
{
  VideoCaptureConfig config;
  VideoCaptureCallback callback;
  bool capturing = false;
  std::thread capture_thread;

  // int fd = -1;  // V4L2 file descriptor
  // Buffer structures for mmap

  Impl() = default;
};

VideoCapture::VideoCapture() : impl_(std::make_unique<Impl>()) {}

VideoCapture::~VideoCapture()
{
  close();
}

std::vector<VideoDevice> VideoCapture::get_devices()
{
  std::vector<VideoDevice> devices;

  /*
  // Scan /dev/video* devices
  for (int i = 0; i < 10; ++i) {
      std::string path = "/dev/video" + std::to_string(i);
      int fd = open(path.c_str(), O_RDWR);
      if (fd < 0) continue;

      v4l2_capability cap;
      if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
          if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
              VideoDevice dev;
              dev.path = path;
              dev.name = reinterpret_cast<char*>(cap.card);
              dev.is_capture_device = true;
              devices.push_back(dev);
          }
      }
      ::close(fd);
  }
  */

  // Stub: Return fake device
  VideoDevice fake;
  fake.path = "/dev/video0";
  fake.name = "Fake Camera (Stub)";
  fake.supported_resolutions = {{1920, 1080}, {1280, 720}, {640, 480}};
  fake.supported_fps = {30, 60};
  fake.is_capture_device = true;
  devices.push_back(fake);

  return devices;
}

std::optional<VideoDevice> VideoCapture::get_default_device()
{
  auto devices = get_devices();
  for (const auto& dev : devices)
  {
    if (dev.is_capture_device)
    {
      return dev;
    }
  }
  return std::nullopt;
}

bool VideoCapture::open(VideoCaptureConfig config)
{
  impl_->config = config;

  /*
  impl_->fd = ::open(config.device_path.c_str(), O_RDWR);
  if (impl_->fd < 0) return false;

  // Set format
  v4l2_format fmt = {};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = config.width;
  fmt.fmt.pix.height = config.height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  fmt.fmt.pix.field = V4L2_FIELD_NONE;

  if (ioctl(impl_->fd, VIDIOC_S_FMT, &fmt) < 0) {
      ::close(impl_->fd);
      return false;
  }

  // Request buffers and mmap...
  */

  return true;
}

bool VideoCapture::start(VideoCaptureCallback callback)
{
  impl_->callback = std::move(callback);
  impl_->capturing = true;

  // Start capture thread
  impl_->capture_thread = std::thread(
      [this]()
      {
        while (impl_->capturing)
        {
          // Stub: Generate fake frame every 33ms (30fps)
          std::this_thread::sleep_for(std::chrono::milliseconds(33));

          if (impl_->callback && impl_->capturing)
          {
            VideoFrame frame;
            frame.width = impl_->config.width;
            frame.height = impl_->config.height;
            frame.stride_y = frame.width;
            frame.stride_u = frame.width / 2;
            frame.stride_v = frame.width / 2;
            frame.data_y.resize(frame.width * frame.height, 128);
            frame.data_u.resize(frame.width * frame.height / 4, 128);
            frame.data_v.resize(frame.width * frame.height / 4, 128);
            frame.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count();

            impl_->callback(frame);
          }
        }
      });

  return true;
}

void VideoCapture::stop()
{
  impl_->capturing = false;
  if (impl_->capture_thread.joinable())
  {
    impl_->capture_thread.join();
  }
}

void VideoCapture::close()
{
  stop();
  // if (impl_->fd >= 0) ::close(impl_->fd);
  // impl_->fd = -1;
}

bool VideoCapture::is_capturing() const
{
  return impl_->capturing;
}

int VideoCapture::width() const
{
  return impl_->config.width;
}

int VideoCapture::height() const
{
  return impl_->config.height;
}

int VideoCapture::fps() const
{
  return impl_->config.fps;
}

}  // namespace video
}  // namespace rtc
