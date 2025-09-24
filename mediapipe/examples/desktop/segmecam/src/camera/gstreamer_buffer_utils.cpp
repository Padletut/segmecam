// cppcheck-suppress missingInclude
#include <algorithm>      // NOLINT - Standard library header resolved by build system
#include <cctype>          // NOLINT - Standard library header resolved by build system
#include <iostream>        // NOLINT - Standard library header resolved by build system
#include <string>          // NOLINT - Standard library header resolved by build system
#include <unordered_map>   // NOLINT - Standard library header resolved by build system
#include <opencv2/opencv.hpp>  // NOLINT - OpenCV header resolved by Bazel/MediaPipe

#include "include/camera/gstreamer_buffer_utils.h"

namespace segmecam {

// Helper function to convert string to uppercase
std::string ToUpperCopy(const std::string& value) {
    std::string upper = value;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return upper;
}

// Helper function to determine and validate the pixel format
std::string DetermineFormat(const std::string& format_hint, size_t buffer_size, int width, int height) {
    std::string fmt_upper = ToUpperCopy(format_hint);
    if (fmt_upper.empty()) {
        fmt_upper = "BGR";
    }

    const std::string known_formats[] = {
        "BGR", "RGB", "BGRX", "BGRA", "RGBX", "RGBA", "YUY2", "UYVY"
    };

    bool recognized = false;
    for (const auto& candidate : known_formats) {
        if (fmt_upper == candidate) {
            recognized = true;
            break;
        }
    }

    if (!recognized) {
        std::cerr << "⚠️  Unrecognized GStreamer format '" << format_hint
                  << "', applying heuristic conversion" << std::endl;
        const size_t total_pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
        if (total_pixels > 0) {
            size_t bytes_per_pixel_guess = buffer_size / total_pixels;
            if (bytes_per_pixel_guess == 4) {
                fmt_upper = "BGRX";
            } else if (bytes_per_pixel_guess == 2) {
                fmt_upper = "YUY2";
            } else {
                fmt_upper = "BGR";
            }
        } else {
            fmt_upper = "BGR";
        }
    }

    return fmt_upper;
}

// Helper function to get format-specific parameters
struct FormatParameters {
    size_t bytes_per_pixel;
    int cv_type;
    int conversion_code;
};

FormatParameters GetFormatParameters(const std::string& format) {
    // Use a lookup table to reduce cyclomatic complexity
    static const std::unordered_map<std::string, FormatParameters> format_map = {
        {"BGR", {3, CV_8UC3, -1}},
        {"RGB", {3, CV_8UC3, cv::COLOR_RGB2BGR}},
        {"BGRX", {4, CV_8UC4, cv::COLOR_BGRA2BGR}},
        {"BGRA", {4, CV_8UC4, cv::COLOR_BGRA2BGR}},
        {"RGBX", {4, CV_8UC4, cv::COLOR_RGBA2BGR}},
        {"RGBA", {4, CV_8UC4, cv::COLOR_RGBA2BGR}},
        {"YUY2", {2, CV_8UC2, cv::COLOR_YUV2BGR_YUY2}},
        {"UYVY", {2, CV_8UC2, cv::COLOR_YUV2BGR_UYVY}}
    };

    auto it = format_map.find(format);
    if (it != format_map.end()) {
        return it->second;
    }

    // Default to BGR if format not found
    return {3, CV_8UC3, -1};
}

// Helper function to calculate the appropriate stride
size_t CalculateStride(int width, size_t bytes_per_pixel, int stride_hint, size_t buffer_size, int height) {
    size_t min_stride = static_cast<size_t>(width) * bytes_per_pixel;
    size_t stride = stride_hint > 0 ? static_cast<size_t>(stride_hint) : min_stride;
    if (stride < min_stride) {
        stride = min_stride;
    }

    if (height > 0) {
        size_t candidate_stride = buffer_size / static_cast<size_t>(height);
        if (stride_hint <= 0 && candidate_stride >= min_stride && candidate_stride % bytes_per_pixel == 0) {
            stride = candidate_stride;
        }
    }

    return stride;
}

// Helper function to validate buffer size requirements
bool ValidateBufferSize(size_t buffer_size, int width, int height, size_t bytes_per_pixel, const std::string& format) {
    size_t min_stride = static_cast<size_t>(width) * bytes_per_pixel;
    size_t min_bytes = min_stride * static_cast<size_t>(height);

    if (buffer_size < min_bytes) {
        std::cerr << "⚠️  GStreamer buffer too small for format " << format
                  << ": received " << buffer_size << " bytes, expected at least "
                  << min_bytes << std::endl;
        return false;
    }
    return true;
}

// Convert GStreamer buffer to OpenCV BGR format
bool ConvertBufferToBgr(const BufferInfo& buffer_info,
                        int width,
                        int height,
                        const std::string& format_hint,
                        int stride_hint,
                        cv::Mat& output) {
    if (!buffer_info.data || width <= 0 || height <= 0) {
        return false;
    }

    std::string format = DetermineFormat(format_hint, buffer_info.size, width, height);
    FormatParameters params = GetFormatParameters(format);
    size_t stride = CalculateStride(width, params.bytes_per_pixel, stride_hint, buffer_info.size, height);

    if (!ValidateBufferSize(buffer_info.size, width, height, params.bytes_per_pixel, format)) {
        return false;
    }

    cv::Mat wrapped(height, width, params.cv_type, buffer_info.data, stride);
    if (params.conversion_code >= 0) {
        cv::Mat converted;
        cv::cvtColor(wrapped, converted, params.conversion_code);
        output = converted;
    } else {
        output = wrapped.clone();
    }
    return true;
}


// Convert BGR (cv::Mat) to YUY2 (packed, width*height*2 bytes)
// Output buffer must be preallocated to width*height*2 bytes
void BGRToYUY2(const cv::Mat& bgr, uint8_t* yuy2_out) {
    const int W = bgr.cols, H = bgr.rows;
    const uint8_t* p = bgr.data; int stride = (int)bgr.step; uint8_t* o = yuy2_out;
    auto clamp8 = [](int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); };
    for (int y=0; y<H; ++y) {
        const uint8_t* row = p + y * stride;
        for (int x=0; x<W; x+=2) {
            int b0=row[x*3+0], g0=row[x*3+1], r0=row[x*3+2];
            int b1=row[(x+1)*3+0], g1=row[(x+1)*3+1], r1=row[(x+1)*3+2];
            int Y0 = ( 66*r0 +129*g0 + 25*b0 +128)>>8; Y0 += 16;
            int Y1 = ( 66*r1 +129*g1 + 25*b1 +128)>>8; Y1 += 16;
            int U  = (-38*r0 - 74*g0 +112*b0 +128)>>8; U += 128;
            int V  = (112*r0 - 94*g0 - 18*b0 +128)>>8; V += 128;
            int U1 = (-38*r1 - 74*g1 +112*b1 +128)>>8; U1 += 128;
            int V1 = (112*r1 - 94*g1 - 18*b1 +128)>>8; V1 += 128;
            U = (U + U1) >> 1; V = (V + V1) >> 1;
            *o++ = clamp8(Y0); *o++ = clamp8(U); *o++ = clamp8(Y1); *o++ = clamp8(V);
        }
    }
}

} // namespace segmecam