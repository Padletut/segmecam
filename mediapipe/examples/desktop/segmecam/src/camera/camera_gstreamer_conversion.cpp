#include "include/camera/camera_manager.h"
#include "include/camera/gstreamer_utils.h"
#include "include/camera/gstreamer_buffer_utils.h"

#include <iostream>

namespace segmecam {

// ConvertSampleToBgr helper methods
bool CameraManager::ParseCapsStructure(GstCaps* caps, int& width, int& height, int& stride_hint, std::string& format) {
    GstStructure* structure = nullptr;
    if (!GetStructureFromCaps(caps, structure)) {
        return false;
    }

    // Get width, height, stride, and format using helper methods
    ExtractIntFromStructure(structure, "width", width);
    ExtractIntFromStructure(structure, "height", height);
    ExtractIntFromStructure(structure, "stride", stride_hint);
    ExtractStringFromStructure(structure, "format", format);

    return true;
}

// ParseCapsStructure helper methods
bool CameraManager::GetStructureFromCaps(GstCaps* caps, GstStructure*& structure) {
    if (!gst_caps_get_structure) {
        return false;
    }

    structure = gst_caps_get_structure(caps, 0);
    return structure != nullptr;
}

bool CameraManager::ExtractIntFromStructure(GstStructure* structure, const char* field_name, int& value) {
    if (!gst_structure_get_int) {
        return false;
    }

    int extracted_value = 0;
    if (gst_structure_get_int(structure, field_name, &extracted_value) && extracted_value > 0) {
        value = extracted_value;
        return true;
    }
    return false;
}

bool CameraManager::ExtractStringFromStructure(GstStructure* structure, const char* field_name, std::string& value) {
    if (!gst_structure_get_string) {
        return false;
    }

    const char* extracted_value = gst_structure_get_string(structure, field_name);
    if (extracted_value && *extracted_value) {
        value = extracted_value;
        return true;
    }
    return false;
}

bool CameraManager::MapAndValidateBuffer(GstBuffer* buffer, GstMapInfo& map_info) {
    if (!gst_buffer_map) {
        return false;
    }

    return gst_buffer_map(buffer, &map_info, GST_MAP_READ);
}

bool CameraManager::ConvertBufferToBgrWithValidation(const BufferInfo& buffer_info, int width, int height,
                                                    const std::string& format, int stride_hint, cv::Mat& output) {
    return segmecam::ConvertBufferToBgr(buffer_info, width, height, format, stride_hint, output);
}

bool CameraManager::ValidateGStreamerFunctions() {
    return gst_sample_get_buffer && gst_sample_get_caps && gst_buffer_map &&
           gst_buffer_unmap && gst_sample_unref;
}

bool CameraManager::ExtractSampleComponents(GstSample* sample, GstBuffer*& buffer, GstCaps*& caps) {
    buffer = static_cast<GstBuffer*>(gst_sample_get_buffer(sample));
    caps = static_cast<GstCaps*>(gst_sample_get_caps(sample));
    return buffer && caps;
}

void CameraManager::InitializeConversionParameters(int& width, int& height, int& stride_hint, std::string& format,
                                                  int width_out, int height_out) {
    width = width_out > 0 ? width_out : 640;
    height = height_out > 0 ? height_out : 480;
    stride_hint = 0;
    format = "BGR";
}

void CameraManager::LogConversionResult(const std::string& format, int width, int height, int stride_hint,
                                        size_t map_size, bool success, int channels) {
    static int format_log_count = 0;
    if (format_log_count < 5) {
        std::cout << "📄 PipeWire sample format: caps_format=" << format
                  << " width=" << width << " height=" << height << " stride_hint=" << stride_hint
                  << " map_size=" << map_size << " success=" << std::boolalpha << success
                  << " channels=" << channels << std::endl;
        format_log_count++;
    }
}

bool CameraManager::PerformConversionAndCleanup(const GStreamerObjects& gst_objects, const ConversionInput& input,
                                               ConversionOutput& output) {
    GstMapInfo map_info = {};
    if (!MapAndValidateBuffer(gst_objects.buffer, map_info)) {
        gst_sample_unref(gst_objects.sample);
        return false;
    }

    cv::Mat converted;
    BufferInfo actual_buffer_info = {map_info.data, map_info.size};
    bool success = ConvertBufferToBgrWithValidation(actual_buffer_info, input.width, input.height, input.format, input.stride_hint, converted);

    LogConversionResult(input.format, input.width, input.height, input.stride_hint, map_info.size, success,
                       converted.empty() ? 0 : converted.channels());

    gst_buffer_unmap(gst_objects.buffer, &map_info);
    gst_sample_unref(gst_objects.sample);

    if (!success) {
        std::cerr << "⚠️  Failed to convert PipeWire sample to BGR (format=" << input.format
                  << ", width=" << input.width << ", height=" << input.height << ", stride_hint=" << input.stride_hint << ")" << std::endl;
        return false;
    }

    output.frame_out = std::move(converted);
    output.width_out = input.width;
    output.height_out = input.height;
    return true;
}

bool CameraManager::ConvertSampleToBgr(GstSample* sample, cv::Mat& frame_out, int& width_out, int& height_out) {
    if (!sample) {
        return false;
    }

    if (!ValidateGStreamerFunctions()) {
        if (gst_sample_unref) {
            gst_sample_unref(sample);
        }
        return false;
    }

    GstBuffer* buffer = nullptr;
    GstCaps* caps = nullptr;
    if (!ExtractSampleComponents(sample, buffer, caps)) {
        gst_sample_unref(sample);
        return false;
    }

    int width, height, stride_hint;
    std::string format;
    InitializeConversionParameters(width, height, stride_hint, format, width_out, height_out);

    if (!ParseCapsStructure(caps, width, height, stride_hint, format)) {
        gst_sample_unref(sample);
        return false;
    }

    if (width <= 0 || height <= 0) {
        gst_sample_unref(sample);
        return false;
    }

    BufferInfo buffer_info = {nullptr, 0};  // Will be set in PerformConversionAndCleanup

    GStreamerObjects gst_objects = {buffer, sample};
    ConversionInput input = {width, height, format, stride_hint};
    ConversionOutput output = {frame_out, width_out, height_out};

    return PerformConversionAndCleanup(gst_objects, input, output);
}

} // namespace segmecam