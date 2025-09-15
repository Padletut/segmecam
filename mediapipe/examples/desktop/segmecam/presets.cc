#include "presets.h"

namespace segmecam {

void ApplyPreset(int idx, BeautyState& s) {
  switch (idx) {
    case 0: // Default
      s.bg_mode = 0; s.show_mask = false; s.blur_strength = 25; s.feather_px = 2.0f;
      s.fx_skin = false; s.fx_skin_adv = true; s.fx_skin_amount = 0.45f; s.fx_skin_radius = 6.0f; s.fx_skin_tex = 0.35f; s.fx_skin_edge = 12.0f;
      s.fx_skin_wrinkle = true; s.fx_skin_smile_boost = 0.5f; s.fx_skin_squint_boost = 0.5f; s.fx_skin_wrinkle_gain = 1.5f; s.fx_skin_forehead_boost = 0.8f;
      s.fx_wrinkle_suppress_lower = true; s.fx_wrinkle_lower_ratio = 0.45f; s.fx_wrinkle_ignore_glasses = true; s.fx_wrinkle_glasses_margin = 12.0f; s.fx_wrinkle_keep_ratio = 0.35f;
      s.fx_wrinkle_custom_scales = true; s.fx_wrinkle_min_px = 2.0f; s.fx_wrinkle_max_px = 8.0f; s.fx_wrinkle_use_skin_gate = false; s.fx_wrinkle_mask_gain = 2.0f;
      s.fx_wrinkle_baseline = 0.5f; s.fx_wrinkle_neg_cap = 0.9f; s.fx_wrinkle_preview = false; s.fx_adv_scale = 1.0f; s.fx_adv_detail_preserve = 0.18f;
      s.fx_lipstick = false; s.fx_teeth = false; break;
    case 1: // Natural
      s.bg_mode = 1; s.blur_strength = 21; s.feather_px = 2.0f; s.show_mask = false;
      s.fx_skin = true; s.fx_skin_adv = true; s.fx_skin_amount = 0.35f; s.fx_skin_radius = 5.0f; s.fx_skin_tex = 0.50f; s.fx_skin_edge = 12.0f;
      s.fx_skin_wrinkle = true; s.fx_skin_wrinkle_gain = 1.2f; s.fx_skin_smile_boost = 0.4f; s.fx_skin_squint_boost = 0.4f; s.fx_skin_forehead_boost = 0.7f;
      s.fx_wrinkle_suppress_lower = true; s.fx_wrinkle_lower_ratio = 0.45f; s.fx_wrinkle_ignore_glasses = true; s.fx_wrinkle_glasses_margin = 10.0f; s.fx_wrinkle_keep_ratio = 0.30f;
      s.fx_wrinkle_custom_scales = true; s.fx_wrinkle_min_px = 2.0f; s.fx_wrinkle_max_px = 8.0f; s.fx_wrinkle_use_skin_gate = true; s.fx_wrinkle_mask_gain = 1.8f;
      s.fx_wrinkle_baseline = 0.35f; s.fx_wrinkle_neg_cap = 0.9f; s.fx_adv_scale = 0.9f; s.fx_adv_detail_preserve = 0.20f;
      s.fx_lipstick = false; s.fx_teeth = false; break;
    case 2: // Studio
      s.bg_mode = 1; s.blur_strength = 31; s.feather_px = 2.5f; s.show_mask = false;
      s.fx_skin = true; s.fx_skin_adv = true; s.fx_skin_amount = 0.5f; s.fx_skin_radius = 6.0f; s.fx_skin_tex = 0.40f; s.fx_skin_edge = 14.0f;
      s.fx_skin_wrinkle = true; s.fx_skin_wrinkle_gain = 1.6f; s.fx_skin_smile_boost = 0.55f; s.fx_skin_squint_boost = 0.5f; s.fx_skin_forehead_boost = 0.9f;
      s.fx_wrinkle_suppress_lower = true; s.fx_wrinkle_lower_ratio = 0.45f; s.fx_wrinkle_ignore_glasses = true; s.fx_wrinkle_glasses_margin = 12.0f; s.fx_wrinkle_keep_ratio = 0.38f;
      s.fx_wrinkle_custom_scales = true; s.fx_wrinkle_min_px = 2.0f; s.fx_wrinkle_max_px = 9.0f; s.fx_wrinkle_use_skin_gate = true; s.fx_wrinkle_mask_gain = 2.2f;
      s.fx_wrinkle_baseline = 0.5f; s.fx_wrinkle_neg_cap = 0.9f; s.fx_adv_scale = 0.88f; s.fx_adv_detail_preserve = 0.22f;
      s.fx_lipstick = false; s.fx_teeth = false; break;
    case 3: // Glam
      s.bg_mode = 1; s.blur_strength = 27; s.feather_px = 2.0f; s.show_mask = false;
      s.fx_skin = true; s.fx_skin_adv = true; s.fx_skin_amount = 0.65f; s.fx_skin_radius = 7.0f; s.fx_skin_tex = 0.30f; s.fx_skin_edge = 16.0f;
      s.fx_skin_wrinkle = true; s.fx_skin_wrinkle_gain = 1.4f; s.fx_skin_smile_boost = 0.55f; s.fx_skin_squint_boost = 0.55f; s.fx_skin_forehead_boost = 1.0f;
      s.fx_wrinkle_suppress_lower = true; s.fx_wrinkle_lower_ratio = 0.45f; s.fx_wrinkle_ignore_glasses = true; s.fx_wrinkle_glasses_margin = 12.0f; s.fx_wrinkle_keep_ratio = 0.40f;
      s.fx_wrinkle_custom_scales = true; s.fx_wrinkle_min_px = 2.0f; s.fx_wrinkle_max_px = 10.0f; s.fx_wrinkle_use_skin_gate = true; s.fx_wrinkle_mask_gain = 2.2f;
      s.fx_wrinkle_baseline = 0.55f; s.fx_wrinkle_neg_cap = 0.9f; s.fx_adv_scale = 0.85f; s.fx_adv_detail_preserve = 0.25f;
      s.fx_lipstick = true; s.fx_lip_alpha = 0.35f; s.fx_lip_feather = 6.0f; s.fx_lip_band = 4.0f; s.fx_lip_light = 0.05f; s.fx_lip_color[0]=0.86f; s.fx_lip_color[1]=0.24f; s.fx_lip_color[2]=0.40f;
      s.fx_teeth = true; s.fx_teeth_strength = 0.35f; s.fx_teeth_margin = 3.0f; break;
    case 4: // Meeting
      s.bg_mode = 1; s.blur_strength = 23; s.feather_px = 2.0f; s.show_mask = false;
      s.fx_skin = true; s.fx_skin_adv = true; s.fx_skin_amount = 0.40f; s.fx_skin_radius = 5.0f; s.fx_skin_tex = 0.55f; s.fx_skin_edge = 12.0f;
      s.fx_skin_wrinkle = true; s.fx_skin_wrinkle_gain = 1.2f; s.fx_skin_smile_boost = 0.45f; s.fx_skin_squint_boost = 0.45f; s.fx_skin_forehead_boost = 0.8f;
      s.fx_wrinkle_suppress_lower = true; s.fx_wrinkle_lower_ratio = 0.45f; s.fx_wrinkle_ignore_glasses = true; s.fx_wrinkle_glasses_margin = 12.0f; s.fx_wrinkle_keep_ratio = 0.32f;
      s.fx_wrinkle_custom_scales = true; s.fx_wrinkle_min_px = 2.0f; s.fx_wrinkle_max_px = 8.0f; s.fx_wrinkle_use_skin_gate = true; s.fx_wrinkle_mask_gain = 2.0f;
      s.fx_wrinkle_baseline = 0.4f; s.fx_wrinkle_neg_cap = 0.9f; s.fx_adv_scale = 0.9f; s.fx_adv_detail_preserve = 0.20f;
      s.fx_lipstick = false; s.fx_teeth = true; s.fx_teeth_strength = 0.25f; s.fx_teeth_margin = 3.0f; break;
  }
}

} // namespace segmecam

