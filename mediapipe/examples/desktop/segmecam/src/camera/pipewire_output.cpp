#include "include/camera/pipewire_output.h"
#include "include/camera/gstreamer_buffer_utils.h"

#include <iostream>
#include <cstring>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/type-info.h>
#include <spa/param/video/raw.h>
#include <spa/param/buffers.h>
#include <spa/pod/builder.h>
#include <spa/param/param.h>
static void log_buffer_info(pw_buffer* buffer, int& process_count) {
  if (process_count < 3) {
    struct spa_buffer* spa_buf = buffer->buffer;
    std::cout << "PipeWire buffer info: n_datas=" << spa_buf->n_datas
              << ", datas[0].maxsize=" << spa_buf->datas[0].maxsize << std::endl;
    std::cout << "PipeWire process callback #" << process_count << " - buffer valid" << std::endl;
    process_count++;
  }
}
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include <opencv2/opencv.hpp>
// Helper functions for frame conversion
static bool convert_yuy2_frame(const cv::Mat& frame, pw_buffer* buffer, int process_count);
static bool convert_bgr_frame(const cv::Mat& frame, pw_buffer* buffer);
static bool convert_rgb_frame(const cv::Mat& frame, pw_buffer* buffer);
static bool convert_bgrx_frame(const cv::Mat& frame, pw_buffer* buffer);
static bool convert_rgbx_frame(const cv::Mat& frame, pw_buffer* buffer);

// Static callback functions

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

