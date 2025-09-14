#include "camera_manager.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <linux/videodev2.h>

namespace fs = std::filesystem;

namespace segmecam {

CameraManager::CameraManager() {
  cam_list_ = EnumerateCameras();
  vcam_list_ = EnumerateLoopbackDevices();
  profile_dir_ = GetProfileDir();
}

bool CameraManager::Initialize(int cam_index) {
  // Find camera index in enumerated list
  if (!cam_list_.empty()) {
    for (size_t i = 0; i < cam_list_.size(); ++i) {
      if (cam_list_[i].index == cam_index) { 
        ui_cam_idx_ = (int)i; 
        break; 
      }
    }
  }
  
  // Set initial resolution
  int init_w = 0, init_h = 0;
  if (!cam_list_.empty() && !cam_list_[ui_cam_idx_].resolutions.empty()) {
    auto wh = cam_list_[ui_cam_idx_].resolutions.back();
    init_w = wh.first; 
    init_h = wh.second;
  }
  
  cap_ = OpenCapture(cam_index, init_w, init_h);
  
  // Setup FPS options
  current_cam_path_ = (!cam_list_.empty() ? cam_list_[ui_cam_idx_].path : std::string());
  if (!current_cam_path_.empty() && init_w > 0 && init_h > 0) {
    ui_fps_opts_ = EnumerateFPS(current_cam_path_, init_w, init_h);
    if (!ui_fps_opts_.empty()) {
      ui_fps_idx_ = (int)ui_fps_opts_.size() - 1;
    }
  }
  
  RefreshControls();
  ApplyDefaultControls();
  
  if (!cap_.isOpened()) {
    std::cout << "V4L2 open failed for index " << cam_index << ", retrying with CAP_ANY" << std::endl;
    cap_.open(cam_index);
  }
  
  if (!cap_.isOpened()) { 
    std::fprintf(stderr, "Unable to open camera %d\n", cam_index); 
    return false; 
  }
  
  std::cout << "Camera backend: " << cap_.getBackendName() << std::endl;
  return true;
}

cv::VideoCapture CameraManager::OpenCapture(int idx, int w, int h) {
  cv::VideoCapture c(idx, cv::CAP_V4L2);
  if (c.isOpened() && w > 0 && h > 0) {
    c.set(cv::CAP_PROP_FRAME_WIDTH, w);
    c.set(cv::CAP_PROP_FRAME_HEIGHT, h);
    c.set(cv::CAP_PROP_FRAME_WIDTH, w);
    c.set(cv::CAP_PROP_FRAME_HEIGHT, h);
  }
  return c;
}

void CameraManager::SetCurrentCamera(int ui_cam_idx, int ui_res_idx, int ui_fps_idx) {
  ui_cam_idx_ = ui_cam_idx;
  ui_res_idx_ = ui_res_idx;
  ui_fps_idx_ = ui_fps_idx;
  
  if (ui_cam_idx_ >= 0 && ui_cam_idx_ < (int)cam_list_.size()) {
    current_cam_path_ = cam_list_[ui_cam_idx_].path;
    
    const auto& rlist = cam_list_[ui_cam_idx_].resolutions;
    if (!rlist.empty() && ui_res_idx_ >= 0 && ui_res_idx_ < (int)rlist.size()) {
      int w = rlist[ui_res_idx_].first;
      int h = rlist[ui_res_idx_].second;
      ui_fps_opts_ = EnumerateFPS(current_cam_path_, w, h);
    }
  }
}

void CameraManager::RefreshControls() {
  if (current_cam_path_.empty()) return;
  
  QueryCtrl(current_cam_path_, V4L2_CID_BRIGHTNESS, &r_brightness_);
  QueryCtrl(current_cam_path_, V4L2_CID_CONTRAST, &r_contrast_);
  QueryCtrl(current_cam_path_, V4L2_CID_SATURATION, &r_saturation_);
  QueryCtrl(current_cam_path_, V4L2_CID_GAIN, &r_gain_);
  QueryCtrl(current_cam_path_, V4L2_CID_SHARPNESS, &r_sharpness_);
  QueryCtrl(current_cam_path_, V4L2_CID_ZOOM_ABSOLUTE, &r_zoom_);
  QueryCtrl(current_cam_path_, V4L2_CID_FOCUS_ABSOLUTE, &r_focus_);
  QueryCtrl(current_cam_path_, V4L2_CID_AUTOGAIN, &r_autogain_);
  QueryCtrl(current_cam_path_, V4L2_CID_FOCUS_AUTO, &r_autofocus_);
  // Exposure controls (fallback when AUTOGAIN is not available)
  QueryCtrl(current_cam_path_, V4L2_CID_EXPOSURE_AUTO, &r_autoexposure_);
  QueryCtrl(current_cam_path_, V4L2_CID_EXPOSURE_ABSOLUTE, &r_exposure_abs_);
  // White balance / backlight / dynamic fps (exposure priority)
  QueryCtrl(current_cam_path_, V4L2_CID_AUTO_WHITE_BALANCE, &r_awb_);
  QueryCtrl(current_cam_path_, V4L2_CID_WHITE_BALANCE_TEMPERATURE, &r_wb_temp_);
  QueryCtrl(current_cam_path_, V4L2_CID_BACKLIGHT_COMPENSATION, &r_backlight_);
  QueryCtrl(current_cam_path_, V4L2_CID_EXPOSURE_AUTO_PRIORITY, &r_expo_dynfps_);
}

void CameraManager::ApplyDefaultControls() {
  // Set auto focus enabled by default if supported
  if (!current_cam_path_.empty() && r_autofocus_.available && r_autofocus_.val == 0) {
    if (SetCtrl(current_cam_path_, V4L2_CID_FOCUS_AUTO, 1)) {
      r_autofocus_.val = 1;
    }
  }
}

std::string CameraManager::GetProfileDir() {
  const char* home = std::getenv("HOME");
  std::string dir = (home && *home) ? std::string(home) + "/.config/segmecam" : std::string("./.segmecam");
  fs::create_directories(dir);
  return dir;
}

std::vector<std::string> CameraManager::ListProfiles() {
  std::vector<std::string> names;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(profile_dir_, ec)) {
    if (ec) break; 
    if (!e.is_regular_file()) continue;
    auto p = e.path(); 
    if (p.extension() == ".yml" || p.extension() == ".yaml") {
      names.push_back(p.stem().string());
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

bool CameraManager::SaveProfile(const std::string& name, const std::function<void(cv::FileStorage&)>& save_callback) {
  if (name.empty()) return false; 
  fs::create_directories(profile_dir_);
  std::string path = profile_dir_ + "/" + name + ".yml";
  cv::FileStorage fsw(path, cv::FileStorage::WRITE);
  if (!fsw.isOpened()) return false;
  
  // Camera info
  int saved_w = 0, saved_h = 0, saved_fps = 0;
  if (!cam_list_.empty() && ui_cam_idx_ >= 0 && ui_cam_idx_ < (int)cam_list_.size()) {
    const auto& rlist = cam_list_[ui_cam_idx_].resolutions;
    if (!rlist.empty() && ui_res_idx_ >= 0 && ui_res_idx_ < (int)rlist.size()) { 
      saved_w = rlist[ui_res_idx_].first; 
      saved_h = rlist[ui_res_idx_].second; 
    }
  }
  if (!ui_fps_opts_.empty() && ui_fps_idx_ >= 0 && ui_fps_idx_ < (int)ui_fps_opts_.size()) {
    saved_fps = ui_fps_opts_[ui_fps_idx_];
  }
  
  fsw << "cam_path" << current_cam_path_;
  fsw << "res_w" << saved_w << "res_h" << saved_h;
  fsw << "fps_value" << saved_fps;
  fsw << "ui_cam_idx" << ui_cam_idx_ << "ui_res_idx" << ui_res_idx_ << "ui_fps_idx" << ui_fps_idx_;
  
  // Call external save callback for app-specific settings
  if (save_callback) {
    save_callback(fsw);
  }
  
  fsw.release();
  return true;
}

bool CameraManager::LoadProfile(const std::string& name, const std::function<void(const cv::FileNode&)>& load_callback) {
  std::string path = profile_dir_ + "/" + name + ".yml";
  cv::FileStorage fsr(path, cv::FileStorage::READ);
  if (!fsr.isOpened()) return false;
  
  cv::FileNode root = fsr.root();
  if (root.empty() || !root.isMap()) return false;
  
  // Camera selection by path, resolution and fps (robust to device order changes)
  std::string saved_path; 
  if (!root["cam_path"].empty()) root["cam_path"] >> saved_path;
  int saved_w = !root["res_w"].empty() ? (int)root["res_w"] : 0;
  int saved_h = !root["res_h"].empty() ? (int)root["res_h"] : 0;
  int saved_fps = !root["fps_value"].empty() ? (int)root["fps_value"] : 0;
  
  // Choose camera index
  if (!cam_list_.empty()) {
    int chosen_idx = -1;
    if (!saved_path.empty()) {
      for (size_t i = 0; i < cam_list_.size(); ++i) {
        if (cam_list_[i].path == saved_path) { 
          chosen_idx = (int)i; 
          break; 
        }
      }
    }
    if (chosen_idx < 0) chosen_idx = 0; // fallback to first enumerated
    ui_cam_idx_ = chosen_idx;
    
    // Choose resolution
    const auto& rlist = cam_list_[ui_cam_idx_].resolutions;
    if (!rlist.empty()) {
      int ridx = -1;
      if (saved_w > 0 && saved_h > 0) {
        for (size_t i = 0; i < rlist.size(); ++i) {
          if (rlist[i].first == saved_w && rlist[i].second == saved_h) { 
            ridx = (int)i; 
            break; 
          }
        }
      }
      if (ridx < 0) ridx = (int)rlist.size() - 1; // fallback to largest known
      ui_res_idx_ = ridx;
      
      // Enumerate fps for this resolution
      current_cam_path_ = cam_list_[ui_cam_idx_].path;
      ui_fps_opts_ = EnumerateFPS(current_cam_path_, rlist[ui_res_idx_].first, rlist[ui_res_idx_].second);
      
      // Pick fps
      if (!ui_fps_opts_.empty()) {
        int fidx = (int)ui_fps_opts_.size() - 1;
        if (saved_fps > 0) {
          for (size_t i = 0; i < ui_fps_opts_.size(); ++i) {
            if (ui_fps_opts_[i] == saved_fps) { 
              fidx = (int)i; 
              break; 
            }
          }
        }
        ui_fps_idx_ = fidx;
      } else {
        ui_fps_idx_ = 0;
      }
    }
  }
  
  // Call external load callback for app-specific settings
  if (load_callback) {
    load_callback(root);
  }
  
  return true;
}

} // namespace segmecam