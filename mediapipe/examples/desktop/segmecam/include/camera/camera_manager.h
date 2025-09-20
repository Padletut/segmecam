#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <opencv2/opencv.hpp>
#include <linux/videodev2.h>
#include "cam_enum.h"

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

// GStreamer function pointer types
typedef void (*gst_init_func)(int*, char***);
typedef GstElement* (*gst_pipeline_new_func)(const char*);
typedef GstElement* (*gst_element_factory_make_func)(const char*, const char*);
typedef int (*gst_element_set_state_func)(GstElement*, int);
typedef int (*gst_bin_add_many_func)(void*, ...);
typedef int (*gst_element_link_many_func)(void*, ...);
typedef void (*gst_object_unref_func)(void*);
typedef void* (*gst_app_sink_pull_sample_func)(GstAppSink*);
typedef void* (*gst_sample_get_buffer_func)(GstSample*);
typedef void* (*gst_sample_get_caps_func)(GstSample*);
typedef int (*gst_buffer_map_func)(GstBuffer*, GstMapInfo*, int);
typedef void (*gst_buffer_unmap_func)(GstBuffer*, GstMapInfo*);
typedef void (*gst_sample_unref_func)(GstSample*);

// Additional GStreamer function pointer types
typedef GstCaps* (*gst_caps_new_simple_func)(const char*, ...);
typedef GstCaps* (*gst_caps_from_string_func)(const char*);
typedef void (*gst_caps_unref_func)(GstCaps*);
typedef void (*gst_message_parse_error_func)(void*, void**, void**);
typedef void* (*gst_bus_timed_pop_filtered_func)(void*, uint64_t, int);
typedef void* (*gst_element_get_bus_func)(GstElement*);
typedef void (*gst_bus_unref_func)(void*);
typedef void (*gst_message_unref_func)(void*);
typedef int (*gst_app_sink_is_eos_func)(GstAppSink*);
typedef GstElement* (*gst_parse_launch_func)(const char*, void**);
typedef GstElement* (*gst_bin_get_by_name_func)(void*, const char*);
typedef int (*gst_element_get_state_func)(GstElement*, int*, int*, uint64_t);
typedef const char* (*gst_structure_get_string_func)(const GstStructure*, const char*);
typedef void (*g_error_free_func)(void*);
typedef void* (*gst_element_get_bus_func)(GstElement*);
typedef void (*gst_bus_unref_func)(void*);
typedef void (*gst_message_unref_func)(void*);
typedef int (*gst_app_sink_is_eos_func)(GstAppSink*);
typedef GstElement* (*gst_parse_launch_func)(const char*, void**);
typedef GstElement* (*gst_bin_get_by_name_func)(void*, const char*);
typedef int (*gst_element_get_state_func)(GstElement*, int*, int*, uint64_t);
typedef void (*g_usleep_func)(unsigned long);
typedef void (*g_error_free_func)(void*);

// GLib function pointer types
typedef void (*g_main_loop_quit_func)(GMainLoop*);
typedef void (*g_main_loop_unref_func)(GMainLoop*);
typedef void (*g_object_set_func)(void*, const char*, ...);
typedef unsigned long (*g_signal_connect_func)(void*, const char*, void*, void*);

typedef GMainLoop* (*g_main_loop_new_func)(void*, gboolean);
typedef void (*g_main_loop_run_func)(GMainLoop*);
typedef void (*g_object_unref_func)(void*);

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

namespace segmecam {

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

// State tracking for camera system
struct CameraState {
    bool is_initialized = false;
    bool is_opened = false;
    std::string current_camera_path;
    int current_width = 0;
    int current_height = 0;
    int current_fps = 0;
    
    // UI state
    int ui_cam_idx = 0;
    int ui_res_idx = 0;
    int ui_fps_idx = 0;
    
    // Camera backend info
    std::string backend_name;
    
    // Performance tracking
    double actual_fps = 0.0;
    int frames_captured = 0;

    // User-facing status
    std::string status_message;
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
    
