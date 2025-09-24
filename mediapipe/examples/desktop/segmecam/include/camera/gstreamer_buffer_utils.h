#pragma once

#include <string>
#include <opencv2/opencv.hpp>

// Minimal GStreamer buffer info structure
struct BufferInfo {
    void* data;
    size_t size;
};

namespace segmecam {

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