static void on_stream_state_changed(void* data, enum pw_stream_state old_state,
                                   enum pw_stream_state new_state, const char* error) {
  if (!data) return;

  auto* events = static_cast<segmecam::PipeWireEvents*>(data);
  if (!events || !events->output) return;

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

// Helper function to validate frame and format
static bool validate_frame_and_format(segmecam::PipeWireOutput* self, const cv::Mat& frame, pw_buffer* buffer, int process_count) {
  // Check if format has been negotiated
  if (!self->IsFormatNegotiated()) {
    if (process_count <= 3) {
      std::cout << "PipeWire process callback: format not negotiated yet, skipping frame" << std::endl;
    }
    pw_stream_queue_buffer(self->GetStream(), buffer);
    return false;
  }

  // Check if buffers are ready
  if (!self->AreBuffersReady()) {
    if (process_count <= 3) {
      std::cout << "PipeWire process callback: buffers not ready yet (AreBuffersReady()=" << self->AreBuffersReady() << "), skipping frame" << std::endl;
    }
    pw_stream_queue_buffer(self->GetStream(), buffer);
    return false;
  }

  // Validate frame dimensions against negotiated format
  spa_video_info_raw format = self->GetNegotiatedFormat();
  if (frame.cols != (int)format.size.width || frame.rows != (int)format.size.height) {
    std::cerr << "PipeWire process callback: frame dimensions mismatch - frame: "
              << frame.cols << "x" << frame.rows
              << ", negotiated: " << format.size.width << "x" << format.size.height << std::endl;
    pw_stream_queue_buffer(self->GetStream(), buffer);
    return false;
  }

  // Calculate expected size based on negotiated format
  size_t expected_size = 0;
  switch (format.format) {
    case SPA_VIDEO_FORMAT_YUY2:
    case SPA_VIDEO_FORMAT_UYVY:
      expected_size = frame.cols * frame.rows * 2;
      break;
    case SPA_VIDEO_FORMAT_BGR:
      expected_size = frame.total() * frame.elemSize();
      break;
    case SPA_VIDEO_FORMAT_RGB:
    case SPA_VIDEO_FORMAT_BGRx:
    case SPA_VIDEO_FORMAT_RGBx:
      expected_size = frame.cols * frame.rows * 4;
      break;
    default:
      expected_size = frame.total() * frame.elemSize();
      break;
  }

  struct spa_buffer* spa_buf = buffer->buffer;
  if (spa_buf->datas[0].maxsize < expected_size) {
    std::cerr << "PipeWire buffer too small: " << spa_buf->datas[0].maxsize << " < " << expected_size << std::endl;
    pw_stream_queue_buffer(self->GetStream(), buffer);
    return false;
  }

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
      return convert_yuy2_frame(frame, buffer, process_count);
    case SPA_VIDEO_FORMAT_BGR:
      return convert_bgr_frame(frame, buffer);
    case SPA_VIDEO_FORMAT_RGB:
      return convert_rgb_frame(frame, buffer);
    case SPA_VIDEO_FORMAT_BGRx:
      return convert_bgrx_frame(frame, buffer);
    case SPA_VIDEO_FORMAT_RGBx:
      return convert_rgbx_frame(frame, buffer);
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

// Helper function to convert YUY2 frame
static bool convert_yuy2_frame(const cv::Mat& frame, pw_buffer* buffer, int process_count) {
  struct spa_buffer* spa_buf = buffer->buffer;
  size_t copy_size = frame.cols * frame.rows * 2;
  int stride = frame.cols * 2;

  if (spa_buf->datas[0].maxsize < copy_size) {
    std::cerr << "PipeWire buffer too small for YUY2 frame: " << spa_buf->datas[0].maxsize << " < " << copy_size << std::endl;
    return false;
  }

  if (process_count <= 3) {
    std::cout << "[YUY2] copy_size=" << copy_size << ", maxsize=" << spa_buf->datas[0].maxsize << ", stride=" << stride << std::endl;
    std::cout << "[YUY2] frame type=" << frame.type() << ", step=" << frame.step << ", cols=" << frame.cols << ", rows=" << frame.rows << std::endl;
  }

  memset(spa_buf->datas[0].data, 0x80, copy_size);

  if (frame.type() == CV_8UC3 && frame.isContinuous() && frame.cols % 2 == 0) {
    segmecam::BGRToYUY2(frame, static_cast<uint8_t*>(spa_buf->datas[0].data));
  } else {
    uint8_t* yuy2 = static_cast<uint8_t*>(spa_buf->datas[0].data);
    for (size_t i = 0; i < copy_size; i += 4) {
      yuy2[i+0] = 128; // Y0
      yuy2[i+1] = 128; // U
      yuy2[i+2] = 128; // Y1
      yuy2[i+3] = 128; // V
    }
  }

  if (process_count <= 3) {
    uint8_t* yuy2 = static_cast<uint8_t*>(spa_buf->datas[0].data);
    std::cout << "[YUY2] First 16 bytes: ";
    for (int i = 0; i < 16 && i < (int)copy_size; ++i) {
      std::cout << std::hex << (int)yuy2[i] << " ";
    }
    std::cout << std::dec << std::endl;
  }

  set_buffer_metadata(buffer, copy_size, stride);
  return true;
}

// Helper function to convert BGR frame
static bool convert_bgr_frame(const cv::Mat& frame, pw_buffer* buffer) {
  struct spa_buffer* spa_buf = buffer->buffer;
  size_t copy_size = frame.total() * frame.elemSize();
  int stride = frame.cols * frame.elemSize();

  if (spa_buf->datas[0].maxsize < copy_size) {
    std::cerr << "PipeWire buffer too small for BGR frame: " << spa_buf->datas[0].maxsize << " < " << copy_size << std::endl;
    return false;
  }

  memcpy(spa_buf->datas[0].data, frame.data, copy_size);
  set_buffer_metadata(buffer, copy_size, stride);
  return true;
}

// Helper function to convert RGB frame
static bool convert_rgb_frame(const cv::Mat& frame, pw_buffer* buffer) {
  struct spa_buffer* spa_buf = buffer->buffer;
  cv::Mat converted;
  cv::cvtColor(frame, converted, cv::COLOR_BGR2RGB);
  size_t copy_size = converted.total() * converted.elemSize();
  int stride = converted.cols * converted.elemSize();

  if (spa_buf->datas[0].maxsize < copy_size) {
    std::cerr << "PipeWire buffer too small for RGB frame: " << spa_buf->datas[0].maxsize << " < " << copy_size << std::endl;
    return false;
  }

  memcpy(spa_buf->datas[0].data, converted.data, copy_size);
  set_buffer_metadata(buffer, copy_size, stride);
  return true;
}

// Helper function to convert BGRx frame
static bool convert_bgrx_frame(const cv::Mat& frame, pw_buffer* buffer) {
  struct spa_buffer* spa_buf = buffer->buffer;
  cv::Mat converted;
  cv::cvtColor(frame, converted, cv::COLOR_BGR2BGRA);
  size_t copy_size = converted.total() * converted.elemSize();
  int stride = converted.cols * converted.elemSize();

  if (spa_buf->datas[0].maxsize < copy_size) {
    std::cerr << "PipeWire buffer too small for BGRx frame: " << spa_buf->datas[0].maxsize << " < " << copy_size << std::endl;
    return false;
  }

  memcpy(spa_buf->datas[0].data, converted.data, copy_size);
  set_buffer_metadata(buffer, copy_size, stride);
  return true;
}

// Helper function to convert RGBx frame
static bool convert_rgbx_frame(const cv::Mat& frame, pw_buffer* buffer) {
  struct spa_buffer* spa_buf = buffer->buffer;
  cv::Mat converted;
  cv::cvtColor(frame, converted, cv::COLOR_BGR2RGBA);
  size_t copy_size = converted.total() * converted.elemSize();
  int stride = converted.cols * converted.elemSize();

  if (spa_buf->datas[0].maxsize < copy_size) {
    std::cerr << "PipeWire buffer too small for RGBx frame: " << spa_buf->datas[0].maxsize << " < " << copy_size << std::endl;
    return false;
  }

  memcpy(spa_buf->datas[0].data, converted.data, copy_size);
  set_buffer_metadata(buffer, copy_size, stride);
  return true;
}

static void on_stream_format_changed(void* data, uint32_t id, const struct spa_pod* param) {
  if (!data) return;

  auto* events = static_cast<segmecam::PipeWireEvents*>(data);
  if (!events || !events->output) return;

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

static void on_stream_process(void* data) {
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

namespace segmecam {

PipeWireOutput::PipeWireOutput() {
  // Initialize PipeWire once per process
  static bool pipewire_initialized = false;
  if (!pipewire_initialized) {
    pw_init(nullptr, nullptr);
    pipewire_initialized = true;
  }

  // Initialize the stream listener hook
  spa_zero(stream_listener_);
}

PipeWireOutput::~PipeWireOutput() {
  Shutdown();
}

bool PipeWireOutput::Initialize(const std::string& stream_name, int width, int height, int fps) {
  // If already initialized with the same parameters, just return success
  if (initialized_ && stream_name_ == stream_name && width_ == width && height_ == height && fps_ == fps) {
    std::cout << "PipeWire output already initialized with matching parameters, skipping re-initialization" << std::endl;
    return true;
  }

  // If initialized with different parameters, we need to shut down first
  if (initialized_) {
    std::cout << "PipeWire output already initialized with different parameters, shutting down first..." << std::endl;
    Shutdown();
  }

  // Validate parameters
  if (width <= 0 || height <= 0 || fps <= 0) {
    std::cerr << "Invalid PipeWire parameters: " << width << "x" << height << "@" << fps << "fps" << std::endl;
    return false;
  }

  if (stream_name.empty()) {
    std::cerr << "Empty stream name provided" << std::endl;
    return false;
  }

  stream_name_ = stream_name;
  width_ = width;
  height_ = height;
  fps_ = fps;

  std::cout << "Initializing PipeWire output: " << stream_name << " (" << width << "x" << height << "@" << fps << "fps)" << std::endl;

  if (!CreatePipeWireStream()) {
    std::cerr << "Failed to create PipeWire stream" << std::endl;
    return false;
  }

  // Thread loop is now started in CreatePipeWireStream()

  initialized_ = true;
  active_ = true;
  std::cout << "PipeWire output initialized successfully: " << stream_name << " (" << width << "x" << height << "@" << fps << "fps)" << std::endl;
  return true;
}

void PipeWireOutput::Shutdown() {
  std::cout << "PipeWire output shutdown initiated" << std::endl;

  // First, set active to false to prevent new frames from being processed
  active_ = false;

  // Wait a bit for any in-flight callbacks to complete
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // CRITICAL: Lock the thread loop before destroying resources to prevent race conditions
  if (thread_loop_) {
    pw_thread_loop_lock(thread_loop_);

    // Now safely destroy PipeWire resources while thread loop is locked
    DestroyPipeWireStream();

    // Unlock thread loop before stopping it
    pw_thread_loop_unlock(thread_loop_);

    // Now stop and destroy the thread loop
    pw_thread_loop_stop(thread_loop_);
    pw_thread_loop_destroy(thread_loop_);
    thread_loop_ = nullptr;
  } else {
    // If no thread loop, just clean up resources
    DestroyPipeWireStream();
  }

  // Clear any pending frames
  {
    std::unique_lock<std::mutex> lock(frame_mutex_);
    current_frame_ = cv::Mat();
    frame_ready_ = false;
  }

  initialized_ = false;
  std::cout << "PipeWire output shutdown complete" << std::endl;
}

bool PipeWireOutput::SendFrame(const cv::Mat& frame) {
  if (!active_ || !initialized_ || frame.empty()) {
    return false;
  }

  // Validate frame dimensions
  if (frame.cols != width_ || frame.rows != height_) {
    std::cerr << "Frame dimensions mismatch: expected " << width_ << "x" << height_
              << ", got " << frame.cols << "x" << frame.rows << std::endl;
    return false;
  }

  // Validate frame format
  if (frame.channels() != 3 && frame.channels() != 4) {
    std::cerr << "Unsupported frame format: " << frame.channels() << " channels" << std::endl;
    return false;
  }

  try {
    // Store frame for PipeWire callback (keep original BGR format)
    cv::Mat frame_to_store = frame.clone();

    // Store frame for PipeWire callback
    {
      std::unique_lock<std::mutex> lock(frame_mutex_);
      current_frame_ = frame_to_store;
      frame_ready_ = true;
    }

    frame_cv_.notify_one();
    return true;
  } catch (const std::exception& e) {
    std::cerr << "❌ Exception in PipeWireOutput::SendFrame: " << e.what() << std::endl;
    return false;
  } catch (...) {
    std::cerr << "❌ Unknown exception in PipeWireOutput::SendFrame" << std::endl;
    return false;
  }
}

bool PipeWireOutput::CreatePipeWireStream() {
  // Create properties for the stream - these are crucial for OBS discovery
  pw_properties* props = CreateStreamProperties();
  if (!props) {
    return false;
  }

  // Initialize thread loop
  if (!InitializeThreadLoop()) {
    pw_properties_free(props);
    return false;
  }

  // Create context and connect to core
  if (!CreateContextAndCore()) {
    pw_properties_free(props);
    return false;
  }

  // Create and setup stream
  if (!CreateAndSetupStream(props)) {
    return false;
  }

  // Build format parameters
  const spa_pod* params[7];
  int param_count = BuildFormatParameters(params, 7);

  // Connect and start stream
  if (!ConnectAndStartStream(params, param_count)) {
    return false;
  }

  std::cout << "PipeWire stream created and connected" << std::endl;
  return true;
}

pw_properties* PipeWireOutput::CreateStreamProperties() {
  // Create properties for the stream - these are crucial for OBS discovery
  pw_properties* props = pw_properties_new(
      PW_KEY_MEDIA_TYPE, "Video",
      PW_KEY_MEDIA_CATEGORY, "Capture",
      PW_KEY_MEDIA_ROLE, "Camera",
      PW_KEY_MEDIA_CLASS, "Video/Source",
      PW_KEY_NODE_NAME, stream_name_.c_str(),
      PW_KEY_NODE_DESCRIPTION, stream_name_.c_str(),
      PW_KEY_NODE_NICK, stream_name_.c_str(),
      PW_KEY_APP_NAME, "SegmeCam",
      PW_KEY_APP_ID, "org.segmecam.SegmeCam",
      PW_KEY_NODE_WANT_DRIVER, "false",
      PW_KEY_NODE_ALWAYS_PROCESS, "true",
      nullptr);

  if (!props) {
    std::cerr << "Failed to create PipeWire properties" << std::endl;
    return nullptr;
  }

  // Debug: Print properties
  std::cout << "PipeWire stream properties:" << std::endl;
  void *state = nullptr;
  const char *key;
  while ((key = pw_properties_iterate(props, &state))) {
    const char *value = pw_properties_get(props, key);
    std::cout << "  " << key << " = " << (value ? value : "(null)") << std::endl;
  }

  return props;
}

bool PipeWireOutput::InitializeThreadLoop() {
  // Create PipeWire thread loop FIRST (following OBS pattern)
  thread_loop_ = pw_thread_loop_new("segmecam-pw", nullptr);
  if (!thread_loop_) {
    std::cerr << "Failed to create PipeWire thread loop" << std::endl;
    return false;
  }

  // START the thread loop BEFORE creating context/core/stream (critical!)
  std::cout << "Starting PipeWire thread loop..." << std::endl;
  int start_result = pw_thread_loop_start(thread_loop_);
  if (start_result < 0) {
    std::cerr << "Failed to start PipeWire thread loop: " << strerror(-start_result) << std::endl;
    return false;
  }
  std::cout << "PipeWire thread loop started successfully" << std::endl;
  return true;
}

bool PipeWireOutput::CreateContextAndCore() {
  // LOCK the thread loop for all PipeWire operations (critical!)
  pw_thread_loop_lock(thread_loop_);

  // Get the loop from the thread loop
  pw_loop* loop = pw_thread_loop_get_loop(thread_loop_);
  std::cout << "PipeWire thread loop locked, creating context..." << std::endl;

  // Create PipeWire context WITHIN the locked thread loop
  context_ = pw_context_new(loop, nullptr, 0);
  if (!context_) {
    std::cerr << "Failed to create PipeWire context" << std::endl;
    pw_thread_loop_unlock(thread_loop_);
    return false;
  }
  std::cout << "PipeWire context created successfully" << std::endl;

  // Connect to PipeWire core WITHIN the locked thread loop
  core_ = pw_context_connect(context_, nullptr, 0);
  if (!core_) {
    std::cerr << "Failed to connect to PipeWire core" << std::endl;
    pw_thread_loop_unlock(thread_loop_);
    return false;
  }
  std::cout << "PipeWire core connected successfully" << std::endl;
  return true;
}

bool PipeWireOutput::CreateAndSetupStream(pw_properties* props) {
  // Create PipeWire stream WITHIN the locked thread loop
  stream_ = pw_stream_new(core_, stream_name_.c_str(), props);
  if (!stream_) {
    std::cerr << "Failed to create PipeWire stream" << std::endl;
    pw_thread_loop_unlock(thread_loop_);
    return false;
  }
  std::cout << "PipeWire stream created successfully" << std::endl;

  // Set up event listeners WITHIN the locked thread loop
  // Clean up any existing listeners first
  if (events_) {
    spa_hook_remove(&stream_listener_);
    delete events_;
  }
  // Reset the hook before reuse
  spa_zero(stream_listener_);
  events_ = new PipeWireEvents();
  events_->output = this;

  // Initialize stream events struct
  stream_events_ = {};
  stream_events_.version = PW_VERSION_STREAM_EVENTS;
  stream_events_.state_changed = on_stream_state_changed;
  stream_events_.process = on_stream_process;
  stream_events_.param_changed = on_stream_format_changed;

  pw_stream_add_listener(stream_, &stream_listener_, &stream_events_, events_);
  std::cout << "PipeWire stream event listeners added" << std::endl;
  return true;
}

int PipeWireOutput::BuildFormatParameters(const spa_pod** params, int max_params) {
  // Build format parameters for the video stream (advertise multiple formats)
  std::cout << "Building PipeWire format parameters (multi-format)..." << std::endl;
  uint8_t buffer[4096];
  spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

  spa_video_info_raw video_format = {};
  video_format.size.width = width_;
  video_format.size.height = height_;
  video_format.framerate.num = fps_;
  video_format.framerate.denom = 1;

  // List of formats to advertise (add YUY2 and UYVY for v4l2sink compatibility)
  int param_count = 0;

  // BGR
  video_format.format = SPA_VIDEO_FORMAT_BGR;
  params[param_count++] = spa_format_video_raw_build(&b, SPA_PARAM_EnumFormat, &video_format);
  // RGB
  video_format.format = SPA_VIDEO_FORMAT_RGB;
  params[param_count++] = spa_format_video_raw_build(&b, SPA_PARAM_EnumFormat, &video_format);
  // BGRx
  video_format.format = SPA_VIDEO_FORMAT_BGRx;
  params[param_count++] = spa_format_video_raw_build(&b, SPA_PARAM_EnumFormat, &video_format);
  // RGBx
  video_format.format = SPA_VIDEO_FORMAT_RGBx;
  params[param_count++] = spa_format_video_raw_build(&b, SPA_PARAM_EnumFormat, &video_format);
  // YUY2
  video_format.format = SPA_VIDEO_FORMAT_YUY2;
  params[param_count++] = spa_format_video_raw_build(&b, SPA_PARAM_EnumFormat, &video_format);
  // UYVY
  video_format.format = SPA_VIDEO_FORMAT_UYVY;
  params[param_count++] = spa_format_video_raw_build(&b, SPA_PARAM_EnumFormat, &video_format);

  // Add buffer parameters to tell PipeWire how to allocate buffers
  // Calculate buffer size for YUY2 (2 bytes per pixel)
  size_t buffer_size = width_ * height_ * 2;
  spa_pod* buffer_param = static_cast<spa_pod*>(spa_pod_builder_add_object(&b,
      SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
      SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(4, 1, 8),  // 4 buffers, min 1, max 8
      SPA_PARAM_BUFFERS_blocks,  SPA_POD_Int(1),
      SPA_PARAM_BUFFERS_size,    SPA_POD_CHOICE_RANGE_Int(buffer_size, buffer_size, buffer_size * 2),
      SPA_PARAM_BUFFERS_stride,  SPA_POD_Int(width_ * 2),
      SPA_PARAM_BUFFERS_align,   SPA_POD_Int(16)));
  params[param_count++] = buffer_param;

  return param_count;
}

bool PipeWireOutput::ConnectAndStartStream(const spa_pod** params, int param_count) {
  std::cout << "Connecting PipeWire stream with format and buffer parameters..." << std::endl;
  int result = pw_stream_connect(
      stream_, PW_DIRECTION_OUTPUT, PW_ID_ANY,
      static_cast<pw_stream_flags>(PW_STREAM_FLAG_MAP_BUFFERS),
      params, param_count);
  if (result < 0) {
    std::cerr << "Failed to connect PipeWire stream with format params: " << strerror(-result) << " (error code: " << result << ")" << std::endl;
    pw_thread_loop_unlock(thread_loop_);
    return false;
  }
  std::cout << "PipeWire stream connected successfully with format and buffer parameters, unlocking thread loop..." << std::endl;

  // UNLOCK the thread loop after all operations are complete
  pw_thread_loop_unlock(thread_loop_);
  std::cout << "PipeWire thread loop unlocked" << std::endl;

  // Start the stream to begin format negotiation and streaming
  std::cout << "Starting PipeWire stream..." << std::endl;
  pw_thread_loop_lock(thread_loop_);
  int active_result = pw_stream_set_active(stream_, true);
  if (active_result < 0) {
    std::cerr << "Failed to start PipeWire stream: " << strerror(-active_result) << std::endl;
    pw_thread_loop_unlock(thread_loop_);
    return false;
  }
  std::cout << "PipeWire stream started successfully" << std::endl;
  pw_thread_loop_unlock(thread_loop_);

  // Don't wait for stabilization - let it happen asynchronously
  // The state callbacks will handle when the stream is ready
  std::cout << "PipeWire stream initialization complete - will stabilize asynchronously" << std::endl;
  return true;
}

void PipeWireOutput::DestroyPipeWireStream() {
  // NOTE: Caller must lock thread_loop_ before calling this function

  // Clean up event listeners FIRST while thread loop is locked
  if (events_) {
    spa_hook_remove(&stream_listener_);
    // Reset the hook after removal
    spa_zero(stream_listener_);
    delete events_;
    events_ = nullptr;
  }

  // Destroy stream
  if (stream_) {
    pw_stream_destroy(stream_);
    stream_ = nullptr;
  }

  // Destroy context
  if (context_) {
    pw_context_destroy(context_);
    context_ = nullptr;
  }

  // NOTE: Thread loop stopping/destroying is now handled by caller after unlocking

  std::cout << "PipeWire resources cleaned up" << std::endl;
}

void PipeWireOutput::StartLoopThread() {
  std::cout << "Starting PipeWire thread loop..." << std::endl;
  // Start the PipeWire thread loop - it handles event processing internally
  if (thread_loop_) {
    std::cout << "Calling pw_thread_loop_start()..." << std::endl;
    int result = pw_thread_loop_start(thread_loop_);
    if (result < 0) {
      std::cerr << "Failed to start PipeWire thread loop: " << strerror(-result) << " (error code: " << result << ")" << std::endl;
      return;
    }
    std::cout << "pw_thread_loop_start() returned successfully" << std::endl;
  } else {
    std::cerr << "thread_loop_ is null!" << std::endl;
    return;
  }
  std::cout << "PipeWire thread loop started successfully" << std::endl;
  std::cout << "Started PipeWire thread loop for event processing" << std::endl;
}

void PipeWireOutput::StopLoopThread() {
  // Stop the PipeWire thread loop
  if (thread_loop_) {
    pw_thread_loop_stop(thread_loop_);
  }

  std::cout << "Stopped PipeWire thread loop" << std::endl;
}

} // namespace segmecam