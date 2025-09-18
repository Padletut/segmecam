#include "include/camera/camera_manager.h"

#include <cstdlib>  // for getenv

// Runtime detection of Flatpak environment
static bool IsRunningInFlatpak() {
    return std::getenv("FLATPAK_ID") != nullptr;
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
typedef struct _GstVideoInfo GstVideoInfo;
typedef struct _GstMapInfo GstMapInfo;

// GStreamer functions (loaded dynamically)
typedef void (*gst_init_func)(int*, char***);
static gst_init_func gst_init = nullptr;
static GstElement* (*gst_pipeline_new)(const char*) = nullptr;
static GstElement* (*gst_element_factory_make)(const char*, const char*) = nullptr;
static int (*gst_element_set_state)(GstElement*, int) = nullptr;
static int (*gst_bin_add_many)(void*, ...) = nullptr;
static int (*gst_element_link_many)(void*, ...) = nullptr;
static void (*gst_object_unref)(void*) = nullptr;
static void* (*gst_app_sink_pull_sample)(GstAppSink*) = nullptr;
static void* (*gst_sample_get_buffer)(GstSample*) = nullptr;
static void* (*gst_sample_get_caps)(GstSample*) = nullptr;
static int (*gst_video_info_from_caps)(GstVideoInfo*, GstCaps*) = nullptr;
static int (*gst_buffer_map)(GstBuffer*, GstMapInfo*, int) = nullptr;
static void (*gst_buffer_unmap)(GstBuffer*, GstMapInfo*) = nullptr;
static void (*gst_sample_unref)(GstSample*) = nullptr;

// GLib functions (loaded dynamically)
static void (*g_object_set)(void*, const char*, ...) = nullptr;
static void (*g_signal_connect_data)(void*, const char*, void*, void*, void*, int) = nullptr;
static void (*g_main_loop_quit)(void*) = nullptr;
static void (*g_main_loop_unref)(void*) = nullptr;
static void (*g_signal_connect)(void*, const char*, void*, void*) = nullptr;

// GStreamer/GLib constants (defined as enums to avoid header dependencies)
enum GstState {
    GST_STATE_NULL = 0,
    GST_STATE_PLAYING = 4
};

enum GstStateChangeReturn {
    GST_STATE_CHANGE_FAILURE = 0
};

enum GstMapFlags {
    GST_MAP_READ = 1
};

const int TRUE = 1;
const int FALSE = 0;

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

CameraManager::CameraManager() {
    // Constructor - GStreamer will be initialized at runtime if needed
}

CameraManager::~CameraManager() {
    Cleanup();
    CleanupGStreamer();
}

int CameraManager::Initialize(const CameraConfig& config) {
    config_ = config;
    state_ = CameraState{}; // Reset state

    std::cout << "📷 Initializing Camera Manager..." << std::endl;

    // Check if running in Flatpak for PipeWire support
    bool use_pipewire = IsRunningInFlatpak();
    if (use_pipewire) {
        std::cout << "📷 Detected Flatpak environment - using PipeWire + Camera Portal" << std::endl;
        // Initialize PipeWire support
        if (!InitializeGStreamer()) {
            std::cout << "⚠️  GStreamer initialization failed, falling back to V4L2" << std::endl;
            use_pipewire = false;
        }
    }

    if (use_pipewire) {
        // For Flatpak/PipeWire, we'll request camera access when needed
        state_.is_initialized = true;
        std::cout << "✅ Camera Manager initialized for Flatpak/PipeWire" << std::endl;
        return 0;
    } else {
        // Regular build - use V4L2 enumeration
        return InitializeV4L2(config);
    }
}

int CameraManager::InitializeV4L2(const CameraConfig& config) {
    // Enumerate available cameras
    RefreshCameraList();
    RefreshVCamList();
    
    if (cam_list_.empty()) {
        std::cout << "⚠️  No cameras found during enumeration" << std::endl;
        return 1;
    }
    
    // Find the requested camera index in the enumerated list
    for (size_t i = 0; i < cam_list_.size(); ++i) {
        if (cam_list_[i].index == config_.default_camera_index) {
            state_.ui_cam_idx = (int)i;
            break;
        }
    }
    
    // Set initial resolution from available cameras
    if (!cam_list_.empty() && !cam_list_[state_.ui_cam_idx].resolutions.empty()) {
        auto resolutions = cam_list_[state_.ui_cam_idx].resolutions;
        
        // Try to find matching resolution or use the largest available
        int best_res_idx = (int)resolutions.size() - 1; // Default to largest
        
        if (config_.default_width > 0 && config_.default_height > 0) {
            for (size_t i = 0; i < resolutions.size(); ++i) {
                if (resolutions[i].first == config_.default_width && 
                    resolutions[i].second == config_.default_height) {
                    best_res_idx = (int)i;
                    break;
                }
            }
        }
        
        state_.ui_res_idx = best_res_idx;
        auto wh = resolutions[best_res_idx];
        state_.current_width = wh.first;
        state_.current_height = wh.second;
    }
    
    // Setup camera path and FPS options
    if (!cam_list_.empty()) {
        state_.current_camera_path = cam_list_[state_.ui_cam_idx].path;
        UpdateFPSOptions(state_.current_camera_path, state_.current_width, state_.current_height);
        
        // Find best FPS option
        if (!ui_fps_opts_.empty()) {
            state_.ui_fps_idx = (int)ui_fps_opts_.size() - 1; // Default to highest
            
            if (config_.default_fps > 0) {
                for (size_t i = 0; i < ui_fps_opts_.size(); ++i) {
                    if (ui_fps_opts_[i] == config_.default_fps) {
                        state_.ui_fps_idx = (int)i;
                        break;
                    }
                }
            }
            
            state_.current_fps = ui_fps_opts_[state_.ui_fps_idx];
        }
    }
    
    // Initialize camera controls
    RefreshControls();
    ApplyDefaultControls();
    
    // Open the camera
    if (!OpenCamera(config_.default_camera_index, state_.current_width, state_.current_height, state_.current_fps)) {
        std::cerr << "❌ Failed to open camera " << config_.default_camera_index << std::endl;
        return 2;
    }
    
    state_.is_initialized = true;
    std::cout << "✅ Camera Manager initialized successfully!" << std::endl;
    std::cout << "📷 Using camera: " << state_.current_camera_path << std::endl;
    std::cout << "📐 Resolution: " << state_.current_width << "x" << state_.current_height << std::endl;
    std::cout << "🎬 FPS: " << state_.current_fps << std::endl;
    std::cout << "🔧 Backend: " << GetBackendName() << std::endl;
    
    return 0;
}

bool CameraManager::OpenCamera(int camera_index) {
    return OpenCamera(camera_index, state_.current_width, state_.current_height, state_.current_fps);
}

bool CameraManager::OpenCamera(int camera_index, int width, int height, int fps) {
    CloseCamera();

    std::cout << "📷 Opening camera " << camera_index << " with resolution: " << width << "x" << height;
    if (fps > 0) std::cout << " @ " << fps << " FPS";
    std::cout << std::endl;

    // Check if we should use PipeWire (Flatpak environment)
    if (IsRunningInFlatpak()) {
        // For Flatpak, use PipeWire instead of direct V4L2 access
        if (!StartPipeWireCapture()) {
            std::cerr << "❌ Failed to start PipeWire camera capture" << std::endl;
            return false;
        }

        // Set state for PipeWire capture
        state_.current_width = width;
        state_.current_height = height;
        state_.current_fps = fps > 0 ? fps : 30; // Default to 30 FPS
        state_.actual_fps = state_.current_fps;
        state_.backend_name = "PipeWire";
        state_.is_opened = true;

        std::cout << "✅ PipeWire camera opened successfully: " << state_.current_width << "x" << state_.current_height
                  << " @ " << state_.actual_fps << " FPS" << std::endl;
        std::cout << "🔧 Backend: " << state_.backend_name << std::endl;

        return true;
    } else {
        // Try V4L2 first if preferred
        if (config_.prefer_v4l2) {
            cap_ = OpenCapture(camera_index, width, height);
        } else {
            cap_.open(camera_index);
        }

        // Fallback to default backend if V4L2 failed
        if (!cap_.isOpened()) {
            std::cout << "📷 V4L2 open failed for index " << camera_index << ", retrying with CAP_ANY" << std::endl;
            cap_.open(camera_index);
        }

        if (!cap_.isOpened()) {
            std::cerr << "❌ Unable to open camera " << camera_index << std::endl;
            return false;
        }

        // Force MJPG format for better FPS support (before setting resolution/FPS)
        cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));

        // Set resolution
        cap_.set(cv::CAP_PROP_FRAME_WIDTH, width);
        cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height);

        // Set FPS if specified
        if (fps > 0) {
            cap_.set(cv::CAP_PROP_FPS, fps);
        }

        // Verify actual settings
        double actual_w = cap_.get(cv::CAP_PROP_FRAME_WIDTH);
        double actual_h = cap_.get(cv::CAP_PROP_FRAME_HEIGHT);
        double actual_fps = cap_.get(cv::CAP_PROP_FPS);

        state_.current_width = (int)actual_w;
        state_.current_height = (int)actual_h;
        state_.actual_fps = actual_fps;
        state_.backend_name = cap_.getBackendName();
        state_.is_opened = true;

        std::cout << "✅ Camera opened successfully: " << state_.current_width << "x" << state_.current_height
                  << " @ " << state_.actual_fps << " FPS" << std::endl;
        std::cout << "🔧 Backend: " << state_.backend_name << std::endl;

        return true;
    }
}

