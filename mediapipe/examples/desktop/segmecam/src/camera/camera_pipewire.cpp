#include "include/camera/camera_manager.h"
#include "include/camera/gstreamer_buffer_utils.h"

#include <iostream>
#include <mutex>
#include <vector>
#include <algorithm>
#include <string>

// GStreamer types (forward declared to avoid header dependencies)
typedef struct _GstElement GstElement;
typedef struct _GstAppSink GstAppSink;
typedef struct _GstSample GstSample;
typedef struct _GstBuffer GstBuffer;
typedef struct _GstCaps GstCaps;
typedef struct _GstMapInfo GstMapInfo;
typedef struct _GstBin GstBin;

// GStreamer functions (loaded dynamically)
extern void* (*gst_parse_launch)(const char*, void**);
extern void* (*gst_bin_get_by_name)(GstBin*, const char*);

// GLib functions
extern void (*g_object_set)(void*, const char*, ...);
extern void (*g_signal_connect)(void*, const char*, void*, void*);

// GStreamer state constants (defined as macros in camera_manager.h)
#define GST_STATE_NULL 0
#define GST_STATE_CHANGE_FAILURE -1
#define GST_STATE_PLAYING 4

// GLib constants
const int TRUE = 1;
const int FALSE = 0;

// Macros
#define G_CALLBACK(f) ((void*)(f))

namespace segmecam {

bool CameraManager::CreatePipelineFromDescription(const char* pipeline_desc) {
    void* error = nullptr;
    pipeline_ = gst_parse_launch(pipeline_desc, &error);
    if (!pipeline_) {
        std::cerr << "❌ Failed to create PipeWire pipeline via gst_parse_launch" << std::endl;
        if (error && g_error_free) {
            std::cerr << "🔍 GStreamer error while constructing pipeline" << std::endl;
            g_error_free(error);
        }
        return false;
    }
    return true;
}

bool CameraManager::RetrievePipelineElements() {
    pipewire_src_ = gst_bin_get_by_name(reinterpret_cast<GstBin*>(pipeline_), "source");
    GstElement* sink_element = gst_bin_get_by_name(reinterpret_cast<GstBin*>(pipeline_), "sink");
    
    if (!pipewire_src_) {
        std::cerr << "❌ PipeWire pipeline missing source" << std::endl;
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        if (sink_element) {
            gst_object_unref(sink_element);
        }
        return false;
    }
    
    if (!sink_element) {
        std::cerr << "❌ PipeWire pipeline missing appsink" << std::endl;
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        gst_object_unref(pipewire_src_);
        pipewire_src_ = nullptr;
        return false;
    }

    appsink_ = sink_element;
    gst_appsink_ = reinterpret_cast<GstAppSink*>(sink_element);
    return true;
}

void CameraManager::ConfigureAppSink() {
    g_object_set(appsink_, "emit-signals", TRUE,
                 "sync", FALSE,
                 "max-buffers", 1,
                 "drop", TRUE,
                 nullptr);
}

void CameraManager::ConnectPipelineSignals() {
    g_signal_connect(appsink_, "new-sample", reinterpret_cast<void*>(OnNewSampleWrapper), this);
    g_signal_connect(appsink_, "eos", reinterpret_cast<void*>(OnEOSWrapper), this);
}

bool CameraManager::CreatePipeWirePipeline(int /*width*/, int /*height*/, int /*fps*/) {
    std::cout << "🎬 Creating PipeWire GStreamer pipeline..." << std::endl;

    if (!gst_parse_launch || !gst_bin_get_by_name) {
        std::cerr << "❌ GStreamer helper functions unavailable" << std::endl;
        return false;
    }

    const char* pipeline_desc =
        "pipewiresrc name=source do-timestamp=true ! videoconvert ! "
        "video/x-raw,format=BGR ! appsink name=sink emit-signals=true sync=false max-buffers=1 drop=true";

    if (!CreatePipelineFromDescription(pipeline_desc)) {
        return false;
    }

    if (!RetrievePipelineElements()) {
        return false;
    }

    ConfigureAppSink();
    ConnectPipelineSignals();

    std::cout << "✅ PipeWire pipeline created successfully" << std::endl;
    return true;
}

bool CameraManager::EnsureCameraPermission() {
    if (!camera_permission_granted_) {
        if (!RequestCameraPermission()) {
            return false;
        }
    }
    return true;
}

void CameraManager::CalculateTargetDimensions(int width, int height, int fps, int& target_width, int& target_height, int& target_fps) {
    target_width = width > 0 ? width : (state_.current_width > 0 ? state_.current_width : 640);
    target_height = height > 0 ? height : (state_.current_height > 0 ? state_.current_height : 480);
    target_fps = fps > 0 ? fps : (state_.current_fps > 0 ? state_.current_fps : 30);
}

bool CameraManager::ValidatePortalConnection() {
    if (portal_fd_ < 0) {
        std::cerr << "❌ Portal PipeWire remote unavailable" << std::endl;
        return false;
    }
    return true;
}

void CameraManager::CleanupExistingPipeline() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        appsink_ = nullptr;
        gst_appsink_ = nullptr;
        if (pipewire_src_) {
            gst_object_unref(pipewire_src_);
            pipewire_src_ = nullptr;
        }
    }
}

