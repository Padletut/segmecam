#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <functional>
#include "cam_enum.h"

namespace segmecam {

class CameraManager {
public:
  CameraManager();
  ~CameraManager() = default;

  // Initialize camera system
  bool Initialize(int cam_index);
  
  // Camera enumeration and selection
  const std::vector<CameraDesc>& GetCameraList() const { return cam_list_; }
  void SetCurrentCamera(int ui_cam_idx, int ui_res_idx, int ui_fps_idx);
  
  // Camera capture
  cv::VideoCapture& GetCapture() { return cap_; }
  bool IsOpened() const { return cap_.isOpened(); }
  
  // Camera controls
  void RefreshControls();
  void ApplyDefaultControls();
  
  // Profile management
  bool SaveProfile(const std::string& name, const std::function<void(cv::FileStorage&)>& save_callback);
  bool LoadProfile(const std::string& name, const std::function<void(const cv::FileNode&)>& load_callback);
  std::vector<std::string> ListProfiles();
  
  // Virtual camera management
  const std::vector<LoopbackDesc>& GetVCamList() const { return vcam_list_; }
  
  // Getters
  int GetCurrentCamIndex() const { return ui_cam_idx_; }
  int GetCurrentResIndex() const { return ui_res_idx_; }
  int GetCurrentFpsIndex() const { return ui_fps_idx_; }
  const std::string& GetCurrentCamPath() const { return current_cam_path_; }
  const std::vector<int>& GetFpsOptions() const { return ui_fps_opts_; }
  
  // Control ranges
  const CtrlRange& GetBrightnessRange() const { return r_brightness_; }
  const CtrlRange& GetContrastRange() const { return r_contrast_; }
  const CtrlRange& GetSaturationRange() const { return r_saturation_; }
  const CtrlRange& GetGainRange() const { return r_gain_; }
  const CtrlRange& GetSharpnessRange() const { return r_sharpness_; }
  const CtrlRange& GetZoomRange() const { return r_zoom_; }
  const CtrlRange& GetFocusRange() const { return r_focus_; }
  const CtrlRange& GetAutogainRange() const { return r_autogain_; }
  const CtrlRange& GetAutofocusRange() const { return r_autofocus_; }
  const CtrlRange& GetAutoexposureRange() const { return r_autoexposure_; }
  const CtrlRange& GetExposureAbsRange() const { return r_exposure_abs_; }
  const CtrlRange& GetAwbRange() const { return r_awb_; }
  const CtrlRange& GetWbTempRange() const { return r_wb_temp_; }
  const CtrlRange& GetBacklightRange() const { return r_backlight_; }
  const CtrlRange& GetExpoDynfpsRange() const { return r_expo_dynfps_; }

private:
  cv::VideoCapture OpenCapture(int idx, int w, int h);
  std::string GetProfileDir();
  
  std::vector<CameraDesc> cam_list_;
  std::vector<LoopbackDesc> vcam_list_;
  cv::VideoCapture cap_;
  
  int ui_cam_idx_ = 0;
  int ui_res_idx_ = 0;
  int ui_fps_idx_ = 0;
  std::string current_cam_path_;
  std::vector<int> ui_fps_opts_;
  
  // Camera control ranges
  CtrlRange r_brightness_, r_contrast_, r_saturation_, r_gain_, r_sharpness_, r_zoom_, r_focus_;
  CtrlRange r_autogain_, r_autofocus_;
  CtrlRange r_autoexposure_, r_exposure_abs_;
  CtrlRange r_awb_, r_wb_temp_, r_backlight_, r_expo_dynfps_;
  
  std::string profile_dir_;
};

} // namespace segmecam