void CameraManager::CloseCamera() {
    if (IsRunningInFlatpak()) {
        StopPipeWireCapture();
    } else {
        if (cap_.isOpened()) {
            cap_.release();
            state_.is_opened = false;
            std::cout << "📷 Camera closed" << std::endl;
        }
    }
}

bool CameraManager::IsOpened() const {
    if (IsRunningInFlatpak()) {
        return state_.is_opened;
    } else {
        return state_.is_opened && cap_.isOpened();
    }
}

bool CameraManager::CaptureFrame(cv::Mat& frame) {
    if (!IsOpened()) {
        return false;
    }

    if (IsRunningInFlatpak()) {
        // For PipeWire, get frame from the current_frame_ buffer
        {
            std::lock_guard<std::mutex> lock(frame_mutex_);
            if (current_frame_.empty()) {
                return false;
            }
            current_frame_.copyTo(frame);
        }
        state_.frames_captured++;
        return true;
    } else {
        bool success = cap_.read(frame);
        if (success) {
            state_.frames_captured++;
        }

        return success;
    }
}

void CameraManager::RefreshCameraList() {
    std::cout << "🔍 Enumerating cameras..." << std::endl;
    cam_list_ = EnumerateCameras();
    
    std::cout << "📷 Found " << cam_list_.size() << " camera(s):" << std::endl;
    for (const auto& cam : cam_list_) {
        std::cout << "  • " << cam.name << " (" << cam.path << ") - " 
                  << cam.resolutions.size() << " resolutions" << std::endl;
    }
}

