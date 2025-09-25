#include "include/camera/pipewire_callbacks.h"
#include "include/camera/pipewire_output.h"
#include "include/camera/pipewire_frame_converters.h"

#include <iostream>
#include <cstring>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/type-info.h>
#include <spa/param/video/raw.h>
#include <spa/pod/builder.h>
#include <spa/param/param.h>
#include <opencv2/opencv.hpp>

// Forward declarations for helper functions
static void handle_format_parameter(segmecam::PipeWireOutput* self, const struct spa_pod* param);
static void parse_video_format(segmecam::PipeWireOutput* self, const struct spa_pod* param);
static void dump_param_structure(const struct spa_pod* param);
static void dump_spa_pod(const struct spa_pod* param);

// Helper functions for on_stream_process refactoring
static bool validate_process_callback_data(void* data, segmecam::PipeWireOutput*& self);
static pw_buffer* dequeue_and_validate_buffer(segmecam::PipeWireOutput* self);
static cv::Mat get_frame_to_send(segmecam::PipeWireOutput* self, int process_count);
static bool validate_frame_and_format(segmecam::PipeWireOutput* self, const cv::Mat& frame, pw_buffer* buffer, int process_count);
static bool convert_frame_to_buffer(segmecam::PipeWireOutput* self, const cv::Mat& frame, pw_buffer* buffer, int process_count);
static void set_buffer_metadata(pw_buffer* buffer, size_t copy_size, int stride);

// Helper functions for validate_frame_and_format refactoring
static bool check_format_negotiated(segmecam::PipeWireOutput* self, pw_buffer* buffer, int process_count);
static bool check_buffers_ready(segmecam::PipeWireOutput* self, pw_buffer* buffer, int process_count);
static bool validate_frame_dimensions(segmecam::PipeWireOutput* self, const cv::Mat& frame, pw_buffer* buffer);
static size_t calculate_expected_size(const cv::Mat& frame, spa_video_info_raw format);
static bool validate_buffer_size(segmecam::PipeWireOutput* self, pw_buffer* buffer, size_t expected_size);

static void log_buffer_info(pw_buffer* buffer, int& process_count) {
  if (process_count < 3) {
    struct spa_buffer* spa_buf = buffer->buffer;
    std::cout << "PipeWire buffer info: n_datas=" << spa_buf->n_datas
              << ", datas[0].maxsize=" << spa_buf->datas[0].maxsize << std::endl;
    std::cout << "PipeWire process callback #" << process_count << " - buffer valid" << std::endl;
    process_count++;
  }
}

// Helper function to handle format parameter parsing
static void handle_format_parameter(segmecam::PipeWireOutput* self, const struct spa_pod* param) {
  if (!param) return;

  // Parse the negotiated format
  uint32_t media_type, media_subtype;
  std::cout << "[PipeWire param callback] spa_format_parse param=" << static_cast<const void*>(param) << std::endl;
  std::cout << "[PipeWire param callback] param->type=" << param->type << ", param->size=" << param->size << std::endl;
  int parse_result = spa_format_parse(param, &media_type, &media_subtype);
  std::cout << "[PipeWire param callback] spa_format_parse result=" << parse_result << std::endl;

  if (parse_result < 0) {
    std::cerr << "[PipeWire param callback] ERROR: spa_format_parse failed (result=" << parse_result << "), param type=" << param->type << std::endl;
    dump_param_structure(param);
    return;
  }

  std::cout << "PipeWire negotiated format - media_type: " << media_type << ", media_subtype: " << media_subtype << std::endl;

  // For video formats, parse additional parameters
  if (media_type == SPA_MEDIA_TYPE_video) {
    parse_video_format(self, param);
  }
}

