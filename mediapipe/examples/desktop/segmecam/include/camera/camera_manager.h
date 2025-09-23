#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <opencv2/opencv.hpp>
#include <linux/videodev2.h>
#include "include/camera/cam_enum.h"
#include "gstreamer_buffer_utils.h"
#include "camera_controls.h"
#include "gstreamer_utils.h"

// Forward declarations for GStreamer types (to avoid header dependencies)
#ifdef __cplusplus
extern "C" {
#endif
typedef struct _GstElement GstElement;
typedef struct _GstAppSink GstAppSink;
typedef struct _GstBin GstBin;
typedef struct _GMainLoop GMainLoop;
typedef struct _GstSample GstSample;
typedef struct _GstBuffer GstBuffer;
typedef struct _GstCaps GstCaps;
typedef struct _GstBus GstBus;
typedef struct _GstMessage GstMessage;
typedef struct _GError GError;
typedef struct _GstStructure GstStructure;
typedef struct _GstMemory GstMemory;
typedef int gboolean;
typedef struct _XdpPortal XdpPortal;
typedef struct _XdpParent XdpParent;

// GStreamer state enums
typedef enum {
    GST_STATE_VOID_PENDING = 0,
    GST_STATE_NULL = 1,
    GST_STATE_READY = 2,
    GST_STATE_PAUSED = 3,
    GST_STATE_PLAYING = 4
} GstState;

typedef enum {
    GST_STATE_CHANGE_FAILURE = 0,
    GST_STATE_CHANGE_SUCCESS = 1,
    GST_STATE_CHANGE_ASYNC = 2,
    GST_STATE_CHANGE_NO_PREROLL = 3
} GstStateChangeReturn;

typedef int GstMapFlags;

#define GST_PADDING 4

typedef struct _GstMapInfo {
    GstMemory* memory;
    GstMapFlags flags;
    unsigned char* data;
    size_t size;
    size_t maxsize;
    void* user_data[4];
    void* _gst_reserved[GST_PADDING];
} GstMapInfo;
typedef void* gpointer;

// GLib forward declarations
typedef struct _GObject GObject;
typedef struct _GAsyncResult GAsyncResult;
typedef struct _GCancellable GCancellable;
typedef void (*GAsyncReadyCallback)(GObject*, GAsyncResult*, gpointer);
typedef unsigned int XdpCameraFlags;
#ifdef __cplusplus
}
#endif

// Forward declarations for GStreamer types (to avoid header dependencies)
#ifdef __cplusplus
extern "C" {
#endif

// GStreamer constants
#define GST_STATE_NULL 0
#define GST_STATE_PLAYING 4
#define GST_MAP_READ 1
#define GST_STATE_CHANGE_FAILURE -1
// Additional constants/macros used without headers
#define GST_SECOND 1000000000ULL
#define G_OBJECT(obj) ((void*)(obj))
#define GST_ELEMENT(obj) ((GstElement*)(obj))
#define GST_APP_SINK(obj) ((GstAppSink*)(obj))

#ifdef __cplusplus
}
#endif

namespace segmecam {

// Forward declarations for modular components
class CameraEnumeration;
class CameraSetup;

// Configuration for camera system
struct CameraConfig {
    int default_camera_index = 0;
    int default_width = 0;
    int default_height = 0;
    int default_fps = 30;
    bool prefer_v4l2 = true;
    bool enable_auto_focus = true;
    bool enable_auto_gain = true;
    bool enable_auto_exposure = true;
};

// Parameters for camera opening operations
struct CameraOpeningParams {
    int camera_index;
    int width;
    int height;
    int fps;
};

// State tracking for camera system
struct CameraState {
    bool is_initialized = false;
    bool is_opened = false;
    [[maybe_unused]] std::string current_camera_path;
    int current_width = 0;
    int current_height = 0;
    int current_fps = 0;
    
    // UI state
    int ui_cam_idx = 0;
    int ui_res_idx = 0;
    int ui_fps_idx = 0;
    
    // Camera backend info
    [[maybe_unused]] std::string backend_name;
    
    // Performance tracking
    double actual_fps = 0.0;
    int frames_captured = 0;

