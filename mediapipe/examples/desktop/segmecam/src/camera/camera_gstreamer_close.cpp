#include "include/camera/camera_manager.h"
#include "include/camera/gstreamer_utils.h"

#include <cstdlib>  // NOLINT - Standard library header resolved by build system
#include <cstring>  // NOLINT - Standard library header resolved by build system
#include <unistd.h> // NOLINT - Standard library header resolved by build system
#include <algorithm> // NOLINT - Standard library header resolved by build system
#include <cctype> // NOLINT - Standard library header resolved by build system
#include <chrono> // NOLINT - Standard library header resolved by build system
#include <iostream> // NOLINT - Standard library header resolved by build system
#include <string>   // NOLINT - Standard library header resolved by build system
#include <utility> // NOLINT - Standard library header resolved by build system

namespace segmecam {

void CameraManager::CloseGStreamerCamera() {
    std::cout << "🛑 GStreamer camera closed" << std::endl;

    if (gst_pipeline_) {
        // Set pipeline to NULL state first
        gst_element_set_state(gst_pipeline_, GST_STATE_NULL);
        g_usleep(100000); // Wait for state change

        // Unref the pipeline
        gst_object_unref(gst_pipeline_);
        gst_pipeline_ = nullptr;
    }

    gst_appsink_ = nullptr;
    gst_camera_active_ = false;
    state_.is_opened = false;
}

} // namespace segmecam