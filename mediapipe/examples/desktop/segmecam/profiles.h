#pragma once

#include <string>
#include <vector>

namespace segmecam {

struct ProfileData {
  std::string cam_path; int res_w=0, res_h=0; int fps_value=0;
  int ui_cam_idx=-1, ui_res_idx=-1, ui_fps_idx=-1;
  int vsync_on=1, show_mask=0, bg_mode=0, blur_strength=25; float feather_px=2.0f;
  float solid_color[3]={0.0f,0.0f,0.0f}; std::string bg_path;
  int show_landmarks=0, lm_roi_mode=0, lm_apply_rot=1, lm_flip_x=0, lm_flip_y=0, lm_swap_xy=0, show_mesh=0, show_mesh_dense=0;
  int fx_skin=0, fx_skin_adv=1; float fx_skin_strength=0.4f, fx_skin_amount=0.5f, fx_skin_radius=6.0f, fx_skin_tex=0.35f, fx_skin_edge=12.0f;
  int fx_skin_wrinkle=1; float fx_skin_smile_boost=0.5f, fx_skin_squint_boost=0.5f, fx_skin_forehead_boost=0.8f, fx_skin_wrinkle_gain=1.5f;
  int fx_wrinkle_suppress_lower=1; float fx_wrinkle_lower_ratio=0.45f; int fx_wrinkle_ignore_glasses=1; float fx_wrinkle_glasses_margin=12.0f;
  float fx_wrinkle_keep_ratio=0.35f; int fx_wrinkle_custom_scales=1; float fx_wrinkle_min_px=2.0f, fx_wrinkle_max_px=8.0f;
  int fx_wrinkle_use_skin_gate=0; float fx_wrinkle_mask_gain=2.0f; float fx_wrinkle_baseline=0.5f; float fx_wrinkle_neg_cap=0.9f; int fx_wrinkle_preview=0;
  float fx_adv_scale=1.0f, fx_adv_detail_preserve=0.18f;
  int fx_lipstick=0; float fx_lip_alpha=0.5f, fx_lip_feather=6.0f, fx_lip_light=0.0f, fx_lip_band=4.0f; float fx_lip_color[3]={0.8f,0.1f,0.3f};
  int fx_teeth=0; float fx_teeth_strength=0.5f, fx_teeth_margin=3.0f;
  int use_opencl=0;
};

// Returns ~/.config/segmecam and ensures it exists.
std::string GetProfileDir();

std::vector<std::string> ListProfiles(const std::string& dir);
bool SaveProfile(const std::string& dir, const std::string& name, const ProfileData& d);
bool LoadProfile(const std::string& dir, const std::string& name, ProfileData* d);

std::string DefaultProfileFile(const std::string& dir);
bool SetDefaultProfile(const std::string& dir, const std::string& name);
bool GetDefaultProfile(const std::string& dir, std::string* name_out);

} // namespace segmecam
