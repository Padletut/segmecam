#include "include/camera/pipewire_frame_converters.h"
#include "include/camera/gstreamer_buffer_utils.h"

#include <iostream>
#include <cstring>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/type-info.h>
#include <spa/param/video/raw.h>
#include <spa/pod/builder.h>
#include <spa/param/param.h>
#include <opencv2/opencv.hpp>

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

// Helper functions for YUY2 conversion
static bool validate_yuy2_buffer_size(struct spa_buffer* spa_buf, size_t copy_size);
static void log_yuy2_conversion_info(int process_count, size_t copy_size, size_t maxsize, int stride, const cv::Mat& frame);
static void perform_yuy2_conversion(const cv::Mat& frame, struct spa_buffer* spa_buf);
static void log_yuy2_first_bytes(int process_count, void* data, size_t copy_size);

// Helper function to validate YUY2 buffer size
static bool validate_yuy2_buffer_size(struct spa_buffer* spa_buf, size_t copy_size) {
  if (spa_buf->datas[0].maxsize < copy_size) {
    std::cerr << "PipeWire buffer too small for YUY2 frame: " << spa_buf->datas[0].maxsize << " < " << copy_size << std::endl;
    return false;
  }
  return true;
}

// Helper function to log YUY2 conversion info
static void log_yuy2_conversion_info(int process_count, size_t copy_size, size_t maxsize, int stride, const cv::Mat& frame) {
  if (process_count <= 3) {
    std::cout << "[YUY2] copy_size=" << copy_size << ", maxsize=" << maxsize << ", stride=" << stride << std::endl;
    std::cout << "[YUY2] frame type=" << frame.type() << ", step=" << frame.step << ", cols=" << frame.cols << ", rows=" << frame.rows << std::endl;
  }
}

// Helper function to perform YUY2 conversion
static void perform_yuy2_conversion(const cv::Mat& frame, struct spa_buffer* spa_buf) {
  if (frame.type() == CV_8UC3 && frame.isContinuous() && frame.cols % 2 == 0) {
    segmecam::BGRToYUY2(frame, static_cast<uint8_t*>(spa_buf->datas[0].data));
  } else {
    uint8_t* yuy2 = static_cast<uint8_t*>(spa_buf->datas[0].data);
    size_t copy_size = frame.cols * frame.rows * 2;
    for (size_t i = 0; i < copy_size; i += 4) {
      yuy2[i+0] = 128; // Y0
      yuy2[i+1] = 128; // U
      yuy2[i+2] = 128; // Y1
      yuy2[i+3] = 128; // V
    }
  }
}

// Helper function to log YUY2 first bytes
static void log_yuy2_first_bytes(int process_count, void* data, size_t copy_size) {
  if (process_count <= 3) {
    uint8_t* yuy2 = static_cast<uint8_t*>(data);
    std::cout << "[YUY2] First 16 bytes: ";
    for (int i = 0; i < 16 && i < (int)copy_size; ++i) {
      std::cout << std::hex << (int)yuy2[i] << " ";
    }
    std::cout << std::dec << std::endl;
  }
}

namespace segmecam {

// Convert YUY2 frame
bool convert_yuy2_frame(const cv::Mat& frame, pw_buffer* buffer, int process_count) {
  struct spa_buffer* spa_buf = buffer->buffer;
  size_t copy_size = frame.cols * frame.rows * 2;
  int stride = frame.cols * 2;

  if (!validate_yuy2_buffer_size(spa_buf, copy_size)) return false;

  log_yuy2_conversion_info(process_count, copy_size, spa_buf->datas[0].maxsize, stride, frame);

  memset(spa_buf->datas[0].data, 0x80, copy_size);

  perform_yuy2_conversion(frame, spa_buf);

  log_yuy2_first_bytes(process_count, spa_buf->datas[0].data, copy_size);

  set_buffer_metadata(buffer, copy_size, stride);
  return true;
}

// Convert BGR frame
bool convert_bgr_frame(const cv::Mat& frame, pw_buffer* buffer) {
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

// Helper function to copy converted frame to PipeWire buffer
static bool copy_converted_frame_to_buffer(const cv::Mat& converted, pw_buffer* buffer, const std::string& format_name) {
  struct spa_buffer* spa_buf = buffer->buffer;
  size_t copy_size = converted.total() * converted.elemSize();
  int stride = converted.cols * converted.elemSize();

  if (spa_buf->datas[0].maxsize < copy_size) {
    std::cerr << "PipeWire buffer too small for " << format_name << " frame: " << spa_buf->datas[0].maxsize << " < " << copy_size << std::endl;
    return false;
  }

  memcpy(spa_buf->datas[0].data, converted.data, copy_size);
  set_buffer_metadata(buffer, copy_size, stride);
  return true;
}

// Convert RGB frame
bool convert_rgb_frame(const cv::Mat& frame, pw_buffer* buffer) {
  cv::Mat converted;
  cv::cvtColor(frame, converted, cv::COLOR_BGR2RGB);
  return copy_converted_frame_to_buffer(converted, buffer, "RGB");
}

// Convert BGRx frame
bool convert_bgrx_frame(const cv::Mat& frame, pw_buffer* buffer) {
  cv::Mat converted;
  cv::cvtColor(frame, converted, cv::COLOR_BGR2BGRA);
  return copy_converted_frame_to_buffer(converted, buffer, "BGRx");
}

// Convert RGBx frame
bool convert_rgbx_frame(const cv::Mat& frame, pw_buffer* buffer) {
  cv::Mat converted;
  cv::cvtColor(frame, converted, cv::COLOR_BGR2RGBA);
  return copy_converted_frame_to_buffer(converted, buffer, "RGBx");
}

} // namespace segmecam