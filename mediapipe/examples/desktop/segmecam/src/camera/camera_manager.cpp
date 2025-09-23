#include "include/camera/camera_manager.h"
#include "include/camera/gstreamer_utils.h"
#include "include/camera/gstreamer_buffer_utils.h"
#include "include/camera/camera_controls.h"
#include "include/camera/camera_enumeration.h"
#include "include/camera/camera_setup.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>

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

CameraManager::CameraManager() 
    : camera_controls_(std::make_unique<CameraControls>()),
      camera_enumeration_(std::make_unique<CameraEnumeration>()),
      camera_setup_(std::make_unique<CameraSetup>()) {
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

int CameraManager::InitializeV4L2(const CameraConfig& config) {
    // Store config
    config_ = config;
    
    // Initialize camera enumeration
    camera_enumeration_->Initialize();
    
    // Enumerate available cameras
    RefreshCameraList();
    RefreshVCamList();
    
    if (cam_list_.empty()) {
        std::cout << "⚠️  No cameras found during enumeration" << std::endl;
        return 1;
    }
    
    // Use CameraSetup for initial camera selection and configuration
    camera_setup_->Initialize(config, cam_list_, state_);
    camera_setup_->SelectInitialCamera(cam_list_, config, state_);
    camera_setup_->SelectInitialResolution(cam_list_, config, state_);
    camera_setup_->SetupCameraPathAndFPS(cam_list_, config, state_, ui_fps_opts_);
    
    // Initialize camera controls
    camera_controls_->Initialize(state_.current_camera_path);
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
        cam_list_ = camera_enumeration_->EnumerateCamerasPortal(config_);
    } else {
        cam_list_ = camera_enumeration_->EnumerateCameras();
    }
    
    std::cout << "📷 Found " << cam_list_.size() << " camera(s):" << std::endl;
    for (const auto& cam : cam_list_) {
        std::cout << "  • " << cam.name << " (" << cam.path << ") - " 
                  << cam.resolutions.size() << " resolutions" << std::endl;
    }
}

void CameraManager::RefreshVCamList() {
    std::cout << "🔍 Enumerating virtual cameras..." << std::endl;
    vcam_list_ = camera_enumeration_->EnumerateLoopbackDevices();
    
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
    camera_controls_->RefreshControls(state_.current_camera_path);
}

void CameraManager::ApplyDefaultControls() {
    if (!config_.enable_auto_focus || state_.current_camera_path.empty()) return;
    
    camera_controls_->ApplyDefaultControls(state_.current_camera_path, config_.enable_auto_focus);
}

// Control setter methods
bool CameraManager::SetBrightness(int value) {
    return camera_controls_->SetBrightness(state_.current_camera_path, value);
}

bool CameraManager::SetContrast(int value) {
    return camera_controls_->SetContrast(state_.current_camera_path, value);
}

bool CameraManager::SetSaturation(int value) {
    return camera_controls_->SetSaturation(state_.current_camera_path, value);
}

bool CameraManager::SetGain(int value) {
    return camera_controls_->SetGain(state_.current_camera_path, value);
}

bool CameraManager::SetSharpness(int value) {
    return camera_controls_->SetSharpness(state_.current_camera_path, value);
}

bool CameraManager::SetZoom(int value) {
    return camera_controls_->SetZoom(state_.current_camera_path, value);
}

bool CameraManager::SetFocus(int value) {
    return camera_controls_->SetFocus(state_.current_camera_path, value);
}

bool CameraManager::SetAutoGain(bool enabled) {
    return camera_controls_->SetAutoGain(state_.current_camera_path, enabled);
}

bool CameraManager::SetAutoFocus(bool enabled) {
    return camera_controls_->SetAutoFocus(state_.current_camera_path, enabled);
}

bool CameraManager::SetAutoExposure(bool enabled) {
    return camera_controls_->SetAutoExposure(state_.current_camera_path, enabled);
}

bool CameraManager::SetExposure(int value) {
    return camera_controls_->SetExposure(state_.current_camera_path, value);
}

bool CameraManager::SetWhiteBalance(bool auto_enabled) {
    return camera_controls_->SetWhiteBalance(state_.current_camera_path, auto_enabled);
}

bool CameraManager::SetWhiteBalanceTemperature(int value) {
    return camera_controls_->SetWhiteBalanceTemperature(state_.current_camera_path, value);
}

bool CameraManager::SetBacklightCompensation(int value) {
    return camera_controls_->SetBacklightCompensation(state_.current_camera_path, value);
}

bool CameraManager::SetControl(uint32_t control_id, int value) {
    return camera_controls_->SetControl(state_.current_camera_path, control_id, value);
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

void CameraManager::UpdateFPSOptions(const std::string& cam_path, int width, int height) {
    ui_fps_opts_ = camera_enumeration_->UpdateFPSOptions(cam_path, width, height);
    
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
    
    GStreamerObjects gst_objects = {buffer, sample};
    ConversionInput input = {width, height, format, stride_hint};
    ConversionOutput output = {frame_out, width_out, height_out};
    
    return PerformConversionAndCleanup(gst_objects, input, output);
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

bool CameraManager::PerformConversionAndCleanup(const GStreamerObjects& gst_objects, const ConversionInput& input,
                                               ConversionOutput& output) {
    GstMapInfo map_info = {};
    if (!MapAndValidateBuffer(gst_objects.buffer, map_info)) {
        gst_sample_unref(gst_objects.sample);
        return false;
    }

    cv::Mat converted;
    BufferInfo actual_buffer_info = {map_info.data, map_info.size};
    bool success = ConvertBufferToBgrWithValidation(actual_buffer_info, input.width, input.height, input.format, input.stride_hint, converted);

    LogConversionResult(input.format, input.width, input.height, input.stride_hint, map_info.size, success, 
                       converted.empty() ? 0 : converted.channels());

    gst_buffer_unmap(gst_objects.buffer, &map_info);
    gst_sample_unref(gst_objects.sample);

    if (!success) {
        std::cerr << "⚠️  Failed to convert PipeWire sample to BGR (format=" << input.format 
                  << ", width=" << input.width << ", height=" << input.height << ", stride_hint=" << input.stride_hint << ")" << std::endl;
        return false;
    }

    output.frame_out = std::move(converted);
    output.width_out = input.width;
    output.height_out = input.height;
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