    // User-facing status
    std::string status_message;
}; 

#include "cam_enum.h"
#include "gstreamer_buffer_utils.h"

// Portal request context for async camera permission operations
struct PortalRequestContext {
    class CameraManager* self = nullptr;
    GMainLoop* loop = nullptr;
    bool success = false;
};

// Parameter structs for PerformConversionAndCleanup method
struct GStreamerObjects {
    GstBuffer* buffer;
    GstSample* sample;
};

struct ConversionInput {
    int width;
    int height;
    std::string format;
    int stride_hint;
};

struct ConversionOutput {
    cv::Mat& frame_out;
    int& width_out;
    int& height_out;
};

// Camera system manager for initialization, enumeration, capture, and V4L2 controls
class CameraManager {
public:
    CameraManager();
    ~CameraManager();
    
    // Core lifecycle
    int Initialize(const CameraConfig& config);
    int InitializeV4L2(const CameraConfig& config);
    bool InitializePortal();
    std::vector<CameraDesc> EnumerateCamerasPortal();
    void Cleanup();
    void CleanupGStreamer();
    
    // Camera operations
    bool OpenCamera(int camera_index);
    bool OpenCamera(int camera_index, int width, int height, int fps = -1);
    void CloseCamera();
    bool IsOpened() const;
    
    // Frame capture
    bool CaptureFrame(cv::Mat& frame);
    bool CaptureFrameFlatpak(cv::Mat& frame);
    bool CaptureFrameNative(cv::Mat& frame);
    bool CaptureV4L2Frame(cv::Mat& frame);
    bool CaptureV4L2FrameInternal(cv::Mat& frame);
    
    // Camera enumeration and selection
    const std::vector<CameraDesc>& GetCameraList() const { return cam_list_; }
    void RefreshCameraList();
    bool SetCurrentCamera(int ui_cam_idx, int ui_res_idx, int ui_fps_idx);
    
    // Virtual camera (v4l2loopback) support
    const std::vector<LoopbackDesc>& GetVCamList() const { return vcam_list_; }
    void RefreshVCamList();
    
    // Resolution and FPS management
    const std::vector<std::pair<int,int>>& GetCurrentResolutions() const;
    const std::vector<int>& GetCurrentFPSOptions() const { return ui_fps_opts_; }
    bool SetResolution(int width, int height);
    bool SetFPS(int fps);
    
    // V4L2 camera controls
    void RefreshControls();
    void ApplyDefaultControls();
    
    // Individual control access
    bool SetBrightness(int value);
    bool SetContrast(int value);
    bool SetSaturation(int value);
    bool SetGain(int value);
    bool SetSharpness(int value);
    bool SetZoom(int value);
    bool SetFocus(int value);
    bool SetAutoGain(bool enabled);
    bool SetAutoFocus(bool enabled);
    bool SetAutoExposure(bool enabled);
    bool SetExposure(int value);
    bool SetWhiteBalance(bool auto_enabled);
    bool SetWhiteBalanceTemperature(int value);
    bool SetBacklightCompensation(int value);
    
    // Generic control method for V4L2 controls
    bool SetControl(uint32_t control_id, int value);
    
    // Control ranges (for UI sliders)
    const CtrlRange& GetBrightnessRange() const { return camera_controls_->GetBrightnessRange(); }
    const CtrlRange& GetContrastRange() const { return camera_controls_->GetContrastRange(); }
    const CtrlRange& GetSaturationRange() const { return camera_controls_->GetSaturationRange(); }
    const CtrlRange& GetGainRange() const { return camera_controls_->GetGainRange(); }
    const CtrlRange& GetSharpnessRange() const { return camera_controls_->GetSharpnessRange(); }
    const CtrlRange& GetZoomRange() const { return camera_controls_->GetZoomRange(); }
    const CtrlRange& GetFocusRange() const { return camera_controls_->GetFocusRange(); }
    const CtrlRange& GetAutoGainRange() const { return camera_controls_->GetAutoGainRange(); }
    const CtrlRange& GetAutoFocusRange() const { return camera_controls_->GetAutoFocusRange(); }
    const CtrlRange& GetAutoExposureRange() const { return camera_controls_->GetAutoExposureRange(); }
    const CtrlRange& GetExposureRange() const { return camera_controls_->GetExposureAbsRange(); }
    const CtrlRange& GetWhiteBalanceRange() const { return camera_controls_->GetAutoWhiteBalanceRange(); }
    const CtrlRange& GetWhiteBalanceTemperatureRange() const { return camera_controls_->GetWhiteBalanceTempRange(); }
    const CtrlRange& GetBacklightCompensationRange() const { return camera_controls_->GetBacklightRange(); }
    const CtrlRange& GetExposureDynamicFPSRange() const { return camera_controls_->GetExposureDynamicFPSRange(); }
    
    // State access
    const CameraState& GetState() const { return state_; }
    const CameraConfig& GetConfig() const { return config_; }
    
