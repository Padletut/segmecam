#include "include/camera/camera_manager.h"
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

namespace {
constexpr unsigned int kXdpCameraFlagNone = 0;
#include "camera/camera_manager.h"

std::string ToUpperCopy(const std::string& value) {
    std::string upper = value;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return upper;
}

// Enumerate PipeWire camera nodes using pw-cli
// Implementation moved to camera_pipewire.cpp

// Get PipeWire node ID for a given camera index
// Implementation moved to camera_pipewire.cpp

} // namespace

CameraManager::CameraManager() {
    // Constructor - GStreamer will be initialized at runtime if needed
}

CameraManager::~CameraManager() {
    Cleanup();
    CleanupGStreamer();
    if (portal_fd_ >= 0) {
        close(portal_fd_);
        portal_fd_ = -1;
    }
    if (portal_instance_) {
        if (g_object_unref_ptr) {
            g_object_unref_ptr(portal_instance_);
        }
        portal_instance_ = nullptr;
    }
    if (portal_library_handle_) {
        dlclose(portal_library_handle_);
        portal_library_handle_ = nullptr;
    }
}

std::vector<CameraDesc> CameraManager::EnumerateCamerasPortal() {
    std::vector<CameraDesc> cams;

    CameraDesc cam;
    cam.path = "PipeWire";
    cam.name = "PipeWire Camera";
    cam.index = 0;

    cam.resolutions = {
        {640, 480},
        {800, 600},
        {960, 720},
        {1280, 720},
        {1600, 900},
        {1920, 1080}
    };

    if (config_.default_width > 0 && config_.default_height > 0) {
        auto desired = std::make_pair(config_.default_width, config_.default_height);
        if (std::find(cam.resolutions.begin(), cam.resolutions.end(), desired) == cam.resolutions.end()) {
            cam.resolutions.push_back(desired);
        }
    }

    std::sort(cam.resolutions.begin(), cam.resolutions.end(), [](const auto& a, const auto& b) {
        long area_a = static_cast<long>(a.first) * a.second;
        long area_b = static_cast<long>(b.first) * b.second;
        if (area_a == area_b) {
            return a.first < b.first;
        }
        return area_a < area_b;
    });
    cam.resolutions.erase(std::unique(cam.resolutions.begin(), cam.resolutions.end()), cam.resolutions.end());

    cams.push_back(cam);
    return cams;
}

void CameraManager::LogV4L2InitializationSuccess() {
    std::cout << "✅ Camera Manager initialized successfully!" << std::endl;
    std::cout << "📷 Using camera: " << state_.current_camera_path << std::endl;
    std::cout << "📐 Resolution: " << state_.current_width << "x" << state_.current_height << std::endl;
    std::cout << "🎬 FPS: " << state_.current_fps << std::endl;
    std::cout << "🔧 Backend: " << GetBackendName() << std::endl;
}

void CameraManager::SelectInitialCamera(const CameraConfig& config) {
    // Find the requested camera index in the enumerated list
    for (size_t i = 0; i < cam_list_.size(); ++i) {
        if (cam_list_[i].index == config.default_camera_index) {
            state_.ui_cam_idx = (int)i;
            break;
        }
    }
}