// Helper function to parse video format details
static void parse_video_format(segmecam::PipeWireOutput* self, const struct spa_pod* param) {
  spa_video_info_raw video_info = {};
  int raw_parse_result = spa_format_video_raw_parse(param, &video_info);

  if (raw_parse_result == 0) {
    std::cout << "PipeWire negotiated video format:" << std::endl;
    std::cout << "  Format: " << video_info.format << std::endl;
    std::cout << "  Size: " << video_info.size.width << "x" << video_info.size.height << std::endl;
    std::cout << "  Framerate: " << video_info.framerate.num << "/" << video_info.framerate.denom << " fps" << std::endl;

    // Store the negotiated format for use in process callback
    self->SetNegotiatedFormat(video_info);
    std::cout << "PipeWire format stored for frame processing" << std::endl;

    // For output streams with PW_STREAM_FLAG_MAP_BUFFERS, buffers should be ready after format negotiation
    self->SetBuffersReady(true);
    std::cout << "PipeWire buffers marked as ready (output stream with MAP_BUFFERS)" << std::endl;
  } else {
    std::cerr << "[PipeWire param callback] ERROR: spa_format_video_raw_parse failed (result=" << raw_parse_result << ")" << std::endl;
    dump_spa_pod(param);

    // Fallback: assume YUY2 640x480 30fps since that's what GStreamer negotiated
    std::cerr << "[PipeWire param callback] Using fallback format: YUY2 640x480 30fps" << std::endl;
    spa_video_info_raw fallback = {};
    fallback.format = SPA_VIDEO_FORMAT_YUY2;
    fallback.size.width = 640;
    fallback.size.height = 480;
    fallback.framerate.num = 30;
    fallback.framerate.denom = 1;
    self->SetNegotiatedFormat(fallback);
    std::cout << "PipeWire fallback format stored for frame processing" << std::endl;
  }
}

// Helper function to dump param structure for debugging
static void dump_param_structure(const struct spa_pod* param) {
  std::cerr << "[PipeWire param callback] Dumping param structure:" << std::endl;
  std::cerr << "  type: " << param->type << std::endl;
  std::cerr << "  size: " << param->size << std::endl;
  if (param->size > 0 && param->size <= 256) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(param);
    std::cerr << "  raw bytes: ";
    for (size_t i = 0; i < param->size && i < 32; ++i) {
      std::cerr << std::hex << (int)data[i] << " ";
    }
    std::cerr << std::dec << std::endl;
  }
}

// Helper function to dump spa_pod for debugging
static void dump_spa_pod(const struct spa_pod* param) {
  std::cerr << "[PipeWire param callback] Dumping spa_pod (first 64 bytes): ";
  const uint8_t* pod_bytes = reinterpret_cast<const uint8_t*>(param);
  for (int i = 0; i < 64 && i < (int)param->size; ++i) {
    std::cerr << std::hex << (int)pod_bytes[i] << " ";
  }
  std::cerr << std::dec << std::endl;
}

// Helper function to validate process callback data
static bool validate_process_callback_data(void* data, segmecam::PipeWireOutput*& self) {
  segmecam::PipeWireEvents* events = static_cast<segmecam::PipeWireEvents*>(data);
  if (!events) {
    std::cerr << "❌ PipeWire process callback: events is null" << std::endl;
    return false;
  }

  if (!events->output) {
    std::cerr << "❌ PipeWire process callback: events->output is null" << std::endl;
    return false;
  }

  self = events->output;

  if (!self->IsActive()) {
    std::cerr << "PipeWire process callback: output not active" << std::endl;
    return false;
  }

  if (!self->GetStream()) {
    std::cerr << "PipeWire process callback: stream is null" << std::endl;
    return false;
  }

  return true;
}

// Helper function to dequeue and validate buffer
static pw_buffer* dequeue_and_validate_buffer(segmecam::PipeWireOutput* self) {
  pw_buffer* buffer = pw_stream_dequeue_buffer(self->GetStream());
  if (!buffer) {
    std::cerr << "PipeWire process callback: failed to dequeue buffer" << std::endl;
    return nullptr;
  }

  // Check buffer validity with additional safety checks
  if (!buffer->buffer || !buffer->buffer->datas || buffer->buffer->n_datas == 0) {
    std::cerr << "PipeWire process callback: invalid buffer structure" << std::endl;
    pw_stream_queue_buffer(self->GetStream(), buffer);
    return nullptr;
  }

  // Additional buffer validation
  struct spa_buffer* spa_buf = buffer->buffer;
  if (!spa_buf->datas[0].data || spa_buf->datas[0].maxsize == 0) {
    std::cerr << "PipeWire process callback: buffer data is invalid - not queuing back" << std::endl;
    // Don't queue back invalid buffers to prevent sending empty buffers to consumers
    return nullptr;
  }

  return buffer;
}

