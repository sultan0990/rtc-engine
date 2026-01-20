/**
 * @file audio_stream.cpp
 * @brief Audio stream implementation
 */

#include "rtc/audio/audio_stream.h"

#include <atomic>
#include <mutex>
#include <thread>

#include "rtc/audio/audio_capture.h"
#include "rtc/audio/audio_processing.h"
#include "rtc/audio/jitter_buffer.h"
#include "rtc/audio/opus_codec.h"


namespace rtc
{
namespace audio
{

class AudioStreamImpl : public AudioStream
{
 public:
  explicit AudioStreamImpl(AudioStreamConfig config)
      : config_(std::move(config)),
        encoder_({.sample_rate = config_.sample_rate,
                  .channels = config_.channels,
                  .bitrate = config_.bitrate,
                  .frame_duration_ms = config_.frame_duration_ms}),
        decoder_({.sample_rate = config_.sample_rate, .channels = config_.channels}),
        jitter_buffer_({.sample_rate = config_.sample_rate}),
        processor_({
            .enable_aec = config_.enable_aec,
            .enable_ns = config_.enable_ns,
            .enable_agc = config_.enable_agc,
        })
  {
  }

  ~AudioStreamImpl() override
  {
    stop();
  }

  bool start() override
  {
    if (running_.load())
    {
      return false;
    }

    if (!encoder_.initialize() || !decoder_.initialize() || !processor_.initialize())
    {
      return false;
    }

    if (!capture_.open({.sample_rate = config_.sample_rate,
                        .channels = config_.channels,
                        .frame_duration_ms = config_.frame_duration_ms}))
    {
      return false;
    }

    running_.store(true);
    sequence_ = 0;
    timestamp_ = 0;

    // Start capture with callback
    capture_.start([this](std::span<const int16_t> samples, int64_t /*ts*/)
                   { on_capture_frame(samples); });

    // Start playout thread
    playout_thread_ = std::thread([this]() { playout_loop(); });

    return true;
  }

  void stop() override
  {
    running_.store(false);
    capture_.stop();

    if (playout_thread_.joinable())
    {
      playout_thread_.join();
    }
  }

  void set_send_callback(AudioSendCallback callback) override
  {
    std::lock_guard lock(mutex_);
    send_callback_ = std::move(callback);
  }

  void set_playback_callback(AudioPlaybackCallback callback) override
  {
    std::lock_guard lock(mutex_);
    playback_callback_ = std::move(callback);
  }

  void receive_packet(std::span<const uint8_t> opus_data, uint32_t timestamp,
                      uint16_t sequence) override
  {
    JitterFrame frame;
    frame.data.assign(opus_data.begin(), opus_data.end());
    frame.timestamp = timestamp;
    frame.sequence_number = sequence;
    frame.arrival_time = std::chrono::steady_clock::now();

    jitter_buffer_.push(std::move(frame));
    stats_.packets_received++;
    stats_.bytes_received += opus_data.size();
  }

  AudioStreamStats stats() const override
  {
    std::lock_guard lock(mutex_);
    auto jb_stats = jitter_buffer_.stats();
    AudioStreamStats s = stats_;
    s.packet_loss_rate = jb_stats.packet_loss_rate;
    s.jitter_ms = jb_stats.jitter_ms;
    return s;
  }

  void set_muted(bool muted) override
  {
    muted_.store(muted);
  }

  bool is_muted() const override
  {
    return muted_.load();
  }

  void set_volume(float volume) override
  {
    volume_.store(volume);
  }

  float audio_level() const override
  {
    return audio_level_.load();
  }

 private:
  void on_capture_frame(std::span<const int16_t> samples)
  {
    if (muted_.load())
    {
      return;
    }

    // Copy samples for processing
    std::vector<int16_t> processed(samples.begin(), samples.end());

    // Apply audio processing
    processor_.process_capture_frame(processed);

    // Calculate audio level
    float level = calculate_audio_level(processed);
    audio_level_.store(level);

    // Encode
    auto result = encoder_.encode(processed);
    if (result.success())
    {
      std::lock_guard lock(mutex_);
      if (send_callback_)
      {
        send_callback_(result.data, timestamp_, sequence_);
      }

      stats_.packets_sent++;
      stats_.bytes_sent += result.data.size();
    }

    timestamp_ += result.samples_encoded;
    sequence_++;
  }

  void playout_loop()
  {
    int frame_size = config_.sample_rate * config_.frame_duration_ms / 1000;

    while (running_.load())
    {
      auto frame = jitter_buffer_.pop();
      if (frame)
      {
        auto result = decoder_.decode(frame->data, frame_size);
        if (result.success())
        {
          // Feed to AEC
          processor_.process_render_frame(result.samples);

          std::lock_guard lock(mutex_);
          if (playback_callback_)
          {
            playback_callback_(result.samples);
          }
        }
      }
      else
      {
        // No packet ready, maybe do PLC or just wait
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.frame_duration_ms / 2));
      }
    }
  }

  float calculate_audio_level(std::span<const int16_t> samples)
  {
    if (samples.empty())
    {
      return -96.0f;
    }

    int64_t sum_squares = 0;
    for (int16_t s : samples)
    {
      sum_squares += static_cast<int64_t>(s) * s;
    }

    double rms = std::sqrt(static_cast<double>(sum_squares) / samples.size());
    if (rms < 1.0)
    {
      return -96.0f;
    }

    return 20.0f * std::log10(rms / 32768.0);
  }

  AudioStreamConfig config_;
  OpusEncoder encoder_;
  OpusDecoder decoder_;
  JitterBuffer jitter_buffer_;
  AudioProcessor processor_;
  AudioCapture capture_;

  std::atomic<bool> running_{false};
  std::atomic<bool> muted_{false};
  std::atomic<float> volume_{1.0f};
  std::atomic<float> audio_level_{-96.0f};

  uint32_t timestamp_ = 0;
  uint16_t sequence_ = 0;

  mutable std::mutex mutex_;
  AudioSendCallback send_callback_;
  AudioPlaybackCallback playback_callback_;
  AudioStreamStats stats_;

  std::thread playout_thread_;
};

std::unique_ptr<AudioStream> create_audio_stream(AudioStreamConfig config)
{
  return std::make_unique<AudioStreamImpl>(std::move(config));
}

}  // namespace audio
}  // namespace rtc
