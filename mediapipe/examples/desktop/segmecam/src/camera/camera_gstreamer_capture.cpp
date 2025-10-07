#include "include/camera/camera_manager.h"
#include "include/camera/gstreamer_utils.h"
#include "include/camera/gstreamer_buffer_utils.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>

namespace segmecam {

bool CameraManager::CaptureGStreamerFrame(cv::Mat& frame) {
    // Validate pipeline state before capture
    if (!ValidateGStreamerPipeline()) {
        return false;
    }

    std::cout << "📸 Attempting to pull sample from appsink..." << std::endl;

    // Pull sample from appsink
    GstSample* sample = static_cast<GstSample*>(gst_app_sink_pull_sample(gst_appsink_));
    if (!sample) {
        std::cout << "⚠️ gst_app_sink_pull_sample returned nullptr" << std::endl;
        return false;
    }

    std::cout << "✅ Got sample from appsink" << std::endl;

    // Convert sample to BGR frame
    int width = state_.current_width > 0 ? state_.current_width : 640;
    int height = state_.current_height > 0 ? state_.current_height : 480;
    cv::Mat captured_frame;

    if (!this->ConvertSampleToBgr(sample, captured_frame, width, height)) {
        return false;
    }

    // Update state and return success
    frame = std::move(captured_frame);
    state_.current_width = width;
    state_.current_height = height;
    state_.frames_captured++;
    state_.is_opened = true;

    return true;
}

bool CameraManager::ValidateGStreamerPipeline() {
    if (!gst_pipeline_ || !gst_appsink_) {
        std::cout << "⚠️ GStreamer pipeline or appsink not initialized" << std::endl;
        return false;
    }

    // Check if EOS
    if (gst_app_sink_is_eos(gst_appsink_)) {
        std::cout << "⚠️ GStreamer EOS reached" << std::endl;
        return false;
    }

    // Check pipeline state
    int state, pending;
    gst_element_get_state(gst_pipeline_, &state, &pending, GST_CLOCK_TIME_NONE);
    if (state != GST_STATE_PLAYING) {
        std::cout << "⚠️ GStreamer pipeline not in PLAYING state: " << state << std::endl;
        return false;
    }

    return true;
}

} // namespace segmecam