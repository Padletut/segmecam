#include "include/camera/camera_manager.h"
#include "include/camera/gstreamer_buffer_utils.h"

#include <cstdlib>  // NOLINT - Standard library header resolved by build system
#include <cstring>  // NOLINT - Standard library header resolved by build system
#include <unistd.h> // NOLINT - Standard library header resolved by build system
#include <algorithm> // NOLINT - Standard library header resolved by build system
#include <cctype> // NOLINT - Standard library header resolved by build system
#include <chrono> // NOLINT - Standard library header resolved by build system
#include <iostream> // NOLINT - Standard library header resolved by build system
#include <string>   // NOLINT - Standard library header resolved by build system
#include <utility> // NOLINT - Standard library header resolved by build system

// Runtime detection of Flatpak environment
static bool IsRunningInFlatpak() {
    const char* flatpak_id = std::getenv("FLATPAK_ID");
    // FLATPAK_ID is set by Flatpak runtime, presence indicates sandboxed environment
    // This is a safe check as we only verify existence, not use the value
    return flatpak_id != nullptr && flatpak_id[0] != '\0';
}

// PipeWire/GStreamer support (loaded at runtime when in Flatpak)
#ifdef __cplusplus
extern "C" {
#endif
// GStreamer types (forward declared to avoid header dependencies)
typedef struct _GstElement GstElement;
typedef struct _GstAppSink GstAppSink;
typedef struct _GstSample GstSample;
typedef struct _GstBuffer GstBuffer;
typedef struct _GstCaps GstCaps;
typedef struct _GstMapInfo GstMapInfo;
GMainLoop* g_main_loop_new(gpointer context, gboolean is_running);
void g_main_loop_run(GMainLoop* loop);
void g_main_loop_quit(GMainLoop* loop);
void g_main_loop_unref(GMainLoop* loop);
void g_error_free(GError* error);
void g_object_unref(gpointer object);

// GStreamer functions (loaded dynamically)
typedef void (*gst_init_func)(int*, char***);
static gst_init_func gst_init = nullptr;
static GstElement* (*gst_pipeline_new)(const char*) = nullptr;
static GstElement* (*gst_element_factory_make)(const char*, const char*) = nullptr;
static int (*gst_element_set_state)(GstElement*, int) = nullptr;
static int (*gst_element_get_state)(GstElement*, int*, int*, uint64_t) = nullptr;
static int (*gst_bin_add_many)(void*, ...) = nullptr;
static int (*gst_element_link_many)(void*, ...) = nullptr;
static void (*gst_object_unref)(void*) = nullptr;
static void* (*gst_app_sink_pull_sample)(GstAppSink*) = nullptr;
static int (*gst_app_sink_is_eos)(GstAppSink*) = nullptr;
static void* (*gst_sample_get_buffer)(GstSample*) = nullptr;
static void* (*gst_sample_get_caps)(GstSample*) = nullptr;
static int (*gst_buffer_map)(GstBuffer*, GstMapInfo*, int) = nullptr;
static void (*gst_buffer_unmap)(GstBuffer*, GstMapInfo*) = nullptr;
static void (*gst_sample_unref)(GstSample*) = nullptr;

const int TRUE = 1;
const int FALSE = 0;
const uint64_t GST_CLOCK_TIME_NONE = (uint64_t)-1;

// Macros for GStreamer/GLib
#define G_CALLBACK(f) ((void*)(f))
#define GST_BIN(obj) ((void*)(obj))
#ifdef __cplusplus
}
#endif
typedef GstElement* (*gst_element_factory_make_func)(const char*, const char*);

// libportal for camera permissions (runtime loaded to avoid Bazel issues)
#include <dlfcn.h>