bool CameraManager::SetupAndStartPipeline(int target_width, int target_height, int target_fps) {
    if (!CreatePipeWirePipeline(target_width, target_height, target_fps)) {
        return false;
    }

    std::cout << "🎬 Starting PipeWire camera capture..." << std::endl;

    // Set the PipeWire remote fd on the source element
    if (pipewire_src_ && portal_fd_ >= 0) {
        g_object_set(pipewire_src_, "fd", portal_fd_, NULL);
    }

    int ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "❌ Failed to start pipeline" << std::endl;
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        appsink_ = nullptr;
        gst_appsink_ = nullptr;
        if (pipewire_src_) {
            gst_object_unref(pipewire_src_);
            pipewire_src_ = nullptr;
        }
        return false;
    }
    return true;
}

void CameraManager::UpdateCameraState(int target_width, int target_height, int target_fps) {
    state_.current_width = target_width;
    state_.current_height = target_height;
    state_.current_fps = target_fps;
    state_.actual_fps = target_fps;
    state_.backend_name = "GStreamer (PipeWire)";
    state_.is_opened = true;
    std::cout << "✅ PipeWire camera capture started" << std::endl;
}

bool CameraManager::StartPipeWireCapture(int width, int height, int fps) {
    if (!EnsureCameraPermission()) {
        return false;
    }

    int target_width, target_height, target_fps;
    CalculateTargetDimensions(width, height, fps, target_width, target_height, target_fps);

    if (!ValidatePortalConnection()) {
        return false;
    }

    CleanupExistingPipeline();

    if (!SetupAndStartPipeline(target_width, target_height, target_fps)) {
        return false;
    }

    UpdateCameraState(target_width, target_height, target_fps);
    return true;
}

void CameraManager::StopPipeWireCapture() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    appsink_ = nullptr;
    gst_appsink_ = nullptr;
    if (pipewire_src_) {
        gst_object_unref(pipewire_src_);
        pipewire_src_ = nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        current_frame_.release();
        frame_ready_ = false;
        frame_ready_cv_.notify_all();
    }
    if (portal_fd_ >= 0) {
        close(portal_fd_);
        portal_fd_ = -1;
    }
    gst_camera_active_ = false;
    state_.is_opened = false;
    std::cout << "🛑 PipeWire camera capture stopped" << std::endl;
}

// Enumerate PipeWire camera nodes using pw-cli
std::vector<int> CameraManager::EnumeratePipeWireCameraNodes() {
    std::vector<int> camera_nodes;

    // SECURITY NOTE: This popen call is considered safe because:
    // 1. The command is hardcoded and cannot be influenced by user input
    // 2. It only executes read-only operations (pw-cli ls)
    // 3. Output is validated before use (std::stoi with exception handling)
    // 4. It's only executed in Flatpak environments where PipeWire is available
    FILE* pipe = popen("pw-cli ls Node 2>/dev/null | grep -E 'node\\.name.*camera|node\\.name.*webcam|node\\.name.*video' | grep -o 'id [0-9]*' | cut -d' ' -f2", "r");
    if (!pipe) {
        std::cerr << "⚠️  Failed to run pw-cli for PipeWire node enumeration" << std::endl;
        return camera_nodes;
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        try {
            // Use std::stoi instead of atoi for better error handling
            int node_id = std::stoi(std::string(buffer));
            if (node_id > 0) {
                camera_nodes.push_back(node_id);
                std::cout << "📷 Found PipeWire camera node: " << node_id << std::endl;
            }
        } catch (const std::exception&) {
            // Skip invalid lines that can't be parsed as integers
            continue;
        }
    }

    pclose(pipe);

    // Sort by node ID
    std::sort(camera_nodes.begin(), camera_nodes.end());

    return camera_nodes;
}

// Get PipeWire node ID for a given camera index
int CameraManager::GetPipeWireNodeIdForCamera(int camera_index) {
    static std::vector<int> cached_nodes;
    static bool nodes_enumerated = false;

    if (!nodes_enumerated) {
        cached_nodes = EnumeratePipeWireCameraNodes();
        nodes_enumerated = true;

        if (cached_nodes.empty()) {
            std::cerr << "⚠️  No PipeWire camera nodes found, cannot enumerate cameras" << std::endl;
            return -1;
        }
    }

    if (camera_index >= 0 && camera_index < static_cast<int>(cached_nodes.size())) {
        return cached_nodes[camera_index];
    }

    std::cerr << "⚠️  Camera index " << camera_index << " out of range (found " << cached_nodes.size() << " camera nodes)" << std::endl;
    return -1;
}

} // namespace segmecam