    // Camera operations
    bool OpenCamera(int camera_index);
    bool OpenCamera(int camera_index, int width, int height, int fps = -1);
    void CloseCamera();
    bool IsOpened() const;
    
    // Frame capture
    bool CaptureFrame(cv::Mat& frame);
    
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
    const CtrlRange& GetBrightnessRange() const { return r_brightness_; }
    const CtrlRange& GetContrastRange() const { return r_contrast_; }
    const CtrlRange& GetSaturationRange() const { return r_saturation_; }
    const CtrlRange& GetGainRange() const { return r_gain_; }
    const CtrlRange& GetSharpnessRange() const { return r_sharpness_; }
    const CtrlRange& GetZoomRange() const { return r_zoom_; }
    const CtrlRange& GetFocusRange() const { return r_focus_; }
    const CtrlRange& GetAutoGainRange() const { return r_autogain_; }
    const CtrlRange& GetAutoFocusRange() const { return r_autofocus_; }
    const CtrlRange& GetAutoExposureRange() const { return r_autoexposure_; }
    const CtrlRange& GetExposureRange() const { return r_exposure_abs_; }
    const CtrlRange& GetWhiteBalanceRange() const { return r_awb_; }
    const CtrlRange& GetWhiteBalanceTemperatureRange() const { return r_wb_temp_; }
    const CtrlRange& GetBacklightCompensationRange() const { return r_backlight_; }
    const CtrlRange& GetExposureDynamicFPSRange() const { return r_expo_dynfps_; }
    
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
    // Configuration and state
    CameraConfig config_;
    CameraState state_;
    
    // Camera enumeration
    std::vector<CameraDesc> cam_list_;
    std::vector<LoopbackDesc> vcam_list_;
    std::vector<int> ui_fps_opts_;
    
    // OpenCV capture
    cv::VideoCapture cap_;
    
    // V4L2 control ranges
    CtrlRange r_brightness_, r_contrast_, r_saturation_, r_gain_;
    CtrlRange r_sharpness_, r_zoom_, r_focus_;
    CtrlRange r_autogain_, r_autofocus_;
    CtrlRange r_autoexposure_, r_exposure_abs_;
    CtrlRange r_awb_, r_wb_temp_, r_backlight_, r_expo_dynfps_;

    // PipeWire/GStreamer specific members
    GstElement* pipeline_ = nullptr;
    GstElement* appsink_ = nullptr;
    GstElement* pipewire_src_ = nullptr;
    GMainLoop* main_loop_ = nullptr;
    bool gst_initialized_ = false;
    bool camera_permission_granted_ = false;
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
    void QueryCtrl(const std::string& cam_path, uint32_t id, CtrlRange* out);
    bool SetCtrl(const std::string& cam_path, uint32_t id, int32_t value);
    bool ConvertSampleToBgr(GstSample* sample, cv::Mat& frame_out, int& width_out, int& height_out);
    bool GetCtrl(const std::string& cam_path, uint32_t id, int32_t* value);
    void UpdateFPSOptions(const std::string& cam_path, int width, int height);

    // PipeWire/GStreamer specific methods
    bool InitializeGStreamer();
    void CleanupGStreamer();
    bool RequestCameraPermission();
    int OpenPipeWireRemote();
    bool CreatePipeWirePipeline(int width, int height, int fps);
    bool StartPipeWireCapture(int width, int height, int fps);
    void StopPipeWireCapture();
    void OnNewSample(GstAppSink* sink);
    void OnEOS(GstAppSink* sink);
    static void OnNewSampleWrapper(GstAppSink* sink, gpointer user_data);
    static void OnEOSWrapper(GstAppSink* sink, gpointer user_data);
    static void OnPortalCameraAccessFinished(GObject* source, GAsyncResult* result, gpointer user_data);
    
    // Direct GStreamer camera capture methods (Flatpak fallback)
    bool OpenGStreamerCamera(int camera_index, int width, int height, int fps);
    void CloseGStreamerCamera();
    bool CaptureGStreamerFrame(cv::Mat& frame);
};

} // namespace segmecam
