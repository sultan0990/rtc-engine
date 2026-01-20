/**
 * @file audio_capture.cpp
 * @brief Audio capture/playback stub implementation
 *
 * Note: Full implementation requires PortAudio.
 * Install via:
 *   - Windows: vcpkg install portaudio
 *   - Linux: apt install libportaudio-dev
 *   - macOS: brew install portaudio
 */

#include "rtc/audio/audio_capture.h"

// Uncomment when PortAudio is available:
// #include <portaudio.h>

namespace rtc
{
namespace audio
{

// AudioCapture implementation
struct AudioCapture::Impl
{
  AudioCaptureConfig config;
  AudioCaptureCallback callback;
  bool capturing = false;
  int frame_size = 0;

  // PaStream* stream = nullptr;  // PortAudio stream

  Impl() = default;
};

AudioCapture::AudioCapture() : impl_(std::make_unique<Impl>()) {}

AudioCapture::~AudioCapture()
{
  close();
}

bool AudioCapture::initialize_audio_system()
{
  // return Pa_Initialize() == paNoError;
  return true;  // Stub
}

void AudioCapture::terminate_audio_system()
{
  // Pa_Terminate();
}

std::vector<AudioDevice> AudioCapture::get_devices()
{
  std::vector<AudioDevice> devices;

  /*
  int num_devices = Pa_GetDeviceCount();
  for (int i = 0; i < num_devices; ++i) {
      const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
      if (info) {
          AudioDevice dev;
          dev.id = i;
          dev.name = info->name;
          dev.max_input_channels = info->maxInputChannels;
          dev.max_output_channels = info->maxOutputChannels;
          dev.default_sample_rate = info->defaultSampleRate;
          dev.is_default_input = (i == Pa_GetDefaultInputDevice());
          dev.is_default_output = (i == Pa_GetDefaultOutputDevice());
          devices.push_back(dev);
      }
  }
  */

  // Stub: Return fake default device
  AudioDevice fake_device;
  fake_device.id = 0;
  fake_device.name = "Default Audio Device (Stub)";
  fake_device.max_input_channels = 2;
  fake_device.max_output_channels = 2;
  fake_device.default_sample_rate = 48000;
  fake_device.is_default_input = true;
  fake_device.is_default_output = true;
  devices.push_back(fake_device);

  return devices;
}

std::optional<AudioDevice> AudioCapture::get_default_input_device()
{
  auto devices = get_devices();
  for (const auto& dev : devices)
  {
    if (dev.is_default_input)
    {
      return dev;
    }
  }
  return std::nullopt;
}

bool AudioCapture::open(AudioCaptureConfig config)
{
  impl_->config = config;
  impl_->frame_size = config.sample_rate * config.frame_duration_ms / 1000;

  /*
  PaStreamParameters params;
  params.device = config.device_id < 0 ? Pa_GetDefaultInputDevice() : config.device_id;
  params.channelCount = config.channels;
  params.sampleFormat = paInt16;
  params.suggestedLatency = Pa_GetDeviceInfo(params.device)->defaultLowInputLatency;
  params.hostApiSpecificStreamInfo = nullptr;

  PaError err = Pa_OpenStream(
      &impl_->stream,
      &params,
      nullptr,  // No output
      config.sample_rate,
      impl_->frame_size,
      paClipOff,
      nullptr,  // Blocking read
      nullptr);

  return err == paNoError;
  */

  return true;  // Stub
}

bool AudioCapture::start(AudioCaptureCallback callback)
{
  impl_->callback = std::move(callback);
  impl_->capturing = true;

  // Pa_StartStream(impl_->stream);
  return true;
}

void AudioCapture::stop()
{
  impl_->capturing = false;
  // Pa_StopStream(impl_->stream);
}

void AudioCapture::close()
{
  stop();
  // if (impl_->stream) Pa_CloseStream(impl_->stream);
  // impl_->stream = nullptr;
}

bool AudioCapture::is_capturing() const
{
  return impl_->capturing;
}

int AudioCapture::frame_size() const
{
  return impl_->frame_size;
}

// AudioPlayback implementation
struct AudioPlayback::Impl
{
  int sample_rate = 48000;
  int channels = 1;
  bool playing = false;

  // PaStream* stream = nullptr;

  Impl() = default;
};

AudioPlayback::AudioPlayback() : impl_(std::make_unique<Impl>()) {}

AudioPlayback::~AudioPlayback()
{
  close();
}

bool AudioPlayback::open(int /*device_id*/, int sample_rate, int channels)
{
  impl_->sample_rate = sample_rate;
  impl_->channels = channels;

  /*
  PaStreamParameters params;
  params.device = device_id < 0 ? Pa_GetDefaultOutputDevice() : device_id;
  params.channelCount = channels;
  params.sampleFormat = paInt16;
  params.suggestedLatency = Pa_GetDeviceInfo(params.device)->defaultLowOutputLatency;
  params.hostApiSpecificStreamInfo = nullptr;

  int frame_size = sample_rate * 20 / 1000;  // 20ms frames

  PaError err = Pa_OpenStream(
      &impl_->stream,
      nullptr,  // No input
      &params,
      sample_rate,
      frame_size,
      paClipOff,
      nullptr,
      nullptr);

  return err == paNoError;
  */

  return true;  // Stub
}

bool AudioPlayback::start()
{
  impl_->playing = true;
  // Pa_StartStream(impl_->stream);
  return true;
}

size_t AudioPlayback::write(std::span<const int16_t> /*samples*/)
{
  // Pa_WriteStream(impl_->stream, samples.data(), samples.size() / impl_->channels);
  return 0;  // Stub
}

void AudioPlayback::stop()
{
  impl_->playing = false;
  // Pa_StopStream(impl_->stream);
}

void AudioPlayback::close()
{
  stop();
  // if (impl_->stream) Pa_CloseStream(impl_->stream);
  // impl_->stream = nullptr;
}

bool AudioPlayback::is_playing() const
{
  return impl_->playing;
}

size_t AudioPlayback::available_buffer_space() const
{
  return 4096;  // Stub
}

}  // namespace audio
}  // namespace rtc