// Helper function to get frame to send
static cv::Mat get_frame_to_send(segmecam::PipeWireOutput* self, int process_count) {
  cv::Mat frame_to_send;
  {
    std::unique_lock<std::mutex> lock(self->GetFrameMutex());
    if (self->GetFrameReady()) {
      frame_to_send = self->GetCurrentFrame().clone();
      self->GetFrameReady() = false;
      self->GetLastFrame() = frame_to_send.clone();  // Store as last frame

      if (process_count <= 3) {
        std::cout << "PipeWire process callback: sending NEW frame "
                  << frame_to_send.cols << "x" << frame_to_send.rows << "x"
                  << frame_to_send.channels() << std::endl;
      }
    } else if (!self->GetLastFrame().empty()) {
      // No new frame, but we have a previous frame - reuse it
      frame_to_send = self->GetLastFrame().clone();
      if (process_count <= 3) {
        std::cout << "PipeWire process callback: reusing LAST frame "
                  << frame_to_send.cols << "x" << frame_to_send.rows << "x"
                  << frame_to_send.channels() << std::endl;
      }
    }
  }
  return frame_to_send;
}

// Helper function implementations
static bool check_format_negotiated(segmecam::PipeWireOutput* self, pw_buffer* buffer, int process_count) {
  if (!self->IsFormatNegotiated()) {
    if (process_count <= 3) {
      std::cout << "PipeWire process callback: format not negotiated yet, skipping frame" << std::endl;
    }
    pw_stream_queue_buffer(self->GetStream(), buffer);
    return false;
  }
  return true;
}

static bool check_buffers_ready(segmecam::PipeWireOutput* self, pw_buffer* buffer, int process_count) {
  if (!self->AreBuffersReady()) {
    if (process_count <= 3) {
      std::cout << "PipeWire process callback: buffers not ready yet (AreBuffersReady()=" << self->AreBuffersReady() << "), skipping frame" << std::endl;
    }
    pw_stream_queue_buffer(self->GetStream(), buffer);
    return false;
  }
  return true;
}

static bool validate_frame_dimensions(segmecam::PipeWireOutput* self, const cv::Mat& frame, pw_buffer* buffer) {
  spa_video_info_raw format = self->GetNegotiatedFormat();
  if (frame.cols != (int)format.size.width || frame.rows != (int)format.size.height) {
    std::cerr << "PipeWire process callback: frame dimensions mismatch - frame: "
              << frame.cols << "x" << frame.rows
              << ", negotiated: " << format.size.width << "x" << format.size.height << std::endl;
    pw_stream_queue_buffer(self->GetStream(), buffer);
    return false;
  }
  return true;
}

static size_t calculate_expected_size(const cv::Mat& frame, spa_video_info_raw format) {
  switch (format.format) {
    case SPA_VIDEO_FORMAT_YUY2:
    case SPA_VIDEO_FORMAT_UYVY:
      return frame.cols * frame.rows * 2;
    case SPA_VIDEO_FORMAT_BGR:
      return frame.total() * frame.elemSize();
    case SPA_VIDEO_FORMAT_RGB:
    case SPA_VIDEO_FORMAT_BGRx:
    case SPA_VIDEO_FORMAT_RGBx:
      return frame.cols * frame.rows * 4;
    default:
      return frame.total() * frame.elemSize();
  }
}

static bool validate_buffer_size(segmecam::PipeWireOutput* self, pw_buffer* buffer, size_t expected_size) {
  struct spa_buffer* spa_buf = buffer->buffer;
  if (spa_buf->datas[0].maxsize < expected_size) {
    std::cerr << "PipeWire buffer too small: " << spa_buf->datas[0].maxsize << " < " << expected_size << std::endl;
    pw_stream_queue_buffer(self->GetStream(), buffer);
    return false;
  }
  return true;
}

static bool validate_frame_and_format(segmecam::PipeWireOutput* self, const cv::Mat& frame, pw_buffer* buffer, int process_count) {
  if (!check_format_negotiated(self, buffer, process_count)) return false;
  if (!check_buffers_ready(self, buffer, process_count)) return false;
  if (!validate_frame_dimensions(self, frame, buffer)) return false;

  spa_video_info_raw format = self->GetNegotiatedFormat();
  size_t expected_size = calculate_expected_size(frame, format);
  if (!validate_buffer_size(self, buffer, expected_size)) return false;

  return true;
}

