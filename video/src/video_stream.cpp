/**
 * @file video_stream.cpp
 * @brief Video stream implementation
 */

#include "rtc/video/video_stream.h"

#include <atomic>
#include <mutex>
#include <thread>

#include "rtc/video/bitrate_controller.h"
#include "rtc/video/frame_buffer.h"
#include "rtc/video/video_capture.h"
#include "rtc/video/video_codec.h"


namespace rtc
{
namespace video
{

class VideoStreamImpl : public VideoStream
{
 public:
  explicit VideoStreamImpl(VideoStreamConfig config)
      : config_(config),
        encoder_({
            .codec = config.codec,
            .width = config.width,
            .height = config.height,
            .fps = config.fps,
            .bitrate_kbps = config.bitrate_kbps,
            .use_hardware = config.use_hardware,
        }),
        decoder_({.codec = config.codec, .use_hardware = config.use_hardware}),
        frame_buffer_({}),
        bitrate_controller_({
            .start_bitrate_bps = static_cast<uint64_t>(config.bitrate_kbps) * 1000,
        })
  {
  }

  ~VideoStreamImpl() override
  {
    stop();
  }

  bool start() override
  {
    if (running_.load()) return false;

    if (!encoder_.initialize() || !decoder_.initialize())
    {
      return false;
    }

    if (!capture_.open({
            .width = config_.width,
            .height = config_.height,
            .fps = config_.fps,
        }))
    {
      return false;
    }

    running_.store(true);
    sequence_ = 0;
    timestamp_ = 0;

    // Setup bitrate callback
    bitrate_controller_.set_callback([this](uint64_t bps)
                                     { encoder_.set_bitrate(static_cast<int>(bps / 1000)); });

    // Start capture
    capture_.start([this](const VideoFrame& frame) { on_capture_frame(frame); });

    // Start decode thread
    decode_thread_ = std::thread([this]() { decode_loop(); });

    return true;
  }

  void stop() override
  {
    running_.store(false);
    capture_.stop();

    if (decode_thread_.joinable())
    {
      decode_thread_.join();
    }
  }

  void set_send_callback(VideoSendCallback callback) override
  {
    std::lock_guard lock(mutex_);
    send_callback_ = std::move(callback);
  }

  void set_render_callback(VideoRenderCallback callback) override
  {
    std::lock_guard lock(mutex_);
    render_callback_ = std::move(callback);
  }

  void set_keyframe_request_callback(KeyframeRequestCallback callback) override
  {
    std::lock_guard lock(mutex_);
    keyframe_request_callback_ = std::move(callback);
  }

  void receive_packet(std::span<const uint8_t> data, uint32_t timestamp, uint16_t sequence,
                      bool marker) override
  {
    // Detect keyframe (simplified - check NAL type for H.264)
    bool is_keyframe = false;
    if (!data.empty())
    {
      // H.264: NAL type 5 = IDR frame
      uint8_t nal_type = data[0] & 0x1F;
      is_keyframe = (nal_type == 5 || nal_type == 7 || nal_type == 8);
    }

    frame_buffer_.insert_packet(data, sequence, timestamp, marker, is_keyframe);
    stats_.frames_received++;
    stats_.bytes_received += data.size();
  }

  void request_keyframe() override
  {
    encoder_.request_keyframe();
  }

  void set_target_bitrate(int bitrate_kbps) override
  {
    bitrate_controller_.on_remb(static_cast<uint64_t>(bitrate_kbps) * 1000);
  }

  VideoStreamStats stats() const override
  {
    std::lock_guard lock(mutex_);
    VideoStreamStats s = stats_;
    auto fb_stats = frame_buffer_.stats();
    s.packet_loss_rate = fb_stats.packet_loss_rate;
    s.current_width = config_.width;
    s.current_height = config_.height;
    s.current_fps = config_.fps;
    s.current_bitrate_kbps = static_cast<float>(bitrate_controller_.target_bitrate()) / 1000.0f;
    return s;
  }

  void set_enabled(bool enabled) override
  {
    enabled_.store(enabled);
  }

  bool is_enabled() const override
  {
    return enabled_.load();
  }

 private:
  void on_capture_frame(const VideoFrame& frame)
  {
    if (!enabled_.load()) return;

    auto result = encoder_.encode(frame);
    if (result.success())
    {
      std::lock_guard lock(mutex_);
      if (send_callback_)
      {
        send_callback_(result.data, timestamp_, sequence_, result.is_keyframe);
      }

      stats_.frames_sent++;
      stats_.bytes_sent += result.data.size();
      bitrate_controller_.on_packet_sent(result.data.size());
    }

    // Increment timestamp (90kHz for video)
    timestamp_ += 90000 / config_.fps;
    sequence_++;

    // Process bitrate controller
    bitrate_controller_.process();
  }

  void decode_loop()
  {
    while (running_.load())
    {
      // Check for keyframe request
      if (frame_buffer_.should_request_keyframe())
      {
        std::lock_guard lock(mutex_);
        if (keyframe_request_callback_)
        {
          keyframe_request_callback_();
        }
      }

      auto buffered = frame_buffer_.pop_frame();
      if (buffered)
      {
        EncodedFrame encoded;
        encoded.data = std::move(buffered->data);
        encoded.is_keyframe = buffered->is_keyframe;
        encoded.codec = config_.codec;
        encoded.width = config_.width;
        encoded.height = config_.height;

        auto decoded = decoder_.decode(encoded);
        if (decoded)
        {
          std::lock_guard lock(mutex_);
          if (render_callback_)
          {
            render_callback_(*decoded);
          }
        }
      }
      else
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    }
  }

  VideoStreamConfig config_;
  VideoEncoder encoder_;
  VideoDecoder decoder_;
  FrameBuffer frame_buffer_;
  BitrateController bitrate_controller_;
  VideoCapture capture_;

  std::atomic<bool> running_{false};
  std::atomic<bool> enabled_{true};

  uint32_t timestamp_ = 0;
  uint16_t sequence_ = 0;

  mutable std::mutex mutex_;
  VideoSendCallback send_callback_;
  VideoRenderCallback render_callback_;
  KeyframeRequestCallback keyframe_request_callback_;
  VideoStreamStats stats_;

  std::thread decode_thread_;
};

std::unique_ptr<VideoStream> create_video_stream(VideoStreamConfig config)
{
  return std::make_unique<VideoStreamImpl>(std::move(config));
}

}  // namespace video
}  // namespace rtc
