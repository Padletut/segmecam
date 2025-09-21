#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <unordered_map>
#include <opencv2/opencv.hpp>
struct BufferInfo {
    void* data;
    size_t size;
};

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

} // namespace segmecam