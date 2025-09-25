#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <spa/utils/hook.h>
#include <spa/param/video/raw.h>
#include <pipewire/stream.h>
#include "mediapipe/framework/port/opencv_core_inc.h"

// Forward declarations for PipeWire types
struct pw_loop;
struct pw_context;
struct pw_core;
struct pw_stream;
struct pw_properties;
struct pw_buffer;
struct pw_thread_loop;
struct spa_pod;

namespace segmecam {

// Forward declaration for PipeWireOutput
class PipeWireOutput;

// PipeWire event listener structure
struct PipeWireEvents {
  PipeWireEvents() {
    spa_zero(hook);
  }
  spa_hook hook;
  PipeWireOutput* output;
};

/**
 * @brief PipeWire-based video output for Flatpak sandboxed environments
 *
 * This class creates a PipeWire video stream that can be consumed by external
 * applications as a virtual camera source, replacing v4l2loopback device access
 * for Flathub compliance.
 */
class PipeWireOutput {
public:
  PipeWireOutput();
  ~PipeWireOutput();

  /**
   * @brief Initialize the PipeWire output stream
   * @param stream_name Name of the video stream (e.g., "SegmeCam Virtual Camera")
   * @param width Frame width in pixels
   * @param height Frame height in pixels
   * @param fps Target frame rate
   * @return true if initialization successful
   */
  bool Initialize(const std::string& stream_name, int width, int height, int fps = 30);

  /**
   * @brief Shut down the PipeWire output stream
   */
  void Shutdown();

  /**
   * @brief Send a frame to the PipeWire stream
   * @param frame OpenCV BGR frame to send
   * @return true if frame was queued successfully
   */
  bool SendFrame(const cv::Mat& frame);

  /**
   * @brief Set the negotiated video format from PipeWire
   * @param format The negotiated video format info
   */
  void SetNegotiatedFormat(const spa_video_info_raw& format) {
    std::unique_lock<std::mutex> lock(format_mutex_);
    negotiated_format_ = format;
    format_negotiated_ = true;
  }

  /**
   * @brief Get the negotiated video format
   * @return The negotiated format info
   */
  spa_video_info_raw GetNegotiatedFormat() const {
    std::unique_lock<std::mutex> lock(format_mutex_);
    return negotiated_format_;
  }

  /**
   * @brief Check if format has been negotiated
   * @return true if format is available
   */
  bool IsFormatNegotiated() const {
    std::unique_lock<std::mutex> lock(format_mutex_);
    return format_negotiated_;
  }

  /**
   * @brief Set buffers ready flag
   * @param ready true if buffers are allocated and ready
   */
  void SetBuffersReady(bool ready) {
    std::unique_lock<std::mutex> lock(buffers_mutex_);
    buffers_ready_ = ready;
  }

  /**
   * @brief Check if buffers are ready
   * @return true if buffers are allocated and ready
   */
  bool AreBuffersReady() const {
    std::unique_lock<std::mutex> lock(buffers_mutex_);
    return buffers_ready_;
  }

  /**
   * @brief Check if the output stream is active
   * @return true if stream is initialized and active
   */
  bool IsActive() const { return active_; }

  /**
   * @brief Get the stream name
   * @return Current stream name
   */
  const std::string& GetStreamName() const { return stream_name_; }

  // Public accessors for callback functions
  pw_stream* GetStream() { return stream_; }
  std::mutex& GetFrameMutex() { return frame_mutex_; }
  cv::Mat& GetCurrentFrame() { return current_frame_; }
  bool& GetFrameReady() { return frame_ready_; }
  cv::Mat& GetLastFrame() { return last_frame_; }

private:
  /**
   * @brief Create PipeWire stream properties
   * @return Properties object or nullptr on failure
   */
  pw_properties* CreateStreamProperties();

  /**
   * @brief Initialize and start the PipeWire thread loop
   * @return true if successful
   */
  bool InitializeThreadLoop();

  /**
   * @brief Create PipeWire context and connect to core
   * @return true if successful
   */
  bool CreateContextAndCore();

  /**
   * @brief Create and setup PipeWire stream with event listeners
   * @param props Stream properties
   * @return true if successful
   */
  bool CreateAndSetupStream(pw_properties* props);

  /**
   * @brief Build format and buffer parameters for the stream
   * @param params Array to store parameters
   * @param max_params Maximum number of parameters
   * @return Number of parameters created
   */
  int BuildFormatParameters(const spa_pod** params, int max_params);

  /**
   * @brief Connect and start the PipeWire stream
   * @param params Format parameters
   * @param param_count Number of parameters
   * @return true if successful
   */
  bool ConnectAndStartStream(const spa_pod** params, int param_count);

  /**
   * @brief Create the PipeWire stream and connect to core
   * @return true if stream creation successful
   */
  bool CreatePipeWireStream();

  /**
   * @brief Destroy the PipeWire stream and cleanup resources
   */
  void DestroyPipeWireStream();

  /**
   * @brief Start the PipeWire event loop thread
   */
  void StartLoopThread();

  /**
   * @brief Stop the PipeWire event loop thread
   */
  void StopLoopThread();

  // PipeWire objects
  pw_thread_loop* thread_loop_ = nullptr;
  pw_context* context_ = nullptr;
  pw_core* core_ = nullptr;
  pw_stream* stream_ = nullptr;

  // Stream configuration
  std::string stream_name_;
  int width_ = 0;
  int height_ = 0;
  int fps_ = 30;

  // State management
  std::atomic<bool> initialized_{false};
  std::atomic<bool> active_{false};

  // Frame management
  std::mutex frame_mutex_;
  std::condition_variable frame_cv_;
  cv::Mat current_frame_;
  cv::Mat last_frame_;  // Store last sent frame for reuse when no new frame available
  bool frame_ready_ = false;

  // Format negotiation
  mutable std::mutex format_mutex_;
  spa_video_info_raw negotiated_format_ = {};
  bool format_negotiated_ = false;

  // Buffer readiness
  mutable std::mutex buffers_mutex_;
  bool buffers_ready_ = false;

  // Event listener management
  PipeWireEvents* events_ = nullptr;
  spa_hook stream_listener_;
  pw_stream_events stream_events_;
};

} // namespace segmecam