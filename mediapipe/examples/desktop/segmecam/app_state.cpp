#include "app_state.h"
#include "mediapipe/framework/port/opencv_core_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"

namespace segmecam {

void AppState::SaveToProfile(cv::FileStorage& fs) const {
  fs << "vsync_on" << (int)vsync_on;
  fs << "show_mask" << (int)show_mask << "bg_mode" << bg_mode << "blur_strength" << blur_strength << "feather_px" << feather_px;
  fs << "solid_color" << "[" << solid_color[0] << solid_color[1] << solid_color[2] << "]";
  fs << "bg_path" << bg_path_buf;
  fs << "show_landmarks" << (int)show_landmarks << "lm_roi_mode" << (int)lm_roi_mode << "lm_apply_rot" << (int)lm_apply_rot
      << "lm_flip_x" << (int)lm_flip_x << "lm_flip_y" << (int)lm_flip_y << "lm_swap_xy" << (int)lm_swap_xy
      << "show_mesh" << (int)show_mesh << "show_mesh_dense" << (int)show_mesh_dense;
  fs << "fx_skin" << (int)fx_skin << "fx_skin_adv" << (int)fx_skin_adv << "fx_skin_strength" << fx_skin_strength
      << "fx_skin_amount" << fx_skin_amount << "fx_skin_radius" << fx_skin_radius << "fx_skin_tex" << fx_skin_tex << "fx_skin_edge" << fx_skin_edge
      << "fx_adv_scale" << fx_adv_scale << "fx_adv_detail_preserve" << fx_adv_detail_preserve;
  fs << "use_opencl" << (int)use_opencl;
  fs << "fx_skin_wrinkle" << (int)fx_skin_wrinkle << "fx_skin_smile_boost" << fx_skin_smile_boost << "fx_skin_squint_boost" << fx_skin_squint_boost
      << "fx_skin_forehead_boost" << fx_skin_forehead_boost << "fx_skin_wrinkle_gain" << fx_skin_wrinkle_gain
      << "fx_wrinkle_suppress_lower" << (int)fx_wrinkle_suppress_lower << "fx_wrinkle_lower_ratio" << fx_wrinkle_lower_ratio
      << "fx_wrinkle_ignore_glasses" << (int)fx_wrinkle_ignore_glasses << "fx_wrinkle_glasses_margin" << fx_wrinkle_glasses_margin
      << "fx_wrinkle_keep_ratio" << fx_wrinkle_keep_ratio << "fx_wrinkle_custom_scales" << (int)fx_wrinkle_custom_scales
      << "fx_wrinkle_min_px" << fx_wrinkle_min_px << "fx_wrinkle_max_px" << fx_wrinkle_max_px
      << "fx_wrinkle_use_skin_gate" << (int)fx_wrinkle_use_skin_gate << "fx_wrinkle_mask_gain" << fx_wrinkle_mask_gain
      << "fx_wrinkle_baseline" << fx_wrinkle_baseline << "fx_wrinkle_neg_cap" << fx_wrinkle_neg_cap
      << "fx_wrinkle_preview" << (int)fx_wrinkle_preview;
  fs << "fx_lipstick" << (int)fx_lipstick << "fx_lip_alpha" << fx_lip_alpha << "fx_lip_feather" << fx_lip_feather << "fx_lip_light" << fx_lip_light << "fx_lip_band" << fx_lip_band;
  fs << "fx_lip_color" << "[" << fx_lip_color[0] << fx_lip_color[1] << fx_lip_color[2] << "]";
  fs << "fx_teeth" << (int)fx_teeth << "fx_teeth_strength" << fx_teeth_strength << "fx_teeth_margin" << fx_teeth_margin;
}

void AppState::LoadFromProfile(const cv::FileNode& root) {
  vsync_on = ReadInt(root["vsync_on"], vsync_on);
  show_mask = ReadInt(root["show_mask"], show_mask);
  bg_mode = ReadInt(root["bg_mode"], bg_mode);
  blur_strength = ReadInt(root["blur_strength"], blur_strength);
  feather_px = ReadFloat(root["feather_px"], feather_px);
  
  // Load solid color array
  auto sc_node = root["solid_color"];
  if (!sc_node.empty() && sc_node.isSeq() && sc_node.size() >= 3) {
    solid_color[0] = ReadFloat(sc_node[0], solid_color[0]);
    solid_color[1] = ReadFloat(sc_node[1], solid_color[1]);
    solid_color[2] = ReadFloat(sc_node[2], solid_color[2]);
  }
  
  // Load background path
  if (!root["bg_path"].empty()) {
    std::string bg_path_str; 
    root["bg_path"] >> bg_path_str;
    std::snprintf(bg_path_buf, sizeof(bg_path_buf), "%s", bg_path_str.c_str());
  }
  
  show_landmarks = ReadInt(root["show_landmarks"], show_landmarks);
  lm_roi_mode = ReadInt(root["lm_roi_mode"], lm_roi_mode);
  lm_apply_rot = ReadInt(root["lm_apply_rot"], lm_apply_rot);
  lm_flip_x = ReadInt(root["lm_flip_x"], lm_flip_x);
  lm_flip_y = ReadInt(root["lm_flip_y"], lm_flip_y);
  lm_swap_xy = ReadInt(root["lm_swap_xy"], lm_swap_xy);
  show_mesh = ReadInt(root["show_mesh"], show_mesh);
  show_mesh_dense = ReadInt(root["show_mesh_dense"], show_mesh_dense);
  
  fx_skin = ReadInt(root["fx_skin"], fx_skin);
  fx_skin_adv = ReadInt(root["fx_skin_adv"], fx_skin_adv);
  fx_skin_strength = ReadFloat(root["fx_skin_strength"], fx_skin_strength);
  fx_skin_amount = ReadFloat(root["fx_skin_amount"], fx_skin_amount);
  fx_skin_radius = ReadFloat(root["fx_skin_radius"], fx_skin_radius);
  fx_skin_tex = ReadFloat(root["fx_skin_tex"], fx_skin_tex);
  fx_skin_edge = ReadFloat(root["fx_skin_edge"], fx_skin_edge);
  fx_adv_scale = ReadFloat(root["fx_adv_scale"], fx_adv_scale);
  fx_adv_detail_preserve = ReadFloat(root["fx_adv_detail_preserve"], fx_adv_detail_preserve);
  
  use_opencl = ReadInt(root["use_opencl"], 1) != 0; // Default to enabled
  
  // Wrinkle settings
  fx_skin_wrinkle = ReadInt(root["fx_skin_wrinkle"], fx_skin_wrinkle);
  fx_skin_smile_boost = ReadFloat(root["fx_skin_smile_boost"], fx_skin_smile_boost);
  fx_skin_squint_boost = ReadFloat(root["fx_skin_squint_boost"], fx_skin_squint_boost);
  fx_skin_forehead_boost = ReadFloat(root["fx_skin_forehead_boost"], fx_skin_forehead_boost);
  fx_skin_wrinkle_gain = ReadFloat(root["fx_skin_wrinkle_gain"], fx_skin_wrinkle_gain);
  fx_wrinkle_suppress_lower = ReadInt(root["fx_wrinkle_suppress_lower"], fx_wrinkle_suppress_lower);
  fx_wrinkle_lower_ratio = ReadFloat(root["fx_wrinkle_lower_ratio"], fx_wrinkle_lower_ratio);
  fx_wrinkle_ignore_glasses = ReadInt(root["fx_wrinkle_ignore_glasses"], fx_wrinkle_ignore_glasses);
  fx_wrinkle_glasses_margin = ReadFloat(root["fx_wrinkle_glasses_margin"], fx_wrinkle_glasses_margin);
  fx_wrinkle_keep_ratio = ReadFloat(root["fx_wrinkle_keep_ratio"], fx_wrinkle_keep_ratio);
  fx_wrinkle_custom_scales = ReadInt(root["fx_wrinkle_custom_scales"], fx_wrinkle_custom_scales);
  fx_wrinkle_min_px = ReadFloat(root["fx_wrinkle_min_px"], fx_wrinkle_min_px);
  fx_wrinkle_max_px = ReadFloat(root["fx_wrinkle_max_px"], fx_wrinkle_max_px);
  fx_wrinkle_use_skin_gate = ReadInt(root["fx_wrinkle_use_skin_gate"], fx_wrinkle_use_skin_gate);
  fx_wrinkle_mask_gain = ReadFloat(root["fx_wrinkle_mask_gain"], fx_wrinkle_mask_gain);
  fx_wrinkle_baseline = ReadFloat(root["fx_wrinkle_baseline"], fx_wrinkle_baseline);
  fx_wrinkle_neg_cap = ReadFloat(root["fx_wrinkle_neg_cap"], fx_wrinkle_neg_cap);
  fx_wrinkle_preview = ReadInt(root["fx_wrinkle_preview"], fx_wrinkle_preview);
  
  // Lip effects
  fx_lipstick = ReadInt(root["fx_lipstick"], fx_lipstick);
  fx_lip_alpha = ReadFloat(root["fx_lip_alpha"], fx_lip_alpha);
  fx_lip_feather = ReadFloat(root["fx_lip_feather"], fx_lip_feather);
  fx_lip_light = ReadFloat(root["fx_lip_light"], fx_lip_light);
  fx_lip_band = ReadFloat(root["fx_lip_band"], fx_lip_band);
  
  // Load lip color array
  auto lc_node = root["fx_lip_color"];
  if (!lc_node.empty() && lc_node.isSeq() && lc_node.size() >= 3) {
    fx_lip_color[0] = ReadFloat(lc_node[0], fx_lip_color[0]);
    fx_lip_color[1] = ReadFloat(lc_node[1], fx_lip_color[1]);
    fx_lip_color[2] = ReadFloat(lc_node[2], fx_lip_color[2]);
  }
  
  // Teeth whitening
  fx_teeth = ReadInt(root["fx_teeth"], fx_teeth);
  fx_teeth_strength = ReadFloat(root["fx_teeth_strength"], fx_teeth_strength);
  fx_teeth_margin = ReadFloat(root["fx_teeth_margin"], fx_teeth_margin);
}

int AppState::ReadInt(const cv::FileNode& n, int def) const {
  return n.empty() ? def : (int)n;
}

float AppState::ReadFloat(const cv::FileNode& n, float def) const {
  return n.empty() ? def : (float)n;
}

} // namespace segmecam