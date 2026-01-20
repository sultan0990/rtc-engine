/**
 * @file video_codec.cpp
 * @brief Video codec implementation (stub)
 *
 * Note: Full implementation requires FFmpeg.
 * Install: apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
 */

#include "rtc/video/video_codec.h"

// Uncomment when FFmpeg is available:
// extern "C" {
// #include <libavcodec/avcodec.h>
// #include <libavutil/imgutils.h>
// #include <libswscale/swscale.h>
// }

namespace rtc
{
namespace video
{

// VideoEncoder implementation
struct VideoEncoder::Impl
{
  VideoEncoderConfig config;
  bool initialized = false;
  bool keyframe_requested = false;

  // AVCodecContext* codec_ctx = nullptr;
  // AVFrame* frame = nullptr;
  // AVPacket* packet = nullptr;

  Impl(VideoEncoderConfig cfg) : config(std::move(cfg)) {}

  ~Impl()
  {
    // if (packet) av_packet_free(&packet);
    // if (frame) av_frame_free(&frame);
    // if (codec_ctx) avcodec_free_context(&codec_ctx);
  }
};

VideoEncoder::VideoEncoder(VideoEncoderConfig config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
}

VideoEncoder::~VideoEncoder() = default;

VideoEncoder::VideoEncoder(VideoEncoder&&) noexcept = default;
VideoEncoder& VideoEncoder::operator=(VideoEncoder&&) noexcept = default;

bool VideoEncoder::initialize()
{
  /*
  const AVCodec* codec = nullptr;
  if (impl_->config.codec == VideoCodecType::H264) {
      if (impl_->config.use_hardware) {
          codec = avcodec_find_encoder_by_name("h264_vaapi");
      }
      if (!codec) {
          codec = avcodec_find_encoder(AV_CODEC_ID_H264);
      }
  } else if (impl_->config.codec == VideoCodecType::VP8) {
      codec = avcodec_find_encoder(AV_CODEC_ID_VP8);
  }

  if (!codec) return false;

  impl_->codec_ctx = avcodec_alloc_context3(codec);
  if (!impl_->codec_ctx) return false;

  impl_->codec_ctx->width = impl_->config.width;
  impl_->codec_ctx->height = impl_->config.height;
  impl_->codec_ctx->time_base = {1, impl_->config.fps};
  impl_->codec_ctx->framerate = {impl_->config.fps, 1};
  impl_->codec_ctx->bit_rate = impl_->config.bitrate_kbps * 1000;
  impl_->codec_ctx->gop_size = impl_->config.keyframe_interval;
  impl_->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  impl_->codec_ctx->thread_count = impl_->config.num_threads;

  if (avcodec_open2(impl_->codec_ctx, codec, nullptr) < 0) {
      return false;
  }

  impl_->frame = av_frame_alloc();
  impl_->packet = av_packet_alloc();
  */

  impl_->initialized = true;
  return true;
}

EncodedFrame VideoEncoder::encode(const VideoFrame& frame)
{
  EncodedFrame result;

  if (!impl_->initialized)
  {
    return result;
  }

  /*
  // Fill AVFrame with YUV data
  impl_->frame->data[0] = const_cast<uint8_t*>(frame.data_y.data());
  impl_->frame->data[1] = const_cast<uint8_t*>(frame.data_u.data());
  impl_->frame->data[2] = const_cast<uint8_t*>(frame.data_v.data());
  impl_->frame->linesize[0] = frame.stride_y;
  impl_->frame->linesize[1] = frame.stride_u;
  impl_->frame->linesize[2] = frame.stride_v;
  impl_->frame->width = frame.width;
  impl_->frame->height = frame.height;
  impl_->frame->format = AV_PIX_FMT_YUV420P;
  impl_->frame->pts = frame.timestamp_us;

  if (impl_->keyframe_requested) {
      impl_->frame->pict_type = AV_PICTURE_TYPE_I;
      impl_->keyframe_requested = false;
  }

  int ret = avcodec_send_frame(impl_->codec_ctx, impl_->frame);
  if (ret < 0) return result;

  ret = avcodec_receive_packet(impl_->codec_ctx, impl_->packet);
  if (ret < 0) return result;

  result.data.assign(impl_->packet->data, impl_->packet->data + impl_->packet->size);
  result.is_keyframe = (impl_->packet->flags & AV_PKT_FLAG_KEY) != 0;
  result.timestamp_us = impl_->packet->pts;
  result.width = frame.width;
  result.height = frame.height;
  result.codec = impl_->config.codec;

  av_packet_unref(impl_->packet);
  */

  // Stub: Return fake encoded data
  result.data.resize(1000);  // Fake frame
  result.width = frame.width;
  result.height = frame.height;
  result.timestamp_us = frame.timestamp_us;
  result.is_keyframe = impl_->keyframe_requested || frame.is_keyframe;
  result.codec = impl_->config.codec;
  impl_->keyframe_requested = false;

  return result;
}

void VideoEncoder::request_keyframe()
{
  impl_->keyframe_requested = true;
}

void VideoEncoder::set_bitrate(int bitrate_kbps)
{
  impl_->config.bitrate_kbps = bitrate_kbps;
  // if (impl_->codec_ctx) {
  //     impl_->codec_ctx->bit_rate = bitrate_kbps * 1000;
  // }
}

void VideoEncoder::set_resolution(int width, int height)
{
  impl_->config.width = width;
  impl_->config.height = height;
  // Would need to reinitialize encoder
}

const VideoEncoderConfig& VideoEncoder::config() const
{
  return impl_->config;
}

bool VideoEncoder::is_initialized() const
{
  return impl_->initialized;
}

// VideoDecoder implementation
struct VideoDecoder::Impl
{
  VideoDecoderConfig config;
  bool initialized = false;

  // AVCodecContext* codec_ctx = nullptr;
  // AVFrame* frame = nullptr;
  // AVPacket* packet = nullptr;

  Impl(VideoDecoderConfig cfg) : config(std::move(cfg)) {}

  ~Impl()
  {
    // Cleanup
  }
};

VideoDecoder::VideoDecoder(VideoDecoderConfig config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
}

VideoDecoder::~VideoDecoder() = default;

VideoDecoder::VideoDecoder(VideoDecoder&&) noexcept = default;
VideoDecoder& VideoDecoder::operator=(VideoDecoder&&) noexcept = default;

bool VideoDecoder::initialize()
{
  impl_->initialized = true;
  return true;
}

std::optional<VideoFrame> VideoDecoder::decode(const EncodedFrame& encoded)
{
  if (!impl_->initialized || encoded.data.empty())
  {
    return std::nullopt;
  }

  // Stub: Return black frame
  VideoFrame frame;
  frame.width = encoded.width > 0 ? encoded.width : 1280;
  frame.height = encoded.height > 0 ? encoded.height : 720;
  frame.stride_y = frame.width;
  frame.stride_u = frame.width / 2;
  frame.stride_v = frame.width / 2;
  frame.data_y.resize(frame.width * frame.height, 16);       // Y = 16 (black)
  frame.data_u.resize(frame.width * frame.height / 4, 128);  // U = 128
  frame.data_v.resize(frame.width * frame.height / 4, 128);  // V = 128
  frame.timestamp_us = encoded.timestamp_us;
  frame.is_keyframe = encoded.is_keyframe;

  return frame;
}

void VideoDecoder::reset()
{
  // avcodec_flush_buffers(impl_->codec_ctx);
}

bool VideoDecoder::is_initialized() const
{
  return impl_->initialized;
}

}  // namespace video
}  // namespace rtc
