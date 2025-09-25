#pragma once

#include <string>
#include <opencv2/opencv.hpp>

// Minimal GStreamer buffer info structure
struct BufferInfo {
    void* data;
    size_t size;
};

namespace segmecam {

// Clamp integer value to uint8_t range [0, 255]
inline uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

// Convert string to uppercase copy
std::string ToUpperCopy(const std::string& value);

// Convert GStreamer buffer to OpenCV BGR format
bool ConvertBufferToBgr(const BufferInfo& buffer_info,
                        int width,
                        int height,
                        const std::string& format_hint,
                        int stride_hint,
                        cv::Mat& output);

// Convert BGR (cv::Mat) to YUY2 (packed, width*height*2 bytes)
// Output buffer must be preallocated to width*height*2 bytes
void BGRToYUY2(const cv::Mat& bgr, uint8_t* yuy2_out);

} // namespace segmecam