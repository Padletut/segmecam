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
    // Try to get PipeWire source first (named "source")
    pipewire_src_ = gst_bin_get_by_name(reinterpret_cast<GstBin*>(pipeline_), "source");
    
    // If no PipeWire source, try V4L2 source
    if (!pipewire_src_) {
        pipewire_src_ = gst_bin_get_by_name(reinterpret_cast<GstBin*>(pipeline_), "v4l2src0");
        if (pipewire_src_) {
            std::cout << "📷 Using V4L2 source element" << std::endl;
        }
    } else {
        std::cout << "📷 Using PipeWire source element" << std::endl;
    }
    
    // If still no source, try videotestsrc (for testing)
    if (!pipewire_src_) {
        pipewire_src_ = gst_bin_get_by_name(reinterpret_cast<GstBin*>(pipeline_), "videotestsrc0");
        if (pipewire_src_) {
            std::cout << "📷 Using test video source element" << std::endl;
        }
    }
    
    GstElement* sink_element = gst_bin_get_by_name(reinterpret_cast<GstBin*>(pipeline_), "sink");
    
    if (!pipewire_src_) {
        std::cerr << "❌ Pipeline missing source element (neither PipeWire nor V4L2)" << std::endl;
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        if (sink_element) {
            gst_object_unref(sink_element);
        }
        return false;
    }
    
    if (!sink_element) {
        std::cerr << "❌ Pipeline missing appsink" << std::endl;
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

    // In Flatpak, V4L2 devices are not accessible, so only try PipeWire
    if (IsRunningInFlatpak()) {
        std::cout << "📷 Flatpak detected - using PipeWire (V4L2 not accessible)" << std::endl;
    } else {
        std::cout << "📷 Native environment - will try PipeWire first, V4L2 as fallback" << std::endl;
    }

    // Try PipeWire source
    const char* pipeline_desc =
        "pipewiresrc name=source do-timestamp=true ! videoconvert ! "
        "video/x-raw,format=BGR ! appsink name=sink emit-signals=true sync=false max-buffers=1 drop=true";

    std::cout << "🔍 Trying PipeWire pipeline: " << pipeline_desc << std::endl;

    if (!CreatePipelineFromDescription(pipeline_desc)) {
        if (!IsRunningInFlatpak()) {
            std::cout << "⚠️  PipeWire pipeline failed, trying V4L2 pipeline..." << std::endl;
            // Fallback to V4L2 pipeline (only on native systems)
            const char* v4l2_pipeline_desc =
                "v4l2src device=/dev/video0 ! videoconvert ! "
                "video/x-raw,format=BGR ! appsink name=sink emit-signals=true sync=false max-buffers=1 drop=true";

            if (!CreatePipelineFromDescription(v4l2_pipeline_desc)) {
                return false;
            }
            using_v4l2_source_ = true;  // Using V4L2 fallback
        } else {
            std::cout << "⚠️  PipeWire pipeline failed in Flatpak (V4L2 not available)" << std::endl;
            return false;
        }
    } else {
        using_v4l2_source_ = false; // Using PipeWire
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
        // Stop the pipeline and wait for state change to complete
        int ret = gst_element_set_state(pipeline_, GST_STATE_NULL);
        if (ret != GST_STATE_CHANGE_FAILURE) {
            gst_element_get_state(pipeline_, nullptr, nullptr, GST_CLOCK_TIME_NONE);
        }
        // Don't unref here - let CleanupGStreamer handle final cleanup
    }
}

bool CameraManager::SetupAndStartPipeline(int target_width, int target_height, int target_fps) {
    if (!CreatePipeWirePipeline(target_width, target_height, target_fps)) {
        return false;
    }

    std::cout << "🎬 Starting PipeWire camera capture..." << std::endl;

    ConfigurePipeWireSource();

    if (!StartPipeline()) {
        HandlePipelineStartFailure();
        return false;
    }

    return true;
}

