#include "include/camera/camera_manager.h"

#include <cstdlib>  // for getenv
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <utility>

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
struct PortalRequestContext {
    CameraManager* self = nullptr;
   GMainLoop* loop = nullptr;
   bool success = false;
};

std::string ToUpperCopy(const std::string& value) {
    std::string upper = value;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return upper;
}

bool ConvertBufferToBgr(const GstMapInfo& map_info,
                        int width,
                        int height,
                        const std::string& format_hint,
                        int stride_hint,
                        cv::Mat& output) {
    if (!map_info.data || width <= 0 || height <= 0) {
        return false;
    }

    std::string fmt_upper = ToUpperCopy(format_hint);
    if (fmt_upper.empty()) {
        fmt_upper = "BGR";
    }

    const std::string known_formats[] = {
        "BGR", "RGB", "BGRX", "BGRA", "RGBX", "RGBA", "YUY2", "UYVY"
    };

    bool recognized = false;
    for (const auto& candidate : known_formats) {
        if (fmt_upper == candidate) {
            recognized = true;
            break;
        }
    }

    const size_t total_pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (!recognized) {
        std::cerr << "⚠️  Unrecognized GStreamer format '" << format_hint
                  << "', applying heuristic conversion" << std::endl;
        if (total_pixels > 0) {
            size_t bytes_per_pixel_guess = map_info.size / total_pixels;
            if (bytes_per_pixel_guess == 4) {
                fmt_upper = "BGRX";
            } else if (bytes_per_pixel_guess == 2) {
                fmt_upper = "YUY2";
            } else {
                fmt_upper = "BGR";
            }
        } else {
            fmt_upper = "BGR";
        }
    }

    size_t bytes_per_pixel = 3;
    int cv_type = CV_8UC3;
    int conversion_code = -1;

    if (fmt_upper == "BGR") {
        bytes_per_pixel = 3;
        cv_type = CV_8UC3;
    } else if (fmt_upper == "RGB") {
        bytes_per_pixel = 3;
        cv_type = CV_8UC3;
        conversion_code = cv::COLOR_RGB2BGR;
    } else if (fmt_upper == "BGRX" || fmt_upper == "BGRA") {
        bytes_per_pixel = 4;
        cv_type = CV_8UC4;
        conversion_code = cv::COLOR_BGRA2BGR;
    } else if (fmt_upper == "RGBX" || fmt_upper == "RGBA") {
        bytes_per_pixel = 4;
        cv_type = CV_8UC4;
        conversion_code = cv::COLOR_RGBA2BGR;
    } else if (fmt_upper == "YUY2") {
        bytes_per_pixel = 2;
        cv_type = CV_8UC2;
        conversion_code = cv::COLOR_YUV2BGR_YUY2;
    } else if (fmt_upper == "UYVY") {
        bytes_per_pixel = 2;
        cv_type = CV_8UC2;
        conversion_code = cv::COLOR_YUV2BGR_UYVY;
    }

    size_t min_stride = static_cast<size_t>(width) * bytes_per_pixel;
    size_t stride = stride_hint > 0 ? static_cast<size_t>(stride_hint) : min_stride;
    if (stride < min_stride) {
        stride = min_stride;
    }

    if (height > 0) {
        size_t candidate_stride = map_info.size / static_cast<size_t>(height);
        if (stride_hint <= 0 && candidate_stride >= min_stride && candidate_stride % bytes_per_pixel == 0) {
            stride = candidate_stride;
        }
    }

    size_t min_bytes = min_stride * static_cast<size_t>(height);
    if (map_info.size < min_bytes) {
        std::cerr << "⚠️  GStreamer buffer too small for format " << fmt_upper
                  << ": received " << map_info.size << " bytes, expected at least "
                  << min_bytes << std::endl;
        return false;
    }

    cv::Mat wrapped(height, width, cv_type, map_info.data, stride);
    if (conversion_code >= 0) {
        cv::Mat converted;
        cv::cvtColor(wrapped, converted, conversion_code);
        output = converted;
    } else {
        output = wrapped.clone();
    }
    return true;
}
}

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
        // Provide sensible defaults when running under Flatpak
        state_.current_width = config.default_width > 0 ? config.default_width : 1280;
        state_.current_height = config.default_height > 0 ? config.default_height : 720;
        state_.current_fps = config.default_fps > 0 ? config.default_fps : 30;
        state_.current_camera_path = "PipeWire";
        state_.backend_name = "GStreamer (PipeWire)";

        if (OpenCamera(config_.default_camera_index, state_.current_width, state_.current_height, state_.current_fps)) {
            // Populate a virtual camera entry for the UI so resolution/FPS selectors remain functional.
            RefreshCameraList();
            RefreshVCamList();

            if (!cam_list_.empty()) {
                state_.ui_cam_idx = 0;
                state_.current_camera_path = cam_list_[0].path;

                if (!cam_list_[0].resolutions.empty()) {
                    state_.ui_res_idx = static_cast<int>(cam_list_[0].resolutions.size()) - 1;

                    for (size_t i = 0; i < cam_list_[0].resolutions.size(); ++i) {
                        if (cam_list_[0].resolutions[i].first == state_.current_width &&
                            cam_list_[0].resolutions[i].second == state_.current_height) {
                            state_.ui_res_idx = static_cast<int>(i);
                            break;
                        }
                    }

                    auto wh = cam_list_[0].resolutions[state_.ui_res_idx];
                    state_.current_width = wh.first;
                    state_.current_height = wh.second;
                }

                UpdateFPSOptions(state_.current_camera_path, state_.current_width, state_.current_height);
                if (!ui_fps_opts_.empty()) {
                    state_.ui_fps_idx = static_cast<int>(ui_fps_opts_.size()) - 1;
                    for (size_t i = 0; i < ui_fps_opts_.size(); ++i) {
                        if (ui_fps_opts_[i] == state_.current_fps) {
                            state_.ui_fps_idx = static_cast<int>(i);
                            break;
                        }
                    }
                    state_.current_fps = ui_fps_opts_[state_.ui_fps_idx];
                }
            }

            state_.is_initialized = true;
            std::cout << "✅ Camera Manager initialized for Flatpak/PipeWire" << std::endl;
            return 0;
        }

        std::cout << "⚠️  PipeWire initialization failed, falling back to V4L2" << std::endl;
        // Fall through to V4L2 path if PipeWire open fails
    }

    // Regular build or fallback - use V4L2 enumeration
    return InitializeV4L2(config);
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
        std::cout << "📷 Flatpak detected - using PipeWire camera access (primary method)" << std::endl;
        int target_width = width > 0 ? width : (state_.current_width > 0 ? state_.current_width : 640);
        int target_height = height > 0 ? height : (state_.current_height > 0 ? state_.current_height : 480);
        int target_fps = fps > 0 ? fps : (state_.current_fps > 0 ? state_.current_fps : 30);

        if (StartPipeWireCapture(target_width, target_height, target_fps)) {
            std::cout << "✅ PipeWire camera opened successfully" << std::endl;
            state_.current_width = target_width;
            state_.current_height = target_height;
            state_.current_fps = target_fps;
            state_.actual_fps = target_fps;
            state_.backend_name = "GStreamer (PipeWire)";
            return true;
        }

        std::cout << "⚠️  PipeWire failed, trying OpenCV/GStreamer fallbacks..." << std::endl;

        // Fallback to legacy GStreamer pipeline which enumerates cameras explicitly
        if (OpenGStreamerCamera(camera_index, target_width, target_height, target_fps)) {
            std::cout << "✅ Fallback GStreamer pipeline opened successfully" << std::endl;
            return true;
        }

        // Final fallback: use OpenCV backends (may require additional permissions)
        cap_.open(camera_index, cv::CAP_GSTREAMER);

        if (!cap_.isOpened()) {
            std::cout << "⚠️  GStreamer backend failed, trying V4L2 backend..." << std::endl;
            cap_.open(camera_index, cv::CAP_V4L2);
        }
        
        if (!cap_.isOpened()) {
            std::cout << "⚠️  V4L2 backend failed, trying default backend..." << std::endl;
            cap_.open(camera_index);
        }

        if (!cap_.isOpened()) {
            std::cerr << "❌ Unable to open camera " << camera_index << " with any backend in Flatpak" << std::endl;
            return false;
        }

        // Set resolution and FPS for OpenCV backends
        cap_.set(cv::CAP_PROP_FRAME_WIDTH, target_width);
        cap_.set(cv::CAP_PROP_FRAME_HEIGHT, target_height);
        if (target_fps > 0) {
            cap_.set(cv::CAP_PROP_FPS, target_fps);
        }

        // Get actual values
        state_.current_width = (int)cap_.get(cv::CAP_PROP_FRAME_WIDTH);
        state_.current_height = (int)cap_.get(cv::CAP_PROP_FRAME_HEIGHT);
        state_.current_fps = (int)cap_.get(cv::CAP_PROP_FPS);
        state_.actual_fps = state_.current_fps;
        state_.backend_name = "Auto (Flatpak Fallback)";
        state_.is_opened = true;

        std::cout << "✅ Camera opened via fallback in Flatpak: " << state_.current_width << "x" << state_.current_height
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
            std::cerr << "❌ Unable to open camera " << camera_index << " with any OpenCV backend" << std::endl;
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
        static int not_open_log = 0;
        if (not_open_log < 5) {
            std::cout << "⚠️  CaptureFrame invoked while camera not opened (flatpak="
                      << std::boolalpha << IsRunningInFlatpak() << ")" << std::endl;
            not_open_log++;
        }
        return false;
    }

    if (IsRunningInFlatpak()) {
        // Check if using direct GStreamer capture
        if (gst_camera_active_) {
            return CaptureGStreamerFrame(frame);
        }

        // For PipeWire, wait briefly for a frame if one is not yet ready
        std::unique_lock<std::mutex> lock(frame_mutex_);
        static int wait_log_count = 0;

        if (!frame_ready_) {
            if (wait_log_count < 10) {
                std::cout << "⏳ CaptureFrame waiting for PipeWire sample..." << std::endl;
            }

            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(800);
            bool signaled = false;
            while (!frame_ready_ && state_.is_opened) {
                signaled = frame_ready_cv_.wait_until(lock, deadline, [this]() {
                    return frame_ready_ || !state_.is_opened;
                });
                if (frame_ready_ || !state_.is_opened || std::chrono::steady_clock::now() >= deadline) {
                    break;
                }
            }

            if (wait_log_count < 10) {
                std::cout << "⏱️  CaptureFrame wait finished (signaled=" << std::boolalpha << signaled
                          << ", frame_ready=" << frame_ready_ << ", opened=" << state_.is_opened << ")" << std::endl;
                wait_log_count++;
            }
        }

        if (!frame_ready_ || current_frame_.empty()) {
            static int empty_log = 0;
            if (empty_log < 10) {
                std::cout << "⚠️  CaptureFrame returning empty frame (ready=" << std::boolalpha << frame_ready_
                          << ", empty=" << current_frame_.empty() << ")" << std::endl;
                empty_log++;
            }
            return false;
        }

        current_frame_.copyTo(frame);
        frame_ready_ = false;
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

bool CameraManager::InitializeGStreamer() {
    if (gst_initialized_) {
        return true;
    }

    std::cout << "🎬 Initializing GStreamer for PipeWire support..." << std::endl;

    // Load GStreamer core library
    std::cout << "🔍 DEBUG: Attempting to load libgstreamer-1.0.so.0" << std::endl;
    void* gst_lib = dlopen("libgstreamer-1.0.so.0", RTLD_LAZY);
    if (!gst_lib) {
        std::cerr << "❌ Failed to load libgstreamer-1.0.so.0: " << dlerror() << std::endl;
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libgstreamer-1.0.so.0" << std::endl;

    // Load GStreamer app library (needed for gst_app_sink_* functions)
    std::cout << "🔍 DEBUG: Attempting to load libgstapp-1.0.so.0" << std::endl;
    void* gstapp_lib = dlopen("libgstapp-1.0.so.0", RTLD_LAZY);
    if (!gstapp_lib) {
        std::cerr << "❌ Failed to load libgstapp-1.0.so.0: " << dlerror() << std::endl;
        dlclose(gst_lib);
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libgstapp-1.0.so.0" << std::endl;

    // Load GStreamer video library (needed for video format helpers)
    std::cout << "🔍 DEBUG: Attempting to load libgstvideo-1.0.so.0" << std::endl;
    void* gstvideo_lib = dlopen("libgstvideo-1.0.so.0", RTLD_LAZY);
    if (!gstvideo_lib) {
        std::cerr << "❌ Failed to load libgstvideo-1.0.so.0: " << dlerror() << std::endl;
        dlclose(gst_lib);
        dlclose(gstapp_lib);
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libgstvideo-1.0.so.0" << std::endl;

    // Load GLib library
    std::cout << "🔍 DEBUG: Attempting to load libglib-2.0.so.0" << std::endl;
    void* glib_lib = dlopen("libglib-2.0.so.0", RTLD_LAZY);
    if (!glib_lib) {
        std::cerr << "❌ Failed to load libglib-2.0.so.0: " << dlerror() << std::endl;
        std::cout << "🔍 DEBUG: dlerror() says: " << dlerror() << std::endl;
        dlclose(gst_lib);
        dlclose(gstapp_lib);
        dlclose(gstvideo_lib);
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libglib-2.0.so.0" << std::endl;

    // Load GObject library (needed for g_object_set, g_signal_connect)
    std::cout << "🔍 DEBUG: Attempting to load libgobject-2.0.so.0" << std::endl;
    void* gobject_lib = dlopen("libgobject-2.0.so.0", RTLD_LAZY);
    if (!gobject_lib) {
        std::cerr << "❌ Failed to load libgobject-2.0.so.0: " << dlerror() << std::endl;
        dlclose(gst_lib);
        dlclose(gstapp_lib);
        dlclose(gstvideo_lib);
        dlclose(glib_lib);
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libgobject-2.0.so.0" << std::endl;

    // Load GStreamer functions from core library
    gst_init = (gst_init_func)dlsym(gst_lib, "gst_init");
    if (!gst_init) std::cerr << "❌ gst_init dlerror: " << dlerror() << std::endl;
    gst_pipeline_new = (GstElement* (*)(const char*))dlsym(gst_lib, "gst_pipeline_new");
    if (!gst_pipeline_new) std::cerr << "❌ gst_pipeline_new dlerror: " << dlerror() << std::endl;
    gst_element_factory_make = (GstElement* (*)(const char*, const char*))dlsym(gst_lib, "gst_element_factory_make");
    if (!gst_element_factory_make) std::cerr << "❌ gst_element_factory_make dlerror: " << dlerror() << std::endl;
    gst_element_set_state = (int (*)(GstElement*, int))dlsym(gst_lib, "gst_element_set_state");
    if (!gst_element_set_state) std::cerr << "❌ gst_element_set_state dlerror: " << dlerror() << std::endl;
    gst_element_get_state = (int (*)(GstElement*, int*, int*, uint64_t))dlsym(gst_lib, "gst_element_get_state");
    if (!gst_element_get_state) std::cerr << "❌ gst_element_get_state dlerror: " << dlerror() << std::endl;
    gst_bin_add_many = (int (*)(void*, ...))dlsym(gst_lib, "gst_bin_add_many");
    if (!gst_bin_add_many) std::cerr << "❌ gst_bin_add_many dlerror: " << dlerror() << std::endl;
    gst_element_link_many = (int (*)(void*, ...))dlsym(gst_lib, "gst_element_link_many");
    if (!gst_element_link_many) std::cerr << "❌ gst_element_link_many dlerror: " << dlerror() << std::endl;
    gst_object_unref = (void (*)(void*))dlsym(gst_lib, "gst_object_unref");
    if (!gst_object_unref) std::cerr << "❌ gst_object_unref dlerror: " << dlerror() << std::endl;
    gst_sample_get_buffer = (void* (*)(GstSample*))dlsym(gst_lib, "gst_sample_get_buffer");
    if (!gst_sample_get_buffer) std::cerr << "❌ gst_sample_get_buffer dlerror: " << dlerror() << std::endl;
    gst_sample_get_caps = (void* (*)(GstSample*))dlsym(gst_lib, "gst_sample_get_caps");
    if (!gst_sample_get_caps) std::cerr << "❌ gst_sample_get_caps dlerror: " << dlerror() << std::endl;
    gst_caps_from_string = (gst_caps_from_string_func)dlsym(gst_lib, "gst_caps_from_string");
    if (!gst_caps_from_string) std::cerr << "❌ gst_caps_from_string dlerror: " << dlerror() << std::endl;
    gst_caps_unref = (gst_caps_unref_func)dlsym(gst_lib, "gst_caps_unref");
    if (!gst_caps_unref) std::cerr << "❌ gst_caps_unref dlerror: " << dlerror() << std::endl;
    gst_caps_get_structure = (GstStructure* (*)(GstCaps*, unsigned int))dlsym(gst_lib, "gst_caps_get_structure");
    if (!gst_caps_get_structure) std::cerr << "❌ gst_caps_get_structure dlerror: " << dlerror() << std::endl;
    gst_structure_get_int = (int (*)(const GstStructure*, const char*, int*))dlsym(gst_lib, "gst_structure_get_int");
    if (!gst_structure_get_int) std::cerr << "❌ gst_structure_get_int dlerror: " << dlerror() << std::endl;
    gst_structure_get_string = (const char* (*)(const GstStructure*, const char*))dlsym(gst_lib, "gst_structure_get_string");
    if (!gst_structure_get_string) std::cerr << "⚠️  gst_structure_get_string dlerror: " << dlerror() << std::endl;
    gst_element_link = (int (*)(void*, void*))dlsym(gst_lib, "gst_element_link");
    if (!gst_element_link) std::cerr << "❌ gst_element_link dlerror: " << dlerror() << std::endl;
    gst_element_link_filtered = (int (*)(void*, void*, GstCaps*))dlsym(gst_lib, "gst_element_link_filtered");
    if (!gst_element_link_filtered) std::cerr << "❌ gst_element_link_filtered dlerror: " << dlerror() << std::endl;
    gst_parse_launch = (GstElement* (*)(const char*, void**))dlsym(gst_lib, "gst_parse_launch");
    if (!gst_parse_launch) std::cerr << "❌ gst_parse_launch dlerror: " << dlerror() << std::endl;
    gst_bin_get_by_name = (GstElement* (*)(void*, const char*))dlsym(gst_lib, "gst_bin_get_by_name");
    if (!gst_bin_get_by_name) std::cerr << "❌ gst_bin_get_by_name dlerror: " << dlerror() << std::endl;
    gst_buffer_map = (int (*)(GstBuffer*, GstMapInfo*, int))dlsym(gst_lib, "gst_buffer_map");
    if (!gst_buffer_map) std::cerr << "❌ gst_buffer_map dlerror: " << dlerror() << std::endl;
    gst_buffer_unmap = (void (*)(GstBuffer*, GstMapInfo*))dlsym(gst_lib, "gst_buffer_unmap");
    if (!gst_buffer_unmap) std::cerr << "❌ gst_buffer_unmap dlerror: " << dlerror() << std::endl;
    gst_sample_unref = (void (*)(GstSample*))dlsym(gst_lib, "gst_sample_unref");
    if (!gst_sample_unref) std::cerr << "❌ gst_sample_unref dlerror: " << dlerror() << std::endl;

    // Load GStreamer app functions from app library
    gst_app_sink_pull_sample = (void* (*)(GstAppSink*))dlsym(gstapp_lib, "gst_app_sink_pull_sample");
    if (!gst_app_sink_pull_sample) std::cerr << "❌ gst_app_sink_pull_sample dlerror: " << dlerror() << std::endl;
    gst_app_sink_is_eos = (int (*)(GstAppSink*))dlsym(gstapp_lib, "gst_app_sink_is_eos");
    if (!gst_app_sink_is_eos) std::cerr << "❌ gst_app_sink_is_eos dlerror: " << dlerror() << std::endl;

    // Load GLib functions
    g_object_set = (void (*)(void*, const char*, ...))dlsym(gobject_lib, "g_object_set");
    if (!g_object_set) std::cerr << "❌ g_object_set dlerror: " << dlerror() << std::endl;
    g_main_loop_quit = (g_main_loop_quit_func)dlsym(glib_lib, "g_main_loop_quit");
    if (!g_main_loop_quit) std::cerr << "❌ g_main_loop_quit dlerror: " << dlerror() << std::endl;
    g_main_loop_unref = (g_main_loop_unref_func)dlsym(glib_lib, "g_main_loop_unref");
    if (!g_main_loop_unref) std::cerr << "❌ g_main_loop_unref dlerror: " << dlerror() << std::endl;
    g_signal_connect = (g_signal_connect_func)dlsym(gobject_lib, "g_signal_connect_data");
    if (!g_signal_connect) std::cerr << "❌ g_signal_connect dlerror: " << dlerror() << std::endl;
    g_main_loop_new = (g_main_loop_new_func)dlsym(glib_lib, "g_main_loop_new");
    if (!g_main_loop_new) std::cerr << "❌ g_main_loop_new dlerror: " << dlerror() << std::endl;
    g_main_loop_run = (g_main_loop_run_func)dlsym(glib_lib, "g_main_loop_run");
    if (!g_main_loop_run) std::cerr << "❌ g_main_loop_run dlerror: " << dlerror() << std::endl;
    g_object_unref_ptr = (g_object_unref_func)dlsym(gobject_lib, "g_object_unref");
    if (!g_object_unref_ptr) std::cerr << "❌ g_object_unref dlerror: " << dlerror() << std::endl;
    g_usleep = (g_usleep_func)dlsym(glib_lib, "g_usleep");
    if (!g_usleep) std::cerr << "❌ g_usleep dlerror: " << dlerror() << std::endl;
    g_error_free = (g_error_free_func)dlsym(glib_lib, "g_error_free");
    if (!g_error_free) std::cerr << "❌ g_error_free dlerror: " << dlerror() << std::endl;

    // Check if all functions were loaded
    std::vector<std::string> failed_functions;
    if (!gst_init) failed_functions.push_back("gst_init");
    if (!gst_pipeline_new) failed_functions.push_back("gst_pipeline_new");
    if (!gst_element_factory_make) failed_functions.push_back("gst_element_factory_make");
    if (!gst_element_set_state) failed_functions.push_back("gst_element_set_state");
    if (!gst_element_get_state) failed_functions.push_back("gst_element_get_state");
    if (!gst_bin_add_many) failed_functions.push_back("gst_bin_add_many");
    if (!gst_element_link_many) failed_functions.push_back("gst_element_link_many");
    if (!gst_object_unref) failed_functions.push_back("gst_object_unref");
    if (!gst_app_sink_pull_sample) failed_functions.push_back("gst_app_sink_pull_sample");
    if (!gst_app_sink_is_eos) failed_functions.push_back("gst_app_sink_is_eos");
    if (!gst_sample_get_buffer) failed_functions.push_back("gst_sample_get_buffer");
    if (!gst_sample_get_caps) failed_functions.push_back("gst_sample_get_caps");
    if (!gst_caps_from_string) failed_functions.push_back("gst_caps_from_string");
    if (!gst_caps_unref) failed_functions.push_back("gst_caps_unref");
    if (!gst_caps_get_structure) failed_functions.push_back("gst_caps_get_structure");
    if (!gst_structure_get_int) failed_functions.push_back("gst_structure_get_int");
    if (!gst_element_link) failed_functions.push_back("gst_element_link");
    if (!gst_element_link_filtered) failed_functions.push_back("gst_element_link_filtered");
    if (!gst_parse_launch) failed_functions.push_back("gst_parse_launch");
    if (!gst_bin_get_by_name) failed_functions.push_back("gst_bin_get_by_name");
    if (!gst_buffer_map) failed_functions.push_back("gst_buffer_map");
    if (!gst_buffer_unmap) failed_functions.push_back("gst_buffer_unmap");
    if (!gst_sample_unref) failed_functions.push_back("gst_sample_unref");
    if (!g_object_set) failed_functions.push_back("g_object_set");
    if (!g_main_loop_quit) failed_functions.push_back("g_main_loop_quit");
    if (!g_main_loop_unref) failed_functions.push_back("g_main_loop_unref");
    if (!g_signal_connect) failed_functions.push_back("g_signal_connect");
    if (!g_main_loop_new) failed_functions.push_back("g_main_loop_new");
    if (!g_main_loop_run) failed_functions.push_back("g_main_loop_run");
    if (!g_object_unref_ptr) failed_functions.push_back("g_object_unref");
    if (!g_usleep) failed_functions.push_back("g_usleep");
    if (!g_error_free) failed_functions.push_back("g_error_free");

    if (!failed_functions.empty()) {
        std::cerr << "❌ Failed to load GStreamer/GLib functions: ";
        for (size_t i = 0; i < failed_functions.size(); ++i) {
            if (i > 0) std::cerr << ", ";
            std::cerr << failed_functions[i];
        }
        std::cerr << std::endl;
        dlclose(gst_lib);
        dlclose(gstapp_lib);
        dlclose(gstvideo_lib);
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

bool CameraManager::RequestCameraPermission() {
    std::cout << "📷 Requesting camera permission via Portal..." << std::endl;

    if (camera_permission_granted_ && portal_fd_ >= 0) {
        return true;
    }

    if (!portal_library_handle_) {
        portal_library_handle_ = dlopen("libportal-1.so.0", RTLD_LAZY);
        if (!portal_library_handle_) {
            portal_library_handle_ = dlopen("libportal.so.1", RTLD_LAZY);
        }
        if (!portal_library_handle_) {
            portal_library_handle_ = dlopen("libportal.so.0", RTLD_LAZY);
        }
        if (!portal_library_handle_) {
            portal_library_handle_ = dlopen("libportal.so", RTLD_LAZY);
        }
        if (!portal_library_handle_) {
            std::cerr << "❌ Failed to load libportal: " << dlerror() << std::endl;
            state_.status_message = "Flatpak camera portal unavailable";
            return false;
        }

        xdp_portal_new = (XdpPortal* (*)())dlsym(portal_library_handle_, "xdp_portal_new");
        if (!xdp_portal_new) std::cerr << "❌ xdp_portal_new dlerror: " << dlerror() << std::endl;
        xdp_portal_is_camera_present = (gboolean (*)(XdpPortal*))dlsym(portal_library_handle_, "xdp_portal_is_camera_present");
        if (!xdp_portal_is_camera_present) std::cerr << "❌ xdp_portal_is_camera_present dlerror: " << dlerror() << std::endl;
        xdp_portal_access_camera = (decltype(xdp_portal_access_camera))dlsym(portal_library_handle_, "xdp_portal_access_camera");
        if (!xdp_portal_access_camera) std::cerr << "❌ xdp_portal_access_camera dlerror: " << dlerror() << std::endl;
        xdp_portal_access_camera_finish = (decltype(xdp_portal_access_camera_finish))dlsym(portal_library_handle_, "xdp_portal_access_camera_finish");
        if (!xdp_portal_access_camera_finish) std::cerr << "❌ xdp_portal_access_camera_finish dlerror: " << dlerror() << std::endl;
        xdp_portal_open_pipewire_remote_for_camera =
            (int (*)(XdpPortal*))dlsym(portal_library_handle_, "xdp_portal_open_pipewire_remote_for_camera");
        if (!xdp_portal_open_pipewire_remote_for_camera) std::cerr << "❌ xdp_portal_open_pipewire_remote_for_camera dlerror: " << dlerror() << std::endl;
    }

    if (!xdp_portal_new || !xdp_portal_access_camera || !xdp_portal_access_camera_finish || !xdp_portal_open_pipewire_remote_for_camera) {
        std::cerr << "❌ Portal functions unavailable" << std::endl;
        state_.status_message = "Flatpak camera portal unavailable";
        return false;
    }

    if (!portal_instance_) {
        portal_instance_ = xdp_portal_new();
        if (!portal_instance_) {
            std::cerr << "❌ Unable to create XdpPortal instance" << std::endl;
            return false;
        }
    }

    if (xdp_portal_is_camera_present && !xdp_portal_is_camera_present(portal_instance_)) {
        std::cerr << "⚠️  Camera portal reports no camera present" << std::endl;
    }

    if (portal_fd_ >= 0) {
        close(portal_fd_);
        portal_fd_ = -1;
    }

    PortalRequestContext ctx;
    ctx.self = this;
    if (g_main_loop_new) {
        ctx.loop = g_main_loop_new(nullptr, FALSE);
    } else {
        std::cerr << "❌ g_main_loop_new unavailable" << std::endl;
        return false;
    }

    xdp_portal_access_camera(portal_instance_, nullptr, kXdpCameraFlagNone, nullptr,
                             &CameraManager::OnPortalCameraAccessFinished, &ctx);

    if (ctx.loop) {
        if (g_main_loop_run) {
            g_main_loop_run(ctx.loop);
        }
        if (g_main_loop_unref) {
            g_main_loop_unref(ctx.loop);
        }
        ctx.loop = nullptr;
    }

    camera_permission_granted_ = ctx.success;
    if (!camera_permission_granted_) {
        if (portal_fd_ >= 0) {
            close(portal_fd_);
            portal_fd_ = -1;
        }
        std::cerr << "❌ Camera permission denied by portal" << std::endl;
        state_.status_message = "Camera permission denied";
        return false;
    }

    std::cout << "✅ Camera portal granted PipeWire remote (fd=" << portal_fd_ << ")" << std::endl;
    state_.status_message.clear();
    return true;
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

    pipewire_src_ = gst_bin_get_by_name ? gst_bin_get_by_name((GstBin*)pipeline_, "source") : nullptr;
    GstElement* sink_element = gst_bin_get_by_name ? gst_bin_get_by_name((GstBin*)pipeline_, "sink") : nullptr;
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
    gst_appsink_ = (GstAppSink*)sink_element;

    g_object_set(appsink_, "emit-signals", TRUE,
                 "sync", FALSE,
                 "max-buffers", 1,
                 "drop", TRUE,
                 nullptr);
    g_signal_connect(appsink_, "new-sample", G_CALLBACK(OnNewSampleWrapper), this);
    g_signal_connect(appsink_, "eos", G_CALLBACK(OnEOSWrapper), this);

    std::cout << "✅ PipeWire pipeline created successfully" << std::endl;
    return true;
}

bool CameraManager::StartPipeWireCapture(int width, int height, int fps) {
    if (!camera_permission_granted_) {
        if (!RequestCameraPermission()) {
            return false;
        }
    }

    int target_width = width > 0 ? width : (state_.current_width > 0 ? state_.current_width : 640);
    int target_height = height > 0 ? height : (state_.current_height > 0 ? state_.current_height : 480);
    int target_fps = fps > 0 ? fps : (state_.current_fps > 0 ? state_.current_fps : 30);

    if (portal_fd_ < 0) {
        std::cerr << "❌ Portal PipeWire remote unavailable" << std::endl;
        return false;
    }

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

    if (!CreatePipeWirePipeline(target_width, target_height, target_fps)) {
        return false;
    }

    std::cout << "🎬 Starting PipeWire camera capture..." << std::endl;

    // Set pipeline to playing state
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        current_frame_.release();
        frame_ready_ = false;
    }
    state_.frames_captured = 0;

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

    state_.current_width = target_width;
    state_.current_height = target_height;
    state_.current_fps = target_fps;
    state_.actual_fps = target_fps;
    state_.backend_name = "GStreamer (PipeWire)";
    state_.is_opened = true;
    std::cout << "✅ PipeWire camera capture started" << std::endl;
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

bool CameraManager::ConvertSampleToBgr(GstSample* sample, cv::Mat& frame_out, int& width_out, int& height_out) {
    if (!sample) {
        return false;
    }

    if (!gst_sample_get_buffer || !gst_sample_get_caps || !gst_buffer_map || !gst_buffer_unmap || !gst_sample_unref) {
        if (gst_sample_unref) {
            gst_sample_unref(sample);
        }
        return false;
    }

    GstBuffer* buffer = (GstBuffer*)gst_sample_get_buffer(sample);
    GstCaps* caps = (GstCaps*)gst_sample_get_caps(sample);
    if (!buffer || !caps) {
        gst_sample_unref(sample);
        return false;
    }

    int width = width_out > 0 ? width_out : 640;
    int height = height_out > 0 ? height_out : 480;
    int stride_hint = 0;
    std::string format = "BGR";

    std::string format_logged;
    if (gst_caps_get_structure) {
        GstStructure* structure = gst_caps_get_structure(caps, 0);
        if (structure) {
            int cap_width = 0;
            if (gst_structure_get_int && gst_structure_get_int(structure, "width", &cap_width) && cap_width > 0) {
                width = cap_width;
            }

            int cap_height = 0;
            if (gst_structure_get_int && gst_structure_get_int(structure, "height", &cap_height) && cap_height > 0) {
                height = cap_height;
            }

            int cap_stride = 0;
            if (gst_structure_get_int && gst_structure_get_int(structure, "stride", &cap_stride) && cap_stride > 0) {
                stride_hint = cap_stride;
            }

            if (gst_structure_get_string) {
                const char* fmt = gst_structure_get_string(structure, "format");
                if (fmt && *fmt) {
                    format = fmt;
                    format_logged = fmt;
                }
            }
        }
    }

    if (width <= 0 || height <= 0) {
        gst_sample_unref(sample);
        return false;
    }

    GstMapInfo map_info = {};
    if (!gst_buffer_map(buffer, &map_info, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return false;
    }

    cv::Mat converted;
    bool success = ConvertBufferToBgr(map_info, width, height, format, stride_hint, converted);

    static int format_log_count = 0;
    if (format_log_count < 5) {
        std::cout << "📄 PipeWire sample format: caps_format="
                  << (format_logged.empty() ? format : format_logged)
                  << " (post-heuristic: " << format << ") width=" << width
                  << " height=" << height << " stride_hint=" << stride_hint
                  << " map_size=" << map_info.size << " success=" << std::boolalpha << success
                  << " channels=" << (converted.empty() ? 0 : converted.channels())
                  << std::endl;
        format_log_count++;
    }

    gst_buffer_unmap(buffer, &map_info);
    gst_sample_unref(sample);

    if (!success) {
        std::cerr << "⚠️  Failed to convert PipeWire sample to BGR (format="
                  << format << ", width=" << width << ", height=" << height
                  << ", stride_hint=" << stride_hint << ")" << std::endl;
        return false;
    }

    frame_out = std::move(converted);
    width_out = width;
    height_out = height;
    return true;
}

void CameraManager::OnNewSample(GstAppSink* sink) {
    if (!gst_app_sink_pull_sample) {
        return;
    }

    GstSample* sample = (GstSample*)gst_app_sink_pull_sample(sink);
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
    if (!ctx || !ctx->self) {
        if (ctx && ctx->loop && ctx->self && ctx->self->g_main_loop_quit) {
            ctx->self->g_main_loop_quit(ctx->loop);
        }
        return;
    }

    CameraManager* self = ctx->self;

    bool granted = false;
    if (self->xdp_portal_access_camera_finish) {
        GError* error = nullptr;
        gboolean allow = self->xdp_portal_access_camera_finish(self->portal_instance_, result, &error);
        if (allow) {
            if (self->portal_fd_ >= 0) {
                close(self->portal_fd_);
                self->portal_fd_ = -1;
            }
            if (self->xdp_portal_open_pipewire_remote_for_camera) {
                int fd = self->xdp_portal_open_pipewire_remote_for_camera(self->portal_instance_);
                if (fd >= 0) {
                    self->portal_fd_ = fd;
                    granted = true;
                } else {
                    std::cerr << "❌ Unable to open PipeWire remote via portal" << std::endl;
                }
            }
        } else {
            std::cerr << "❌ Camera access denied by portal" << std::endl;
            self->state_.status_message = "Camera permission denied";
        }
        if (error) {
            if (self->g_error_free) {
                self->g_error_free(error);
            }
        }
    }

    ctx->success = granted;
    if (ctx->loop) {
        if (self->g_main_loop_quit) {
            self->g_main_loop_quit(ctx->loop);
        }
    }
}

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
        std::cout << "📷 Flatpak detected - trying PipeWire camera access first" << std::endl;

        // Map camera index to PipeWire node ID (for Flatpak environment)
        // camera_index 0 -> node 42 (HD Pro Webcam C920 in Flatpak)
        // camera_index 1 -> node 43 (if available)
        int pipewire_node_id = 42 + camera_index;  // Correct mapping for Flatpak

        // Create PipeWire pipeline: pipewiresrc path=39 ! videoconvert ! video/x-raw,format=BGR ! appsink name=sink
        char pipewire_pipeline_str[256];
        snprintf(pipewire_pipeline_str, sizeof(pipewire_pipeline_str),
                 "pipewiresrc path=%d ! videoconvert ! video/x-raw,format=BGR ! appsink name=sink",
                 pipewire_node_id);

        std::cout << "🎬 Trying PipeWire pipeline: " << pipewire_pipeline_str << std::endl;

        void* pw_error = nullptr;
        gst_pipeline_ = gst_parse_launch(pipewire_pipeline_str, &pw_error);

        if (gst_pipeline_) {
            std::cout << "✅ PipeWire pipeline created successfully" << std::endl;

            // Get the appsink element
            gst_appsink_ = (GstAppSink*)gst_bin_get_by_name((GstBin*)gst_pipeline_, "sink");
            if (gst_appsink_) {
                std::cout << "✅ GStreamer PipeWire pipeline created, configuring appsink..." << std::endl;

                // Configure appsink
                g_object_set(gst_appsink_, "emit-signals", TRUE, "sync", FALSE, NULL);

                // Set pipeline to playing state
                std::cout << "✅ Appsink configured, setting pipeline to playing state..." << std::endl;
                GstStateChangeReturn ret = (GstStateChangeReturn)gst_element_set_state(gst_pipeline_, GST_STATE_PLAYING);

                if (ret != GST_STATE_CHANGE_FAILURE) {
                    // Wait for pipeline to stabilize
                    std::cout << "✅ Pipeline state set to playing, waiting for stabilization..." << std::endl;
                    g_usleep(500000); // 500ms for PipeWire

                    // Check final state
                    std::cout << "✅ Pipeline stabilized, checking state..." << std::endl;
                    int state, pending;
                    ret = (GstStateChangeReturn)gst_element_get_state(gst_pipeline_, &state, &pending, GST_CLOCK_TIME_NONE);

                    if (state == GST_STATE_PLAYING) {
                        std::cout << "✅ PipeWire GStreamer pipeline ready for capture" << std::endl;
                        gst_camera_active_ = true;

                        // Set state values
                        state_.current_width = width;
                        state_.current_height = height;
                        state_.current_fps = fps;
                        state_.actual_fps = fps;
                        state_.backend_name = "GStreamer (PipeWire)";
                        state_.is_opened = true;

                        return true;
                    }
                }
            }

            // PipeWire failed, clean up
            std::cout << "⚠️  PipeWire pipeline failed, cleaning up..." << std::endl;
            if (gst_pipeline_) {
                gst_element_set_state(gst_pipeline_, GST_STATE_NULL);
                g_usleep(100000);
                gst_object_unref(gst_pipeline_);
                gst_pipeline_ = nullptr;
            }
            gst_appsink_ = nullptr;
        } else {
            std::cout << "⚠️  PipeWire pipeline creation failed" << std::endl;
            if (pw_error) {
                std::cout << "🔍 PipeWire pipeline error: " << (char*)pw_error << std::endl;
                g_error_free(pw_error);
            }
        }

        // PipeWire failed, fall back to V4L2
        std::cout << "📷 PipeWire failed, trying V4L2 camera access as fallback" << std::endl;

        // Create V4L2 pipeline: v4l2src device=/dev/video0 ! videoconvert ! video/x-raw,format=BGR ! appsink name=sink
        char v4l2_pipeline_str[256];
        snprintf(v4l2_pipeline_str, sizeof(v4l2_pipeline_str),
                 "v4l2src device=/dev/video%d ! videoconvert ! video/x-raw,format=BGR ! appsink name=sink",
                 camera_index);

        std::cout << "🎬 Trying V4L2 pipeline: " << v4l2_pipeline_str << std::endl;

        void* error = nullptr;
        gst_pipeline_ = gst_parse_launch(v4l2_pipeline_str, &error);

        if (gst_pipeline_) {
            std::cout << "✅ V4L2 pipeline created successfully" << std::endl;

            // Get the appsink element
            gst_appsink_ = (GstAppSink*)gst_bin_get_by_name((GstBin*)gst_pipeline_, "sink");
            if (gst_appsink_) {
                std::cout << "✅ GStreamer V4L2 pipeline created, configuring appsink..." << std::endl;

                // Configure appsink
                g_object_set(gst_appsink_, "emit-signals", TRUE, "sync", FALSE, NULL);

                // Set pipeline to playing state
                std::cout << "✅ Appsink configured, setting pipeline to playing state..." << std::endl;
                GstStateChangeReturn ret = (GstStateChangeReturn)gst_element_set_state(gst_pipeline_, GST_STATE_PLAYING);

                if (ret != GST_STATE_CHANGE_FAILURE) {
                    // Wait for pipeline to stabilize
                    std::cout << "✅ Pipeline state set to playing, waiting for stabilization..." << std::endl;
                    g_usleep(200000); // 200ms

                    // Check final state
                    std::cout << "✅ Pipeline stabilized, checking state..." << std::endl;
                    int state, pending;
                    ret = (GstStateChangeReturn)gst_element_get_state(gst_pipeline_, &state, &pending, GST_CLOCK_TIME_NONE);

                    if (state == GST_STATE_PLAYING) {
                        std::cout << "✅ V4L2 GStreamer pipeline ready for capture" << std::endl;
                        gst_camera_active_ = true;

                        // Set state values
                        state_.current_width = width;
                        state_.current_height = height;
                        state_.current_fps = fps;
                        state_.actual_fps = fps;
                        state_.backend_name = "GStreamer (V4L2)";
                        state_.is_opened = true;

                        return true;
                    }
                }
            }

            // V4L2 failed, clean up
            std::cout << "⚠️  V4L2 pipeline failed, cleaning up..." << std::endl;
            if (gst_pipeline_) {
                gst_element_set_state(gst_pipeline_, GST_STATE_NULL);
                g_usleep(100000);
                gst_object_unref(gst_pipeline_);
                gst_pipeline_ = nullptr;
            }
            gst_appsink_ = nullptr;
        } else {
            std::cout << "⚠️  V4L2 pipeline creation failed" << std::endl;
            if (error) {
                g_error_free(error);
            }
        }
    }

    std::cout << "❌ All GStreamer camera opening attempts failed" << std::endl;
    return false;
}

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
