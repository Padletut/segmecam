#include "include/camera/camera_manager.h"
#include <iostream>

namespace segmecam {

void CameraManager::CleanupGStreamer() {
    std::cout << "🧹 Cleaning up GStreamer resources..." << std::endl;
    
    // Clean up PipeWire pipeline
    if (pipeline_) {
        std::cout << "🧹 Setting PipeWire pipeline to NULL state..." << std::endl;
        
        // Always set to NULL state to ensure clean disposal
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        std::cout << "✅ PipeWire pipeline cleaned up" << std::endl;
    }

    // Clean up direct GStreamer pipeline
    if (gst_pipeline_) {
        std::cout << "🧹 Setting direct GStreamer pipeline to NULL state..." << std::endl;
        gst_element_set_state(gst_pipeline_, GST_STATE_NULL);
        gst_object_unref(gst_pipeline_);
        gst_pipeline_ = nullptr;
        std::cout << "✅ Direct GStreamer pipeline cleaned up" << std::endl;
    }

    if (gst_appsink_) {
        gst_object_unref(GST_ELEMENT(gst_appsink_));
        gst_appsink_ = nullptr;
    }

    if (appsink_) {
        gst_object_unref(appsink_);
        appsink_ = nullptr;
    }

    if (pipewire_src_) {
        gst_object_unref(pipewire_src_);
        pipewire_src_ = nullptr;
    }

    if (main_loop_) {
        g_main_loop_quit(main_loop_);
        g_main_loop_unref(main_loop_);
        main_loop_ = nullptr;
    }

    gst_initialized_ = false;
    camera_permission_granted_ = false;
    std::cout << "✅ GStreamer cleanup completed" << std::endl;
}

} // namespace segmecam