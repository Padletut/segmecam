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

} // namespace segmecam