namespace segmecam {

// GStreamer direct capture methods for Flatpak
bool CameraManager::OpenGStreamerCamera(int camera_index, int width, int height, int fps) {
    std::cout << "🎬 Opening camera " << camera_index << " with direct GStreamer pipeline..." << std::endl;

    if (!gst_initialized_) {
        std::cerr << "❌ GStreamer not initialized" << std::endl;
        return false;
    }

    // Close any existing GStreamer camera
    CloseGStreamerCamera();

    // In Flatpak with --device=all, try PipeWire access first since cameras are accessible via PipeWire
    if (IsRunningInFlatpak()) {
        if (TryOpenPipeWireCamera(camera_index, width, height, fps)) {
            return true;
        }

        // PipeWire failed, fall back to V4L2
        if (TryOpenV4L2Camera(camera_index, width, height, fps)) {
            return true;
        }
    }

    std::cout << "❌ All GStreamer camera opening attempts failed" << std::endl;
    return false;

    std::cout << "❌ All GStreamer camera opening attempts failed" << std::endl;
    return false;
}

// PipeWire camera helper methods
std::string CameraManager::CreatePipeWirePipelineString(int pipewire_node_id) {
    char pipeline_str[256];
    snprintf(pipeline_str, sizeof(pipeline_str),
             "pipewiresrc path=%d ! videoconvert ! video/x-raw,format=BGR ! appsink name=sink",
             pipewire_node_id);
    return std::string(pipeline_str);
}

bool CameraManager::CreateAndValidatePipeWirePipeline(const std::string& pipeline_str) {
    std::cout << "🎬 Trying PipeWire pipeline: " << pipeline_str << std::endl;

    void* pw_error = nullptr;
    gst_pipeline_ = gst_parse_launch(pipeline_str.c_str(), &pw_error);

    if (!gst_pipeline_) {
        std::cout << "⚠️  PipeWire pipeline creation failed" << std::endl;
        if (pw_error) {
            std::cout << "🔍 PipeWire pipeline error: " << static_cast<char*>(pw_error) << std::endl;
            g_error_free(pw_error);
        }
        return false;
    }

    std::cout << "✅ PipeWire pipeline created successfully" << std::endl;
    return true;
}

bool CameraManager::ConfigurePipeWireAppSink() {
    // Get the appsink element
    gst_appsink_ = reinterpret_cast<GstAppSink*>(gst_bin_get_by_name(reinterpret_cast<GstBin*>(gst_pipeline_), "sink"));
    if (!gst_appsink_) {
        std::cout << "⚠️  Failed to get appsink from PipeWire pipeline" << std::endl;
        CleanupPipeline();
        return false;
    }

    std::cout << "✅ GStreamer PipeWire pipeline created, configuring appsink..." << std::endl;

    // Configure appsink
    g_object_set(gst_appsink_, "emit-signals", TRUE, "sync", FALSE, NULL);
    return true;
}

bool CameraManager::StartAndValidatePipeWirePipeline() {
    // Set pipeline to playing state
    std::cout << "✅ Appsink configured, setting pipeline to playing state..." << std::endl;
    GstStateChangeReturn ret = static_cast<GstStateChangeReturn>(gst_element_set_state(gst_pipeline_, GST_STATE_PLAYING));

    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cout << "⚠️  PipeWire pipeline failed to start" << std::endl;
        CleanupPipeline();
        return false;
    }

    // Wait for pipeline to stabilize
    std::cout << "✅ Pipeline state set to playing, waiting for stabilization..." << std::endl;
    g_usleep(500000); // 500ms for PipeWire

    // Check final state
    std::cout << "✅ Pipeline stabilized, checking state..." << std::endl;
    int state, pending;
    ret = static_cast<GstStateChangeReturn>(gst_element_get_state(gst_pipeline_, &state, &pending, GST_CLOCK_TIME_NONE));

    if (state != GST_STATE_PLAYING) {
        std::cout << "⚠️  PipeWire pipeline not in playing state" << std::endl;
        CleanupPipeline();
        return false;
    }

    return true;
}

// Helper method to try opening camera with PipeWire pipeline
bool CameraManager::TryOpenPipeWireCamera(int camera_index, int width, int height, int fps) {
    std::cout << "📷 Flatpak detected - trying PipeWire camera access" << std::endl;

    // Get the correct PipeWire node ID by enumerating available camera nodes
    int pipewire_node_id = CameraManager::GetPipeWireNodeIdForCamera(camera_index);
    if (pipewire_node_id < 0) {
        std::cout << "⚠️  No PipeWire camera node found for index " << camera_index << std::endl;
        return false;
    }

    // Create and validate PipeWire pipeline
    std::string pipeline_str = CreatePipeWirePipelineString(pipewire_node_id);
    if (!CreateAndValidatePipeWirePipeline(pipeline_str)) {
        return false;
    }

    // Configure appsink
    if (!ConfigurePipeWireAppSink()) {
        return false;
    }

    // Start and validate pipeline
    if (!StartAndValidatePipeWirePipeline()) {
        return false;
    }

    std::cout << "✅ PipeWire GStreamer pipeline ready for capture" << std::endl;
    gst_camera_active_ = true;

    // Set state values
    SetCameraState(width, height, fps, "GStreamer (PipeWire)");
    return true;
}

