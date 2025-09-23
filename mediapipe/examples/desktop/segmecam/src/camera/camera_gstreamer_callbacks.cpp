#include "include/camera/camera_manager.h"
#include "include/camera/gstreamer_utils.h"

#include <iostream>

namespace segmecam {

// Frame processing callbacks for GStreamer
void CameraManager::OnNewSample(GstAppSink* sink) {
    if (!gst_app_sink_pull_sample) {
        return;
    }

    GstSample* sample = static_cast<GstSample*>(gst_app_sink_pull_sample(sink));
    if (!sample) {
        return;
    }

    static int sample_debug = 0;
    if (sample_debug < 10) {
        std::cout << "✅ PipeWire sample received (count=" << sample_debug + 1 << ")" << std::endl;
        sample_debug++;
    }

    int width = state_.current_width > 0 ? state_.current_width : 640;
    int height = state_.current_height > 0 ? state_.current_height : 480;
    cv::Mat frame;

    if (!ConvertSampleToBgr(sample, frame, width, height)) {
        static int convert_fail_log = 0;
        if (convert_fail_log < 5) {
            std::cout << "❌ OnNewSample failed to convert sample" << std::endl;
            convert_fail_log++;
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        current_frame_ = std::move(frame);
        state_.current_width = width;
        state_.current_height = height;
        state_.backend_name = "GStreamer (PipeWire)";
        state_.is_opened = true;
        state_.frames_captured++;
        frame_ready_ = true;
    }
    frame_ready_cv_.notify_all();
}

void CameraManager::OnEOS(GstAppSink* sink) {
    std::cout << "🎬 PipeWire stream ended" << std::endl;
    state_.is_opened = false;
}

// Static wrapper functions for GStreamer callbacks
void CameraManager::OnNewSampleWrapper(GstAppSink* sink, gpointer user_data) {
    CameraManager* self = static_cast<CameraManager*>(user_data);
    self->OnNewSample(sink);
}

void CameraManager::OnEOSWrapper(GstAppSink* sink, gpointer user_data) {
    CameraManager* self = static_cast<CameraManager*>(user_data);
    self->OnEOS(sink);
}

void CameraManager::OnPortalCameraAccessFinished(GObject* /*source*/, GAsyncResult* result, gpointer user_data) {
    auto* ctx = static_cast<PortalRequestContext*>(user_data);
    if (!ctx) return;

    if (!ctx->self) {
        // If context self is null, we can't do anything meaningful
        return;
    }

    CameraManager* self = ctx->self;
    bool granted = false;

    if (self->ProcessPortalAccessResult(result, granted) && granted) {
        self->OpenPipeWireRemote(granted);
    }

    ctx->success = granted;
    self->CleanupPortalRequestContext(ctx);
}

} // namespace segmecam