// Helper function to convert frame to buffer
static bool convert_frame_to_buffer(segmecam::PipeWireOutput* self, const cv::Mat& frame, pw_buffer* buffer, int process_count) {
  spa_video_info_raw format = self->GetNegotiatedFormat();

  if (!buffer->buffer->datas[0].data) {
    std::cerr << "PipeWire buffer data pointer is null!" << std::endl;
    return false;
  }

  switch (format.format) {
    case SPA_VIDEO_FORMAT_YUY2:
      return segmecam::convert_yuy2_frame(frame, buffer, process_count);
    case SPA_VIDEO_FORMAT_BGR:
      return segmecam::convert_bgr_frame(frame, buffer);
    case SPA_VIDEO_FORMAT_RGB:
      return segmecam::convert_rgb_frame(frame, buffer);
    case SPA_VIDEO_FORMAT_BGRx:
      return segmecam::convert_bgrx_frame(frame, buffer);
    case SPA_VIDEO_FORMAT_RGBx:
      return segmecam::convert_rgbx_frame(frame, buffer);
    default:
      std::cerr << "PipeWire process callback: unsupported negotiated format: " << format.format << std::endl;
      return false;
  }
}

// Helper function to set buffer metadata
static void set_buffer_metadata(pw_buffer* buffer, size_t copy_size, int stride) {
  struct spa_buffer* spa_buf = buffer->buffer;

  // Set SPA buffer metadata for GStreamer compatibility
  spa_buf->datas[0].chunk->size = copy_size;
  spa_buf->datas[0].chunk->stride = stride;
  spa_buf->datas[0].chunk->flags = 0;
  spa_buf->datas[0].chunk->offset = 0;

  // Ensure all other data chunks are properly initialized (set to empty)
  for (uint32_t i = 1; i < spa_buf->n_datas; ++i) {
    if (spa_buf->datas[i].chunk) {
      spa_buf->datas[i].chunk->size = 0;
      spa_buf->datas[i].chunk->offset = 0;
      spa_buf->datas[i].chunk->stride = 0;
      spa_buf->datas[i].chunk->flags = 0;
    }
  }
}

namespace segmecam {

// Static callback functions

void on_stream_state_changed(void* data, enum pw_stream_state old_state,
                                   enum pw_stream_state new_state, const char* error) {
  if (!data) return;

  auto* events = static_cast<segmecam::PipeWireEvents*>(data);
  if (!events->output) return;

  const char* old_str = pw_stream_state_as_string(old_state);
  const char* new_str = pw_stream_state_as_string(new_state);
  std::cout << "PipeWire stream state changed: " << old_str << " -> " << new_str << std::endl;

  if (error) {
    std::cerr << "PipeWire stream error: " << error << std::endl;
  }

  // Log state-specific messages
  if (new_state == PW_STREAM_STATE_STREAMING) {
    std::cout << "PipeWire stream is now active and should be discoverable by OBS Studio" << std::endl;
  }
}

void on_stream_format_changed(void* data, uint32_t id, const struct spa_pod* param) {
  if (!data) return;

  auto* events = static_cast<segmecam::PipeWireEvents*>(data);
  if (!events->output) return;

  auto* self = events->output;

  std::cout << "🔄 PipeWire param callback: ENTER - id=" << id << std::endl;

  // Mark buffers ready (workaround for output streams)
  self->SetBuffersReady(true);

  if (id == SPA_PARAM_Buffers) {
    std::cout << "[PipeWire param callback] Buffers allocated - marking as ready" << std::endl;
    return;
  }

  if (id == SPA_PARAM_Format) {
    handle_format_parameter(self, param);
  }

  std::cout << "🔄 PipeWire param callback: EXIT" << std::endl;
}

void on_stream_process(void* data) {
  segmecam::PipeWireOutput* self = nullptr;

  if (!validate_process_callback_data(data, self)) {
    return;
  }

  pw_buffer* buffer = dequeue_and_validate_buffer(self);
  if (!buffer) {
    return;
  }

  // Log first few process calls
  static int process_count = 0;
  log_buffer_info(buffer, process_count);

  cv::Mat frame_to_send = get_frame_to_send(self, process_count);

  if (!frame_to_send.empty()) {
    if (!validate_frame_and_format(self, frame_to_send, buffer, process_count)) {
      return;
    }

    if (!convert_frame_to_buffer(self, frame_to_send, buffer, process_count)) {
      return;
    }
  } else {
    // No frame available at all (neither new nor last)
    if (process_count <= 3) {
      std::cout << "PipeWire process callback: no frame available at all" << std::endl;
    }
  }

  // Queue buffer back
  pw_stream_queue_buffer(self->GetStream(), buffer);
}

} // namespace segmecam