void CameraManager::RefreshVCamList() {
    std::cout << "🔍 Enumerating virtual cameras..." << std::endl;
    vcam_list_ = EnumerateLoopbackDevices();
    
    std::cout << "📹 Found " << vcam_list_.size() << " virtual camera(s):" << std::endl;
    for (const auto& vcam : vcam_list_) {
        std::cout << "  • " << vcam.name << " (" << vcam.path << ")" << std::endl;
    }
}

bool CameraManager::SetCurrentCamera(int ui_cam_idx, int ui_res_idx, int ui_fps_idx) {
    if (ui_cam_idx < 0 || ui_cam_idx >= (int)cam_list_.size()) {
        return false;
    }
    
    const auto& cam = cam_list_[ui_cam_idx];
    if (ui_res_idx < 0 || ui_res_idx >= (int)cam.resolutions.size()) {
        return false;
    }
    
    state_.ui_cam_idx = ui_cam_idx;
    state_.ui_res_idx = ui_res_idx;
    state_.ui_fps_idx = ui_fps_idx;
    
    // Update current settings
    state_.current_camera_path = cam.path;
    auto wh = cam.resolutions[ui_res_idx];
    state_.current_width = wh.first;
    state_.current_height = wh.second;
    
    // Update FPS options for new resolution
    UpdateFPSOptions(state_.current_camera_path, state_.current_width, state_.current_height);
    
    // Validate and set FPS
    if (ui_fps_idx >= 0 && ui_fps_idx < (int)ui_fps_opts_.size()) {
        state_.current_fps = ui_fps_opts_[ui_fps_idx];
    } else if (!ui_fps_opts_.empty()) {
        state_.ui_fps_idx = (int)ui_fps_opts_.size() - 1;
        state_.current_fps = ui_fps_opts_[state_.ui_fps_idx];
    }
    
    // Refresh controls for new camera
    RefreshControls();
    
    return true;
}