void CameraManager::SelectInitialResolution(const CameraConfig& config) {
    // Set initial resolution from available cameras
    if (!cam_list_.empty() && !cam_list_[state_.ui_cam_idx].resolutions.empty()) {
        auto resolutions = cam_list_[state_.ui_cam_idx].resolutions;
        
        // Try to find matching resolution or use the largest available
        int best_res_idx = (int)resolutions.size() - 1; // Default to largest
        
        if (config.default_width > 0 && config.default_height > 0) {
            for (size_t i = 0; i < resolutions.size(); ++i) {
                if (resolutions[i].first == config.default_width && 
                    resolutions[i].second == config.default_height) {
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
}

void CameraManager::SetupCameraPathAndFPS(const CameraConfig& config) {
    // Setup camera path and FPS options
    if (!cam_list_.empty()) {
        state_.current_camera_path = cam_list_[state_.ui_cam_idx].path;
        UpdateFPSOptions(state_.current_camera_path, state_.current_width, state_.current_height);
        
        // Find best FPS option
        if (!ui_fps_opts_.empty()) {
            state_.ui_fps_idx = (int)ui_fps_opts_.size() - 1; // Default to highest
            
            if (config.default_fps > 0) {
                for (size_t i = 0; i < ui_fps_opts_.size(); ++i) {
                    if (ui_fps_opts_[i] == config.default_fps) {
                        state_.ui_fps_idx = (int)i;
                        break;
                    }
                }
            }
            
            state_.current_fps = ui_fps_opts_[state_.ui_fps_idx];
        }
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
    
    SelectInitialCamera(config);
    SelectInitialResolution(config);
    SetupCameraPathAndFPS(config);
    
    // Initialize camera controls
    RefreshControls();
    ApplyDefaultControls();
    
    // Open the camera
    if (!OpenCamera(config.default_camera_index, state_.current_width, state_.current_height, state_.current_fps)) {
        std::cerr << "❌ Failed to open camera " << config.default_camera_index << std::endl;
        return 2;
    }
    
    state_.is_initialized = true;
    LogV4L2InitializationSuccess();
    
    return 0;
}

bool CameraManager::OpenCamera(int camera_index) {
    return OpenCamera(camera_index, state_.current_width, state_.current_height, state_.current_fps);
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

void CameraManager::RefreshCameraList() {
    std::cout << "🔍 Enumerating cameras..." << std::endl;
    if (IsRunningInFlatpak()) {
        cam_list_ = EnumerateCamerasPortal();
    } else {
        cam_list_ = EnumerateCameras();
    }
    
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
    if (cam_path == "PipeWire" || cam_path == "pipewire") {
        ui_fps_opts_ = {15, 24, 30, 45, 60};
        return;
    }

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

bool CameraManager::ConvertSampleToBgr(GstSample* sample, cv::Mat& frame_out, int& width_out, int& height_out) {
    if (!sample) {
        return false;
    }

    if (!ValidateGStreamerFunctions()) {
        if (gst_sample_unref) {
            gst_sample_unref(sample);
        }
        return false;
    }

    GstBuffer* buffer = nullptr;
    GstCaps* caps = nullptr;
    if (!ExtractSampleComponents(sample, buffer, caps)) {
        gst_sample_unref(sample);
        return false;
    }

    int width, height, stride_hint;
    std::string format;
    InitializeConversionParameters(width, height, stride_hint, format, width_out, height_out);

    if (!ParseCapsStructure(caps, width, height, stride_hint, format)) {
        gst_sample_unref(sample);
        return false;
    }

    if (width <= 0 || height <= 0) {
        gst_sample_unref(sample);
        return false;
    }

    BufferInfo buffer_info = {nullptr, 0};  // Will be set in PerformConversionAndCleanup
    return PerformConversionAndCleanup(buffer, sample, width, height, format, stride_hint,
                                      frame_out, width_out, height_out);
}

// ConvertSampleToBgr helper methods
bool CameraManager::ParseCapsStructure(GstCaps* caps, int& width, int& height, int& stride_hint, std::string& format) {
    GstStructure* structure = nullptr;
    if (!GetStructureFromCaps(caps, structure)) {
        return false;
    }

    // Get width, height, stride, and format using helper methods
    ExtractIntFromStructure(structure, "width", width);
    ExtractIntFromStructure(structure, "height", height);
    ExtractIntFromStructure(structure, "stride", stride_hint);
    ExtractStringFromStructure(structure, "format", format);

    return true;
}

// ParseCapsStructure helper methods
bool CameraManager::GetStructureFromCaps(GstCaps* caps, GstStructure*& structure) {
    if (!gst_caps_get_structure) {
        return false;
    }

    structure = gst_caps_get_structure(caps, 0);
    return structure != nullptr;
}

bool CameraManager::ExtractIntFromStructure(GstStructure* structure, const char* field_name, int& value) {
    if (!gst_structure_get_int) {
        return false;
    }

    int extracted_value = 0;
    if (gst_structure_get_int(structure, field_name, &extracted_value) && extracted_value > 0) {
        value = extracted_value;
        return true;
    }
    return false;
}

bool CameraManager::ExtractStringFromStructure(GstStructure* structure, const char* field_name, std::string& value) {
    if (!gst_structure_get_string) {
        return false;
    }

    const char* extracted_value = gst_structure_get_string(structure, field_name);
    if (extracted_value && *extracted_value) {
        value = extracted_value;
        return true;
    }
    return false;
}

bool CameraManager::MapAndValidateBuffer(GstBuffer* buffer, GstMapInfo& map_info) {
    if (!gst_buffer_map) {
        return false;
    }

    return gst_buffer_map(buffer, &map_info, GST_MAP_READ);
}

bool CameraManager::ConvertBufferToBgrWithValidation(const BufferInfo& buffer_info, int width, int height, 
                                                    const std::string& format, int stride_hint, cv::Mat& output) {
    return segmecam::ConvertBufferToBgr(buffer_info, width, height, format, stride_hint, output);
}

bool CameraManager::ValidateGStreamerFunctions() {
    return gst_sample_get_buffer && gst_sample_get_caps && gst_buffer_map && 
           gst_buffer_unmap && gst_sample_unref;
}

bool CameraManager::ExtractSampleComponents(GstSample* sample, GstBuffer*& buffer, GstCaps*& caps) {
    buffer = static_cast<GstBuffer*>(gst_sample_get_buffer(sample));
    caps = static_cast<GstCaps*>(gst_sample_get_caps(sample));
    return buffer && caps;
}

void CameraManager::InitializeConversionParameters(int& width, int& height, int& stride_hint, std::string& format, 
                                                  int width_out, int height_out) {
    width = width_out > 0 ? width_out : 640;
    height = height_out > 0 ? height_out : 480;
    stride_hint = 0;
    format = "BGR";
}

void CameraManager::LogConversionResult(const std::string& format, int width, int height, int stride_hint, 
                                        size_t map_size, bool success, int channels) {
    static int format_log_count = 0;
    if (format_log_count < 5) {
        std::cout << "📄 PipeWire sample format: caps_format=" << format
                  << " width=" << width << " height=" << height << " stride_hint=" << stride_hint
                  << " map_size=" << map_size << " success=" << std::boolalpha << success
                  << " channels=" << channels << std::endl;
        format_log_count++;
    }
}

bool CameraManager::PerformConversionAndCleanup(GstBuffer* buffer, GstSample* sample,
                                               int width, int height, const std::string& format, int stride_hint,
                                               cv::Mat& frame_out, int& width_out, int& height_out) {
    GstMapInfo map_info = {};
    if (!MapAndValidateBuffer(buffer, map_info)) {
        gst_sample_unref(sample);
        return false;
    }

    cv::Mat converted;
    BufferInfo actual_buffer_info = {map_info.data, map_info.size};
    bool success = ConvertBufferToBgrWithValidation(actual_buffer_info, width, height, format, stride_hint, converted);

    LogConversionResult(format, width, height, stride_hint, map_info.size, success, 
                       converted.empty() ? 0 : converted.channels());

    gst_buffer_unmap(buffer, &map_info);
    gst_sample_unref(sample);

    if (!success) {
        std::cerr << "⚠️  Failed to convert PipeWire sample to BGR (format=" << format 
                  << ", width=" << width << ", height=" << height << ", stride_hint=" << stride_hint << ")" << std::endl;
        return false;
    }

    frame_out = std::move(converted);
    width_out = width;
    height_out = height;
    return true;
}

// OnPortalCameraAccessFinished helper methods
bool CameraManager::ProcessPortalAccessResult(GAsyncResult* result, bool& granted) {
    if (!xdp_portal_access_camera_finish) {
        return false;
    }

    GError* error = nullptr;
    gboolean allow = xdp_portal_access_camera_finish(portal_instance_, result, &error);
    
    if (allow) {
        granted = true;
    } else {
        std::cerr << "❌ Camera access denied by portal" << std::endl;
        state_.status_message = "Camera permission denied";
    }

    HandlePortalError(error);
    return true;
}

bool CameraManager::OpenPipeWireRemote(bool& granted) {
    if (!xdp_portal_open_pipewire_remote_for_camera) {
        return false;
    }

    if (portal_fd_ >= 0) {
        close(portal_fd_);
        portal_fd_ = -1;
    }

    int fd = xdp_portal_open_pipewire_remote_for_camera(portal_instance_);
    if (fd >= 0) {
        portal_fd_ = fd;
        granted = true;
        return true;
    } else {
        std::cerr << "❌ Unable to open PipeWire remote via portal" << std::endl;
        return false;
    }
}

void CameraManager::HandlePortalError(GError* error) {
    if (error && g_error_free) {
        g_error_free(error);
    }
}

void CameraManager::CleanupPortalRequestContext(PortalRequestContext* ctx) {
    if (ctx && ctx->loop && g_main_loop_quit) {
        g_main_loop_quit(ctx->loop);
    }
}

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

// GStreamer direct capture methods for Flatpak
// Implementation moved to camera_gstreamer_open.cpp, camera_gstreamer_close.cpp, camera_gstreamer_capture.cpp

} // namespace segmecam