    // UI indices access (for compatibility with existing UI code)
    int GetUICameraIndex() const { return state_.ui_cam_idx; }
    int GetUIResolutionIndex() const { return state_.ui_res_idx; }
    int GetUIFPSIndex() const { return state_.ui_fps_idx; }
    
    // Backend information
    std::string GetBackendName() const;
    double GetActualFPS() const;
    void UpdatePerformanceStats();
    
    // Current camera settings
    int GetCurrentWidth() const { return state_.current_width; }
    int GetCurrentHeight() const { return state_.current_height; }
    int GetCurrentFPS() const { return state_.current_fps; }

private:
    // Helper methods
    void LogV4L2InitializationSuccess();

    // Frame capture helpers
    bool WaitForPipeWireFrame();
    bool ValidateAndCopyFrame(cv::Mat& frame);
    bool ValidateNativeFrame(cv::Mat& frame);
    bool PerformFrameValidation(const cv::Mat& frame);
    bool ValidateFrameChannels(const cv::Mat& frame);
    void LogWaitStart();
    bool ShouldContinueWaiting();
    bool IsTimeoutExpired(const std::chrono::steady_clock::time_point& deadline);

    // V4L2 capture helpers
    bool ValidateV4L2Initialization() const;
    bool CheckV4L2PipelineState() const;
    GstSample* PullV4L2Sample() const;
    bool ExtractV4L2Buffer(GstSample* sample, GstBuffer*& buffer, GstMapInfo& map_info) const;
    bool CreateFrameFromV4L2Buffer(GstSample* sample, const GstMapInfo& map_info, cv::Mat& frame);
    bool InitializeV4L2Fallback();

    // Configuration and state
    CameraConfig config_;
    CameraState state_;
    
    // Camera enumeration
    std::vector<CameraDesc> cam_list_;
    std::vector<LoopbackDesc> vcam_list_;
    std::vector<int> ui_fps_opts_;
    
    // New modular components
    std::unique_ptr<CameraControls> camera_controls_;
    std::unique_ptr<CameraEnumeration> camera_enumeration_;
    std::unique_ptr<CameraSetup> camera_setup_;
    
    // OpenCV capture
    cv::VideoCapture cap_;
    
    // PipeWire/GStreamer specific members
    GstElement* pipeline_ = nullptr;
    GstElement* appsink_ = nullptr;
    GstElement* pipewire_src_ = nullptr;
    GMainLoop* main_loop_ = nullptr;
    bool gst_initialized_ = false;
    bool camera_permission_granted_ = false;
    bool using_v4l2_source_ = false;  // Track if we're using V4L2 instead of PipeWire
    int pipewire_failure_count_ = 0;  // Track consecutive PipeWire failures
    XdpPortal* portal_instance_ = nullptr;
    void* portal_library_handle_ = nullptr;
    int portal_fd_ = -1;
    cv::Mat current_frame_;
    std::mutex frame_mutex_;
    std::condition_variable frame_ready_cv_;
    bool frame_ready_ = false;
    
    // Direct GStreamer camera capture (fallback for Flatpak)
    GstElement* gst_pipeline_ = nullptr;
    GstAppSink* gst_appsink_ = nullptr;
    bool gst_camera_active_ = false;
    
    // GStreamer function pointers
    gst_init_func gst_init = nullptr;
    gst_pipeline_new_func gst_pipeline_new = nullptr;
    gst_element_factory_make_func gst_element_factory_make = nullptr;
    gst_element_set_state_func gst_element_set_state = nullptr;
    gst_bin_add_many_func gst_bin_add_many = nullptr;
    gst_element_link_many_func gst_element_link_many = nullptr;
    gst_object_unref_func gst_object_unref = nullptr;
    gst_app_sink_pull_sample_func gst_app_sink_pull_sample = nullptr;
    gst_sample_get_buffer_func gst_sample_get_buffer = nullptr;
    gst_sample_get_caps_func gst_sample_get_caps = nullptr;
    gst_buffer_map_func gst_buffer_map = nullptr;
    gst_buffer_unmap_func gst_buffer_unmap = nullptr;
    gst_sample_unref_func gst_sample_unref = nullptr;
    