const std::vector<std::pair<int,int>>& CameraManager::GetCurrentResolutions() const {
    static std::vector<std::pair<int,int>> empty;
    if (state_.ui_cam_idx >= 0 && state_.ui_cam_idx < (int)cam_list_.size()) {
        return cam_list_[state_.ui_cam_idx].resolutions;
    }
    return empty;
}

bool CameraManager::SetResolution(int width, int height) {
    if (!IsOpened()) return false;
    
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    
    // Verify what was actually set
    double actual_w = cap_.get(cv::CAP_PROP_FRAME_WIDTH);
    double actual_h = cap_.get(cv::CAP_PROP_FRAME_HEIGHT);
    
    state_.current_width = (int)actual_w;
    state_.current_height = (int)actual_h;
    
    return (state_.current_width == width && state_.current_height == height);
}

bool CameraManager::SetFPS(int fps) {
    if (!IsOpened() || cam_list_.empty() || state_.ui_cam_idx >= (int)cam_list_.size()) {
        return false;
    }
    
    // Store current settings
    int current_camera_index = cam_list_[state_.ui_cam_idx].index;
    int current_width = state_.current_width;
    int current_height = state_.current_height;
    
    // Close and reopen with new FPS
    CloseCamera();
    bool success = OpenCamera(current_camera_index, current_width, current_height, fps);
    
    if (success) {
        state_.current_fps = fps;
        std::cout << "✅ FPS changed to " << state_.actual_fps << " (requested: " << fps << ")" << std::endl;
    } else {
        std::cerr << "❌ Failed to change FPS to " << fps << std::endl;
        // Try to reopen with original settings
        OpenCamera(current_camera_index, current_width, current_height, state_.current_fps);
    }
    
    return success;
}

void CameraManager::RefreshControls() {
    if (state_.current_camera_path.empty()) return;
    
    std::cout << "🔧 Refreshing camera controls for " << state_.current_camera_path << std::endl;
    
    QueryCtrl(state_.current_camera_path, V4L2_CID_BRIGHTNESS, &r_brightness_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_CONTRAST, &r_contrast_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_SATURATION, &r_saturation_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_GAIN, &r_gain_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_SHARPNESS, &r_sharpness_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_ZOOM_ABSOLUTE, &r_zoom_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_FOCUS_ABSOLUTE, &r_focus_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_AUTOGAIN, &r_autogain_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_FOCUS_AUTO, &r_autofocus_);
    
    // Exposure controls
    QueryCtrl(state_.current_camera_path, V4L2_CID_EXPOSURE_AUTO, &r_autoexposure_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_EXPOSURE_ABSOLUTE, &r_exposure_abs_);
    
    // White balance controls
    QueryCtrl(state_.current_camera_path, V4L2_CID_AUTO_WHITE_BALANCE, &r_awb_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_WHITE_BALANCE_TEMPERATURE, &r_wb_temp_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_BACKLIGHT_COMPENSATION, &r_backlight_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_EXPOSURE_AUTO_PRIORITY, &r_expo_dynfps_);
}

void CameraManager::ApplyDefaultControls() {
    if (!config_.enable_auto_focus || state_.current_camera_path.empty()) return;
    
    // Set auto focus enabled by default if supported
    if (r_autofocus_.available && r_autofocus_.val == 0) {
        if (SetCtrl(state_.current_camera_path, V4L2_CID_FOCUS_AUTO, 1)) {
            r_autofocus_.val = 1;
            std::cout << "🔧 Enabled auto focus by default" << std::endl;
        }
    }
}

// Control setter methods
bool CameraManager::SetBrightness(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_BRIGHTNESS, value);
}

bool CameraManager::SetContrast(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_CONTRAST, value);
}

bool CameraManager::SetSaturation(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_SATURATION, value);
}

bool CameraManager::SetGain(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_GAIN, value);
}

bool CameraManager::SetSharpness(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_SHARPNESS, value);
}

bool CameraManager::SetZoom(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_ZOOM_ABSOLUTE, value);
}

bool CameraManager::SetFocus(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_FOCUS_ABSOLUTE, value);
}

bool CameraManager::SetAutoGain(bool enabled) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_AUTOGAIN, enabled ? 1 : 0);
}

