#include "include/camera/camera_manager.h"
#include "include/camera/gstreamer_buffer_utils.h"

#include <cstdlib>  // for getenv
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <string>   // for std::string, std::stoi
#include <utility>

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

bool CameraManager::CaptureGStreamerFrame(cv::Mat& frame) {
    if (!gst_pipeline_ || !gst_appsink_) {
        std::cout << "⚠️ GStreamer pipeline or appsink not initialized" << std::endl;
        return false;
    }

    // Check if EOS
    if (gst_app_sink_is_eos(gst_appsink_)) {
        std::cout << "⚠️ GStreamer EOS reached" << std::endl;
        return false;
    }

    // Debug: Check pipeline state
    int state, pending;
    GstStateChangeReturn ret = (GstStateChangeReturn)gst_element_get_state(gst_pipeline_, &state, &pending, GST_CLOCK_TIME_NONE);
    if (state != GST_STATE_PLAYING) {
        std::cout << "⚠️ GStreamer pipeline not in PLAYING state: " << state << std::endl;
        return false;
    }

    std::cout << "📸 Attempting to pull sample from appsink..." << std::endl;

    // Pull sample from appsink
    GstSample* sample = (GstSample*)gst_app_sink_pull_sample(gst_appsink_);
    if (!sample) {
        std::cout << "⚠️ gst_app_sink_pull_sample returned nullptr" << std::endl;
        return false;
    }

    std::cout << "✅ Got sample from appsink" << std::endl;

    int width = state_.current_width > 0 ? state_.current_width : 640;
    int height = state_.current_height > 0 ? state_.current_height : 480;
    cv::Mat captured_frame;

    if (!ConvertSampleToBgr(sample, captured_frame, width, height)) {
        return false;
    }

    frame = std::move(captured_frame);
    state_.current_width = width;
    state_.current_height = height;
    state_.frames_captured++;
    state_.is_opened = true;

    return true;
}

} // namespace segmecam