    // Additional GStreamer function pointers
    gst_caps_new_simple_func gst_caps_new_simple = nullptr;
    gst_caps_from_string_func gst_caps_from_string = nullptr;
    gst_caps_unref_func gst_caps_unref = nullptr;
    GstStructure* (*gst_caps_get_structure)(GstCaps*, unsigned int) = nullptr;
    int (*gst_structure_get_int)(const GstStructure*, const char*, int*) = nullptr;
    gst_structure_get_string_func gst_structure_get_string = nullptr;
    gst_message_parse_error_func gst_message_parse_error = nullptr;
    gst_bus_timed_pop_filtered_func gst_bus_timed_pop_filtered = nullptr;
    gst_element_get_bus_func gst_element_get_bus = nullptr;
    gst_bus_unref_func gst_bus_unref = nullptr;
    gst_message_unref_func gst_message_unref = nullptr;
    gst_app_sink_is_eos_func gst_app_sink_is_eos = nullptr;
    gst_parse_launch_func gst_parse_launch = nullptr;
    gst_bin_get_by_name_func gst_bin_get_by_name = nullptr;
    gst_element_get_state_func gst_element_get_state = nullptr;
    // Linking helpers
    int (*gst_element_link)(void*, void*) = nullptr;
    int (*gst_element_link_filtered)(void*, void*, GstCaps*) = nullptr;
    g_usleep_func g_usleep = nullptr;
    g_error_free_func g_error_free = nullptr;
    g_object_set_func g_object_set = nullptr;
    g_main_loop_new_func g_main_loop_new = nullptr;
    g_main_loop_run_func g_main_loop_run = nullptr;
    g_object_unref_func g_object_unref_ptr = nullptr;

    // Portal function pointers
    XdpPortal* (*xdp_portal_new)(void) = nullptr;
    gboolean (*xdp_portal_is_camera_present)(XdpPortal*) = nullptr;
    void (*xdp_portal_access_camera)(XdpPortal*, XdpParent*, XdpCameraFlags, GCancellable*, GAsyncReadyCallback, gpointer) = nullptr;
    gboolean (*xdp_portal_access_camera_finish)(XdpPortal*, GAsyncResult*, GError**) = nullptr;
    int (*xdp_portal_open_pipewire_remote_for_camera)(XdpPortal*) = nullptr;

    // GLib function pointers
    g_main_loop_quit_func g_main_loop_quit = nullptr;
    g_main_loop_unref_func g_main_loop_unref = nullptr;
    g_signal_connect_func g_signal_connect = nullptr;
    
    // Helper methods
    cv::VideoCapture OpenCapture(int idx, int w, int h);
    bool ConvertSampleToBgr(GstSample* sample, cv::Mat& frame_out, int& width_out, int& height_out);
    void UpdateFPSOptions(const std::string& cam_path, int width, int height);

    // PipeWire initialization helpers
    bool TryInitializePipeWire();
    void SetupPipeWireDefaults();
    void SetupPipeWireUIState();
    void SetupPipeWireCameraSelection();
    void SetupPipeWireResolution();
    void SetupPipeWireFPS();
    bool PopulateUIStateForPipeWire();
    void SelectOptimalResolution();
    void SelectOptimalFPS();

    // V4L2 initialization helpers
    bool TryEnumerateCameras();
    void SetupInitialResolution(const CameraConfig& config);
    int InitializeCameraAndControls(const CameraConfig& config);

    // PipeWire/GStreamer specific methods
    bool InitializeGStreamer();
    bool LoadRequiredLibraries(void*& gst_lib, void*& gstapp_lib, void*& gstvideo_lib, void*& glib_lib, void*& gobject_lib);
    bool LoadGStreamerCoreFunctions(void* gst_lib);
    bool LoadGStreamerAppFunctions(void* gstapp_lib);
    bool LoadGLibFunctions(void* glib_lib, void* gobject_lib);
    bool ValidateFunctionLoading();
    
    // ConvertSampleToBgr helper methods
    bool ParseCapsStructure(GstCaps* caps, int& width, int& height, int& stride_hint, std::string& format);
    bool MapAndValidateBuffer(GstBuffer* buffer, GstMapInfo& map_info);
    bool ConvertBufferToBgrWithValidation(const BufferInfo& buffer_info, int width, int height, 
                                         const std::string& format, int stride_hint, cv::Mat& output);
    bool ValidateGStreamerFunctions();
    bool ExtractSampleComponents(GstSample* sample, GstBuffer*& buffer, GstCaps*& caps);
    void InitializeConversionParameters(int& width, int& height, int& stride_hint, std::string& format, 
                                       int width_out, int height_out);
    void LogConversionResult(const std::string& format, int width, int height, int stride_hint, 
                            size_t map_size, bool success, int channels);
    bool PerformConversionAndCleanup(const GStreamerObjects& gst_objects, const ConversionInput& input,
                                    ConversionOutput& output);
    // ParseCapsStructure helper methods
    bool GetStructureFromCaps(GstCaps* caps, GstStructure*& structure);
    bool ExtractIntFromStructure(GstStructure* structure, const char* field_name, int& value);
    bool ExtractStringFromStructure(GstStructure* structure, const char* field_name, std::string& value);

