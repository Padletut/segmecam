#pragma once

#include "mediapipe/framework/port/opencv_core_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include <string>
#include <vector>
#include <cstdint>
#include "include/camera/vcam.h"
#include "mediapipe/framework/formats/classification.pb.h"
#include "effects/advanced_skin_effects.h"

namespace segmecam {

struct AppState {
  // Display and processing state
  bool show_mask = false;
  int blur_strength = 25;
  float feather_px = 2.0f;
  int64_t frame_id = 0;
  bool dbg_composite_rgb = false;
  
  // OpenCL acceleration  
  bool use_opencl = true; // Enable by default if available
  bool opencl_available = false;
  
  // Performance logging
  bool perf_log = false;
  int perf_log_interval_ms = 5000;
  uint32_t perf_last_log_ms = 0;
  double perf_sum_frame_ms = 0.0;
  double perf_sum_smooth_ms = 0.0;
  double perf_sum_bg_ms = 0.0;
  uint32_t perf_sum_frames = 0;
  bool perf_logged_caps = false;
  
  // Auto processing scale
  bool auto_processing_scale = true; // Enabled by default - now stable with conservative adjustments
  float target_fps = 14.5f;
  float current_fps = 0.0f;
  float auto_scale_last_adjustment = 0.0f;
  uint32_t auto_scale_last_time_ms = 0;
  bool auto_scale_enabled = false;
  bool vcam_auto_start = false;
  
  // Background mode: 0=None, 1=Blur, 2=Image, 3=Solid Color
  int bg_mode = 0; // Default to no background effect
  cv::Mat bg_image; // background image (BGR)
  char bg_path_buf[512] = {0};
  float solid_color[3] = {0.0f, 0.0f, 0.0f}; // RGB 0..1
  
  // Cached data
  cv::Mat last_mask_u8;  // cache latest mask to avoid blocking
  cv::Mat last_display_rgb;
  
  // Latest blendshapes for facial expression analysis
  std::unique_ptr<mediapipe::ClassificationList> last_blendshapes;
  
  // Latest facial expression metrics for debug display
  FacialExpressionMetrics last_facial_metrics;
  
  // Beauty controls
  bool fx_skin = false;
  float fx_skin_strength = 0.4f;
  bool fx_skin_adv = true;
  float fx_skin_amount = 0.5f;
  float fx_skin_radius = 6.0f;
  float fx_skin_tex = 0.35f;
  float fx_skin_edge = 12.0f;
  
  // Advanced processing scale
  float fx_adv_scale = 0.8f; // 0.5..1.0 - Default to 0.8 for better performance
  float fx_adv_detail_preserve = 0.18f; // 0..0.5, re-inject hi-freq after upsample
  
  // Wrinkle reduction
  bool fx_skin_wrinkle = false;
  float fx_skin_smile_boost = 0.6f;
  float fx_skin_squint_boost = 0.5f;
  float fx_skin_forehead_boost = 0.8f;
  float fx_skin_wrinkle_gain = 0.3f;
  float fx_skin_smile_wrinkle_gain = 1.0f;
  bool fx_wrinkle_suppress_lower = false;
  float fx_wrinkle_lower_ratio = 0.45f;
  bool fx_wrinkle_ignore_glasses = false;
  float fx_wrinkle_glasses_margin = 12.0f;
  float fx_wrinkle_keep_ratio = 0.35f;
  bool fx_wrinkle_custom_scales = false;
  float fx_wrinkle_min_px = 2.0f;
  float fx_wrinkle_max_px = 8.0f;
  bool fx_wrinkle_preview = false;
  bool fx_wrinkle_use_skin_gate = false;
  float fx_wrinkle_mask_gain = 2.0f;
  float fx_wrinkle_baseline = 0.5f;
  float fx_wrinkle_neg_cap = 0.9f;
  
  // Lip effects
  bool fx_lipstick = false;
  float fx_lip_alpha = 0.5f;
  float fx_lip_color[3] = {0.8f, 0.1f, 0.3f};
  float fx_lip_feather = 6.0f;
  float fx_lip_light = 0.0f;
  float fx_lip_band = 4.0f;
  
  // Teeth whitening
  bool fx_teeth = false;
  float fx_teeth_strength = 0.5f;
  float fx_teeth_margin = 3.0f;
  
  // Landmark and debug overlay display
  bool show_landmarks = false;
  bool show_mesh = false;
  bool show_mesh_dense = false;
  bool show_facemask = false;
  bool show_blendshapes_debug = false; // Show blendshape debug overlay
  bool lm_roi_mode = false;
  bool lm_apply_rot = true;
  bool lm_flip_x = false;
  bool lm_flip_y = false;
  bool lm_swap_xy = false;
  
  // FPS tracking
  double fps = 0.0;
  uint64_t fps_frames = 0;
  uint32_t fps_last_ms = 0;
  uint32_t dbg_last_ms = 0;
  
  // Control flags
  bool first_frame_log = false;
  bool first_mask_log = false;
  bool first_mask_info = false;
  bool running = true;
  bool vsync_on = true;
  
  // Camera info for status display
  int camera_width = 0;
  int camera_height = 0;
  int camera_fps = 0;
  std::string camera_status_message;
  
  // Virtual camera
  segmecam::VCam vcam;
  int ui_vcam_idx = 0;
  std::string virtual_camera_path = "/dev/video20"; // Default path
  
  // PipeWire output status
  bool pipewire_output_active = false;
  
  // Profile management
  int ui_profile_idx = -1;
  char profile_name_buf[128] = {0};
  
  // Save/load state to/from profile
  void SaveToProfile(cv::FileStorage& fs) const;
  void LoadFromProfile(const cv::FileNode& root);
  
  // Helper methods for loading specific setting categories
  void LoadDisplaySettings(const cv::FileNode& root);
  void LoadBackgroundSettings(const cv::FileNode& root);
  void LoadLandmarkSettings(const cv::FileNode& root);
  void LoadMeshSettings(const cv::FileNode& root);
  void LoadSkinEffectSettings(const cv::FileNode& root);
  void LoadPerformanceSettings(const cv::FileNode& root);
  void LoadWrinkleSettings(const cv::FileNode& root);
  void LoadLipEffectSettings(const cv::FileNode& root);
  void LoadTeethSettings(const cv::FileNode& root);
  
  // Shared background image loading function
  bool LoadBackgroundImageFromPath(const std::string& path);
  
private:
  int ReadInt(const cv::FileNode& n, int def) const;
  float ReadFloat(const cv::FileNode& n, float def) const;
};

} // namespace segmecam