// Helper method to try opening camera with V4L2 pipeline
bool CameraManager::TryOpenV4L2Camera(int camera_index, int width, int height, int fps) {
    std::cout << "📷 Trying V4L2 camera access as fallback" << std::endl;

    // Create V4L2 pipeline: v4l2src device=/dev/video0 ! videoconvert ! video/x-raw,format=BGR ! appsink name=sink
    char v4l2_pipeline_str[256];
    snprintf(v4l2_pipeline_str, sizeof(v4l2_pipeline_str),
             "v4l2src device=/dev/video%d ! videoconvert ! video/x-raw,format=BGR ! appsink name=sink",
             camera_index);

    std::cout << "🎬 Trying V4L2 pipeline: " << v4l2_pipeline_str << std::endl;

    void* error = nullptr;
    gst_pipeline_ = gst_parse_launch(v4l2_pipeline_str, &error);

    if (!gst_pipeline_) {
        std::cout << "⚠️  V4L2 pipeline creation failed" << std::endl;
        if (error) {
            g_error_free(error);
        }
        return false;
    }

    std::cout << "✅ V4L2 pipeline created successfully" << std::endl;

    // Get the appsink element
    gst_appsink_ = reinterpret_cast<GstAppSink*>(gst_bin_get_by_name(reinterpret_cast<GstBin*>(gst_pipeline_), "sink"));
    if (!gst_appsink_) {
        std::cout << "⚠️  Failed to get appsink from V4L2 pipeline" << std::endl;
        CleanupPipeline();
        return false;
    }

    std::cout << "✅ GStreamer V4L2 pipeline created, configuring appsink..." << std::endl;

    // Configure appsink
    g_object_set(gst_appsink_, "emit-signals", TRUE, "sync", FALSE, NULL);

    // Set pipeline to playing state
    std::cout << "✅ Appsink configured, setting pipeline to playing state..." << std::endl;
    GstStateChangeReturn ret = static_cast<GstStateChangeReturn>(gst_element_set_state(gst_pipeline_, GST_STATE_PLAYING));

    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cout << "⚠️  V4L2 pipeline failed to start" << std::endl;
        CleanupPipeline();
        return false;
    }

    // Wait for pipeline to stabilize
    std::cout << "✅ Pipeline state set to playing, waiting for stabilization..." << std::endl;
    g_usleep(200000); // 200ms

    // Check final state
    std::cout << "✅ Pipeline stabilized, checking state..." << std::endl;
    int state, pending;
    ret = static_cast<GstStateChangeReturn>(gst_element_get_state(gst_pipeline_, &state, &pending, GST_CLOCK_TIME_NONE));

    if (state != GST_STATE_PLAYING) {
        std::cout << "⚠️  V4L2 pipeline not in playing state" << std::endl;
        CleanupPipeline();
        return false;
    }

    std::cout << "✅ V4L2 GStreamer pipeline ready for capture" << std::endl;
    gst_camera_active_ = true;

    // Set state values
    SetCameraState(width, height, fps, "GStreamer (V4L2)");
    return true;
}

// Helper method to clean up pipeline resources
void CameraManager::CleanupPipeline() {
    if (gst_pipeline_) {
        gst_element_set_state(gst_pipeline_, GST_STATE_NULL);
        g_usleep(100000);
        gst_object_unref(gst_pipeline_);
        gst_pipeline_ = nullptr;
    }
    gst_appsink_ = nullptr;
}

// Helper method to set camera state after successful opening
void CameraManager::SetCameraState(int width, int height, int fps, const std::string& backend_name) {
    state_.current_width = width;
    state_.current_height = height;
    state_.current_fps = fps;
    state_.actual_fps = fps;
    state_.backend_name = backend_name;
    state_.is_opened = true;
}

} // namespace segmecam