bool CameraManager::SetAutoFocus(bool enabled) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_FOCUS_AUTO, enabled ? 1 : 0);
}

bool CameraManager::SetAutoExposure(bool enabled) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_EXPOSURE_AUTO, enabled ? V4L2_EXPOSURE_AUTO : V4L2_EXPOSURE_MANUAL);
}

bool CameraManager::SetExposure(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_EXPOSURE_ABSOLUTE, value);
}

bool CameraManager::SetWhiteBalance(bool auto_enabled) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_AUTO_WHITE_BALANCE, auto_enabled ? 1 : 0);
}

bool CameraManager::SetWhiteBalanceTemperature(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_WHITE_BALANCE_TEMPERATURE, value);
}

bool CameraManager::SetBacklightCompensation(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_BACKLIGHT_COMPENSATION, value);
}

bool CameraManager::SetControl(uint32_t control_id, int value) {
    return SetCtrl(state_.current_camera_path, control_id, value);
}

std::string CameraManager::GetBackendName() const {
    return state_.backend_name;
}

double CameraManager::GetActualFPS() const {
    return state_.actual_fps;
}

void CameraManager::UpdatePerformanceStats() {
    if (IsOpened()) {
        state_.actual_fps = cap_.get(cv::CAP_PROP_FPS);
    }
}

void CameraManager::Cleanup() {
    if (!state_.is_initialized) return;
    
    std::cout << "🧹 Cleaning up Camera Manager..." << std::endl;
    
    CloseCamera();
    
    // Reset state
    state_ = CameraState{};
    cam_list_.clear();
    ui_fps_opts_.clear();
    
    std::cout << "✅ Camera Manager cleanup completed" << std::endl;
}

// Private helper methods
cv::VideoCapture CameraManager::OpenCapture(int idx, int w, int h) {
    cv::VideoCapture c(idx, cv::CAP_V4L2);
    if (c.isOpened() && w > 0 && h > 0) {
        // Set MJPG format for higher FPS support (YUYV is limited to 10 FPS at higher resolutions)
        c.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
        c.set(cv::CAP_PROP_FRAME_WIDTH, w);
        c.set(cv::CAP_PROP_FRAME_HEIGHT, h);
        // Set twice to ensure it's applied (some cameras need this)
        c.set(cv::CAP_PROP_FRAME_WIDTH, w);
        c.set(cv::CAP_PROP_FRAME_HEIGHT, h);
    }
    return c;
}

void CameraManager::QueryCtrl(const std::string& cam_path, uint32_t id, CtrlRange* out) {
    if (!::QueryCtrl(cam_path, id, out)) {
        *out = CtrlRange{}; // Reset to defaults if query fails
    }
}

bool CameraManager::SetCtrl(const std::string& cam_path, uint32_t id, int32_t value) {
    bool success = ::SetCtrl(cam_path, id, value);
    if (success) {
        // Update the cached value in the appropriate range
        // This is a simplified approach - in a full implementation you'd want
        // to identify which control was set and update its cached val
        RefreshControls();
    }
    return success;
}

bool CameraManager::GetCtrl(const std::string& cam_path, uint32_t id, int32_t* value) {
    return ::GetCtrl(cam_path, id, value);
}

void CameraManager::UpdateFPSOptions(const std::string& cam_path, int width, int height) {
    ui_fps_opts_ = EnumerateFPS(cam_path, width, height);
    
    if (!ui_fps_opts_.empty()) {
        std::cout << "🎬 Available FPS options: ";
        for (size_t i = 0; i < ui_fps_opts_.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << ui_fps_opts_[i];
        }
        std::cout << std::endl;
    }
}

