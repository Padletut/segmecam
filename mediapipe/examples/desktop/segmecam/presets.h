// Lightweight preset application for beauty and background settings.
// Extracted to keep segmecam_gui_gpu.cpp thinner.

#pragma once

namespace segmecam {

struct BeautyState {
  // Background
  int bg_mode = 0;            // 0(None) 1(Blur) 2(Image) 3(Color)
  int blur_strength = 25;     // odd kernel
  float feather_px = 2.0f;
  bool show_mask = false;
  float solid_color[3] = {0.0f, 0.0f, 0.0f}; // RGB 0..1 for solid background

  // Skin smoothing core
  bool fx_skin = false;
  bool fx_skin_adv = true;
  float fx_skin_amount = 0.5f;
  float fx_skin_radius = 6.0f;
  float fx_skin_tex = 0.35f;
  float fx_skin_edge = 12.0f;

  // Wrinkle-aware
  bool fx_skin_wrinkle = true;
  float fx_skin_smile_boost = 0.5f;
  float fx_skin_squint_boost = 0.5f;
  float fx_skin_forehead_boost = 0.8f;
  float fx_skin_wrinkle_gain = 1.5f;
  bool fx_wrinkle_suppress_lower = true;
  float fx_wrinkle_lower_ratio = 0.45f;
  bool fx_wrinkle_ignore_glasses = true;
  float fx_wrinkle_glasses_margin = 12.0f;
  float fx_wrinkle_keep_ratio = 0.35f;
  bool fx_wrinkle_custom_scales = true;
  float fx_wrinkle_min_px = 2.0f;
  float fx_wrinkle_max_px = 8.0f;
  bool fx_wrinkle_use_skin_gate = false;
  float fx_wrinkle_mask_gain = 2.0f;
  float fx_wrinkle_baseline = 0.5f;
  float fx_wrinkle_neg_cap = 0.9f;
  bool fx_wrinkle_preview = false;

  // Advanced scaling tweaks
  float fx_adv_scale = 1.0f;             // processing scale for ROI
  float fx_adv_detail_preserve = 0.18f;  // detail re-injection amount

  // Lips / Teeth
  bool fx_lipstick = false;
  float fx_lip_alpha = 0.5f;
  float fx_lip_feather = 6.0f;
  float fx_lip_light = 0.0f;
  float fx_lip_band = 4.0f;
  float fx_lip_color[3] = {0.8f, 0.1f, 0.3f}; // RGB 0..1

  bool fx_teeth = false;
  float fx_teeth_strength = 0.5f;
  float fx_teeth_margin = 3.0f;
  
  // Auto processing scale
  bool auto_processing_scale = true; // Default enabled for better performance
  float target_fps = 14.5f;
};

// idx: 0=Default, 1=Natural, 2=Studio, 3=Glam, 4=Meeting
void ApplyPreset(int idx, BeautyState& s);

} // namespace segmecam