    // OnPortalCameraAccessFinished helper methods
    bool ProcessPortalAccessResult(GAsyncResult* result, bool& granted);
    bool OpenPipeWireRemote(bool& granted);
    void HandlePortalError(GError* error);
    void CleanupPortalRequestContext(PortalRequestContext* ctx);
    
    // Helper methods for function loading
    template<typename T>
    bool LoadFunctionPointer(T*& func_ptr, void* library, const char* symbol_name, const char* func_name);
    bool ValidateFunctionPointers(const std::vector<std::pair<void*, const char*>>& functions_to_check);
    bool RequestCameraPermission();
    // RequestCameraPermission helper methods
    bool LoadPortalLibrary();
    bool LoadPortalFunctions();
    bool ValidatePortalFunctions();
    bool CreatePortalInstance();
    bool SetupAsyncRequest(PortalRequestContext& ctx);
    bool RunMainLoop(PortalRequestContext& ctx);
    bool HandlePermissionResult(const PortalRequestContext& ctx);
    int OpenPipeWireRemote();
    bool CreatePipeWirePipeline(int width, int height, int fps);
    bool StartPipeWireCapture(int width, int height, int fps);
    void StopPipeWireCapture();
    
    // Helper methods for PipeWire pipeline management
    bool CreatePipelineFromDescription(const char* pipeline_desc);
    bool RetrievePipelineElements();
    void ConfigureAppSink();
    void ConnectPipelineSignals();
    
    // Helper methods for PipeWire capture startup
    bool EnsureCameraPermission();
    void CalculateTargetDimensions(int width, int height, int fps, int& target_width, int& target_height, int& target_fps);
    bool ValidatePortalConnection();
    void CleanupExistingPipeline();
    bool SetupAndStartPipeline(int target_width, int target_height, int target_fps);
    void ConfigurePipeWireSource();
    void ConfigurePipeWireSourceForNative();
    bool StartPipeline();
    void HandlePipelineStartFailure();
    void UpdateCameraState(int target_width, int target_height, int target_fps);
    
    void OnNewSample(GstAppSink* sink);
    void OnEOS(GstAppSink* sink);
    static void OnNewSampleWrapper(GstAppSink* sink, gpointer user_data);
    static void OnEOSWrapper(GstAppSink* sink, gpointer user_data);
    static void OnPortalCameraAccessFinished(GObject* source, GAsyncResult* result, gpointer user_data);
    
    // Direct GStreamer camera capture methods (Flatpak fallback)
    bool OpenGStreamerCamera(int camera_index, int width, int height, int fps);
    bool TryOpenPipeWireCamera(int camera_index, int width, int height, int fps);
    bool TryOpenV4L2Camera(int camera_index, int width, int height, int fps);
    void CleanupPipeline();
    void SetCameraState(int width, int height, int fps, const std::string& backend_name);
    void CloseGStreamerCamera();
    bool CaptureGStreamerFrame(cv::Mat& frame);
    bool ValidateGStreamerPipeline();
    
    // PipeWire camera helper methods
    std::string CreatePipeWirePipelineString(int pipewire_node_id);
    bool CreateAndValidatePipeWirePipeline(const std::string& pipeline_str);
    bool ConfigurePipeWireAppSink();
    bool StartAndValidatePipeWirePipeline();
    
    // OpenCamera helper methods
    void LogCameraOpening(int camera_index, int width, int height, int fps);
    CameraOpeningParams PrepareCameraParameters(int camera_index, int width, int height, int fps);
    bool OpenCameraInFlatpak(const CameraOpeningParams& params);
    bool OpenCameraNatively(const CameraOpeningParams& params);
    bool TryOpenPipeWireCapture(int width, int height, int fps);
    bool TryOpenOpenCVFallback(int camera_index, int width, int height, int fps);
    bool TryOpenNativeCamera(int camera_index, int width, int height, int fps);
    void ConfigureCameraProperties(int width, int height, int fps);
    void UpdateCameraStateFromCapture(const std::string& backend_name);
    
    // PipeWire utility functions
    static std::vector<int> EnumeratePipeWireCameraNodes();
    static int GetPipeWireNodeIdForCamera(int camera_index);
};

} // namespace segmecam