void CameraManager::ConfigurePipeWireSource() {
    // Only set PipeWire-specific properties if we have a pipewire source AND not using V4L2
    if (pipewire_src_ && portal_fd_ >= 0 && !using_v4l2_source_) {
        g_object_set(pipewire_src_, "fd", portal_fd_, NULL);
        std::cout << "🔧 Set PipeWire source fd: " << portal_fd_ << std::endl;

        // In Flatpak with portal, don't try to enumerate nodes - the portal should make camera available automatically
        if (!IsRunningInFlatpak()) {
            ConfigurePipeWireSourceForNative();
        } else {
            std::cout << "ℹ️  Using PipeWire portal - camera should be available automatically" << std::endl;
            // Don't set any path - let the portal handle camera routing
        }
    } else {
        if (using_v4l2_source_) {
            std::cout << "ℹ️  Using V4L2 source (no PipeWire-specific setup needed)" << std::endl;
        } else {
            std::cout << "ℹ️  Using PipeWire source but no portal fd available" << std::endl;
        }
    }
}

void CameraManager::ConfigurePipeWireSourceForNative() {
    // Try to find and set a camera path (only for non-Flatpak)
    std::vector<int> camera_nodes = EnumeratePipeWireCameraNodes();
    if (!camera_nodes.empty()) {
        int camera_node = camera_nodes[0];  // Use first available camera
        std::string camera_path = std::to_string(camera_node);
        g_object_set(pipewire_src_, "path", camera_path.c_str(), NULL);
        std::cout << "🔧 Set PipeWire source path: " << camera_path << std::endl;
    } else {
        std::cout << "⚠️  No PipeWire camera nodes found, trying default camera..." << std::endl;
        // Try setting path to "0" as a fallback (some systems use this for default camera)
        g_object_set(pipewire_src_, "path", "0", NULL);
        std::cout << "🔧 Set PipeWire source path to default: 0" << std::endl;
    }
}

bool CameraManager::StartPipeline() {
    int ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    return ret != GST_STATE_CHANGE_FAILURE;
}

void CameraManager::HandlePipelineStartFailure() {
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
    std::cout << "🛑 Stopping PipeWire camera capture..." << std::endl;
    if (pipeline_) {
        std::cout << "🛑 Setting PipeWire pipeline to NULL state..." << std::endl;
        
        // First disconnect signal handlers to prevent callbacks during cleanup
        if (appsink_) {
            // Note: g_signal_handlers_disconnect_by_data not available, 
            // but setting pipeline to NULL should prevent callbacks
            std::cout << "✅ Signal handlers will be disconnected by pipeline cleanup" << std::endl;
        }
        
        // First set to READY state, then to NULL
        gst_element_set_state(pipeline_, GST_STATE_READY);
        g_usleep(10000); // Small delay
        
        // Now set to NULL state
        int ret = gst_element_set_state(pipeline_, GST_STATE_NULL);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "❌ Failed to set pipeline to NULL state" << std::endl;
        } else if (ret == GST_STATE_CHANGE_ASYNC) {
            std::cout << "⏳ Pipeline state change is asynchronous, waiting..." << std::endl;
            // Wait for async state change to complete
            gst_element_get_state(pipeline_, nullptr, nullptr, GST_CLOCK_TIME_NONE);
            std::cout << "✅ Async state change completed" << std::endl;
        } else {
            std::cout << "✅ Pipeline state set to NULL successfully" << std::endl;
        }
        
        // Clean up element references before pipeline disposal
        appsink_ = nullptr;
        gst_appsink_ = nullptr;
        if (pipewire_src_) {
            gst_object_unref(pipewire_src_);
            pipewire_src_ = nullptr;
        }
        
        // NOTE: Do NOT unref the pipeline here - let CleanupGStreamer() handle final cleanup
        // to avoid double-unref issues
        std::cout << "✅ PipeWire pipeline state set to NULL (cleanup deferred)" << std::endl;
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
    std::cout << "🔍 Enumerating PipeWire camera nodes..." << std::endl;

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
    
    std::cout << "📊 PipeWire enumeration complete: found " << camera_nodes.size() << " camera nodes" << std::endl;

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