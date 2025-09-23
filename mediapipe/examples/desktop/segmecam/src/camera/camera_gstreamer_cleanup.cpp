#include "include/camera/camera_manager.h"
#include <iostream>

namespace segmecam {

void CameraManager::CleanupGStreamer() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
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
}

} // namespace segmecam