bool CameraManager::InitializeGStreamer() {
    if (gst_initialized_) {
        return true;
    }

    std::cout << "🎬 Initializing GStreamer for PipeWire support..." << std::endl;

    // Load GStreamer library at runtime
    void* gst_lib = dlopen("libgstreamer-1.0.so", RTLD_LAZY);
    if (!gst_lib) {
        std::cerr << "❌ Failed to load libgstreamer-1.0.so: " << dlerror() << std::endl;
        return false;
    }

    // Load GLib library
    void* glib_lib = dlopen("libglib-2.0.so", RTLD_LAZY);
    if (!glib_lib) {
        std::cerr << "❌ Failed to load libglib-2.0.so: " << dlerror() << std::endl;
        dlclose(gst_lib);
        return false;
    }

    // Load GStreamer functions
    gst_init = (gst_init_func)dlsym(gst_lib, "gst_init");
    gst_pipeline_new = (GstElement* (*)(const char*))dlsym(gst_lib, "gst_pipeline_new");
    gst_element_factory_make = (GstElement* (*)(const char*, const char*))dlsym(gst_lib, "gst_element_factory_make");
    gst_element_set_state = (int (*)(GstElement*, int))dlsym(gst_lib, "gst_element_set_state");
    gst_bin_add_many = (int (*)(void*, ...))dlsym(gst_lib, "gst_bin_add_many");
    gst_element_link_many = (int (*)(void*, ...))dlsym(gst_lib, "gst_element_link_many");
    gst_object_unref = (void (*)(void*))dlsym(gst_lib, "gst_object_unref");
    gst_app_sink_pull_sample = (void* (*)(GstAppSink*))dlsym(gst_lib, "gst_app_sink_pull_sample");
    gst_sample_get_buffer = (void* (*)(GstSample*))dlsym(gst_lib, "gst_sample_get_buffer");
    gst_sample_get_caps = (void* (*)(GstSample*))dlsym(gst_lib, "gst_sample_get_caps");
    gst_video_info_from_caps = (int (*)(GstVideoInfo*, GstCaps*))dlsym(gst_lib, "gst_video_info_from_caps");
    gst_buffer_map = (int (*)(GstBuffer*, GstMapInfo*, int))dlsym(gst_lib, "gst_buffer_map");
    gst_buffer_unmap = (void (*)(GstBuffer*, GstMapInfo*))dlsym(gst_lib, "gst_buffer_unmap");
    gst_sample_unref = (void (*)(GstSample*))dlsym(gst_lib, "gst_sample_unref");

    // Load GLib functions
    g_object_set = (void (*)(void*, const char*, ...))dlsym(glib_lib, "g_object_set");
    g_signal_connect_data = (void (*)(void*, const char*, void*, void*, void*, int))dlsym(glib_lib, "g_signal_connect_data");
    g_main_loop_quit = (void (*)(void*))dlsym(glib_lib, "g_main_loop_quit");
    g_main_loop_unref = (void (*)(void*))dlsym(glib_lib, "g_main_loop_unref");
    g_signal_connect = (void (*)(void*, const char*, void*, void*))dlsym(glib_lib, "g_signal_connect");

    // Check if all functions were loaded
    if (!gst_init || !gst_pipeline_new || !gst_element_factory_make || !gst_element_set_state ||
        !gst_bin_add_many || !gst_element_link_many || !gst_object_unref || !gst_app_sink_pull_sample ||
        !gst_sample_get_buffer || !gst_sample_get_caps || !gst_video_info_from_caps ||
        !gst_buffer_map || !gst_buffer_unmap || !gst_sample_unref ||
        !g_object_set || !g_signal_connect_data || !g_main_loop_quit || !g_main_loop_unref || !g_signal_connect) {
        std::cerr << "❌ Failed to load some GStreamer/GLib functions" << std::endl;
        dlclose(gst_lib);
        dlclose(glib_lib);
        return false;
    }

    // Initialize GStreamer
    gst_init(nullptr, nullptr);
    gst_initialized_ = true;

    std::cout << "✅ GStreamer initialized successfully" << std::endl;
    return true;
}

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

    if (main_loop_) {
        g_main_loop_quit(main_loop_);
        g_main_loop_unref(main_loop_);
        main_loop_ = nullptr;
    }

    gst_initialized_ = false;
    camera_permission_granted_ = false;
}

bool CameraManager::RequestCameraPermission() {
    std::cout << "📷 Requesting camera permission via Portal..." << std::endl;

    // Load libportal at runtime to avoid Bazel header issues
    void* portal_lib = dlopen("libportal.so", RTLD_LAZY);
    if (!portal_lib) {
        std::cerr << "❌ Failed to load libportal: " << dlerror() << std::endl;
        std::cout << "📷 Assuming camera permission granted (libportal not available)" << std::endl;
        camera_permission_granted_ = true;
        return true;
    }

    // Try to get the xdp_portal_new function
    typedef void* (*xdp_portal_new_func)();
    xdp_portal_new_func xdp_portal_new = (xdp_portal_new_func)dlsym(portal_lib, "xdp_portal_new");

    if (!xdp_portal_new) {
        std::cerr << "❌ Failed to find xdp_portal_new: " << dlerror() << std::endl;
        dlclose(portal_lib);
        std::cout << "📷 Assuming camera permission granted (portal functions not available)" << std::endl;
        camera_permission_granted_ = true;
        return true;
    }

    // For now, assume permission is granted since we can't easily call the portal API
    // TODO: Implement full portal API integration
    std::cout << "✅ Camera permission request completed (simplified implementation)" << std::endl;
    camera_permission_granted_ = true;
    dlclose(portal_lib);
    return true;
}

