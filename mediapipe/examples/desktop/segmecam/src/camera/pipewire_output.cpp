#include "include/camera/pipewire_output.h"
#include "include/camera/pipewire_frame_converters.h"
#include "include/camera/pipewire_callbacks.h"

#include <iostream>
#include <cstring>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/type-info.h>
#include <spa/param/video/raw.h>
#include <spa/pod/builder.h>
#include <spa/param/param.h>
#include <opencv2/opencv.hpp>

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

bool PipeWireOutput::check_already_initialized(const std::string& stream_name, int width, int height, int fps) {
  if (initialized_ && stream_name_ == stream_name && width_ == width && height_ == height && fps_ == fps) {
    std::cout << "PipeWire output already initialized with matching parameters, skipping re-initialization" << std::endl;
    return false;  // Don't proceed with initialization
  }

  if (initialized_) {
    std::cout << "PipeWire output already initialized with different parameters, shutting down first..." << std::endl;
    Shutdown();
  }

  return true;  // Proceed with initialization
}

bool PipeWireOutput::validate_parameters(int width, int height, int fps, const std::string& stream_name) {
  if (width <= 0 || height <= 0 || fps <= 0) {
    std::cerr << "Invalid PipeWire parameters: " << width << "x" << height << "@" << fps << "fps" << std::endl;
    return false;
  }

  if (stream_name.empty()) {
    std::cerr << "Empty stream name provided" << std::endl;
    return false;
  }

  return true;
}

void PipeWireOutput::set_parameters(const std::string& stream_name, int width, int height, int fps) {
  stream_name_ = stream_name;
  width_ = width;
  height_ = height;
  fps_ = fps;

  std::cout << "Initializing PipeWire output: " << stream_name << " (" << width << "x" << height << "@" << fps << "fps)" << std::endl;
}

bool PipeWireOutput::Initialize(const std::string& stream_name, int width, int height, int fps) {
  if (!check_already_initialized(stream_name, width, height, fps)) {
    return true;  // Already initialized with same params
  }

  if (!validate_parameters(width, height, fps, stream_name)) {
    return false;
  }

  set_parameters(stream_name, width, height, fps);

  if (!CreatePipeWireStream()) {
    std::cerr << "Failed to create PipeWire stream" << std::endl;
    return false;
  }

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

bool PipeWireOutput::validate_state_and_frame(const cv::Mat& frame) {
  return active_ && initialized_ && !frame.empty();
}

bool PipeWireOutput::validate_frame_properties(const cv::Mat& frame) {
  if (frame.cols != width_ || frame.rows != height_) {
    std::cerr << "Frame dimensions mismatch: expected " << width_ << "x" << height_
              << ", got " << frame.cols << "x" << frame.rows << std::endl;
    return false;
  }

  if (frame.channels() != 3 && frame.channels() != 4) {
    std::cerr << "Unsupported frame format: " << frame.channels() << " channels" << std::endl;
    return false;
  }

  return true;
}

bool PipeWireOutput::store_frame(const cv::Mat& frame) {
  try {
    cv::Mat frame_to_store = frame.clone();
    {
      std::unique_lock<std::mutex> lock(frame_mutex_);
      current_frame_ = frame_to_store;
      frame_ready_ = true;
    }
    frame_cv_.notify_one();
    return true;
  } catch (const std::exception& e) {
    std::cerr << "❌ Exception in PipeWireOutput::store_frame: " << e.what() << std::endl;
    return false;
  } catch (...) {
    std::cerr << "❌ Unknown exception in PipeWireOutput::store_frame" << std::endl;
    return false;
  }
}

bool PipeWireOutput::SendFrame(const cv::Mat& frame) {
  if (!validate_state_and_frame(frame)) {
    return false;
  }

  if (!validate_frame_properties(frame)) {
    return false;
  }

  return store_frame(frame);
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
  stream_events_.state_changed = segmecam::on_stream_state_changed;
  stream_events_.process = segmecam::on_stream_process;
  stream_events_.param_changed = segmecam::on_stream_format_changed;

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