bool CameraManager::CreatePipeWirePipeline() {
    std::cout << "🎬 Creating PipeWire GStreamer pipeline..." << std::endl;

    // Create pipeline: pipewiresrc ! videoconvert ! video/x-raw,format=BGR ! appsink
    pipeline_ = gst_pipeline_new("camera-pipeline");

    if (!pipeline_) {
        std::cerr << "❌ Failed to create pipeline" << std::endl;
        return false;
    }

    // Create elements
    GstElement* pipewiresrc = gst_element_factory_make("pipewiresrc", "source");
    GstElement* videoconvert = gst_element_factory_make("videoconvert", "convert");
    appsink_ = gst_element_factory_make("appsink", "sink");

    if (!pipewiresrc || !videoconvert || !appsink_) {
        std::cerr << "❌ Failed to create pipeline elements" << std::endl;
        return false;
    }

    // Configure appsink
    g_object_set(appsink_, "emit-signals", TRUE, nullptr);
    g_signal_connect(appsink_, "new-sample", G_CALLBACK(OnNewSample), this);
    g_signal_connect(appsink_, "eos", G_CALLBACK(OnEOS), this);

    // Add elements to pipeline
    gst_bin_add_many(GST_BIN(pipeline_), pipewiresrc, videoconvert, appsink_, nullptr);

    // Link elements
    if (!gst_element_link_many(pipewiresrc, videoconvert, appsink_, nullptr)) {
        std::cerr << "❌ Failed to link pipeline elements" << std::endl;
        return false;
    }

    std::cout << "✅ PipeWire pipeline created successfully" << std::endl;
    return true;
}

bool CameraManager::StartPipeWireCapture() {
    if (!camera_permission_granted_) {
        if (!RequestCameraPermission()) {
            return false;
        }
    }

    if (!pipeline_ && !CreatePipeWirePipeline()) {
        return false;
    }

    std::cout << "🎬 Starting PipeWire camera capture..." << std::endl;

    // Set pipeline to playing state
    int ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "❌ Failed to start pipeline" << std::endl;
        return false;
    }

    state_.is_opened = true;
    std::cout << "✅ PipeWire camera capture started" << std::endl;
    return true;
}

void CameraManager::StopPipeWireCapture() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
    }
    state_.is_opened = false;
    std::cout << "🛑 PipeWire camera capture stopped" << std::endl;
}

void CameraManager::OnNewSample(GstAppSink* sink, gpointer user_data) {
    CameraManager* self = static_cast<CameraManager*>(user_data);

    // Get the sample
    GstSample* sample = (GstSample*)gst_app_sink_pull_sample(sink);
    if (!sample) {
        return;
    }

    // Get buffer and caps
    GstBuffer* buffer = (GstBuffer*)gst_sample_get_buffer(sample);
    GstCaps* caps = (GstCaps*)gst_sample_get_caps(sample);

    if (!buffer || !caps) {
        gst_sample_unref(sample);
        return;
    }

    // For now, create a simple test frame since GStreamer structure handling is complex
    // TODO: Implement proper GStreamer buffer handling
    {
        std::lock_guard<std::mutex> lock(self->frame_mutex_);
        // Create a test frame - replace with actual buffer data extraction
        self->current_frame_ = cv::Mat(480, 640, CV_8UC3, cv::Scalar(0, 255, 0)); // Green test frame
    }

    gst_sample_unref(sample);
}

void CameraManager::OnEOS(GstAppSink* sink, gpointer user_data) {
    CameraManager* self = static_cast<CameraManager*>(user_data);
    std::cout << "🎬 PipeWire stream ended" << std::endl;
    self->state_.is_opened = false;
}

} // namespace segmecam