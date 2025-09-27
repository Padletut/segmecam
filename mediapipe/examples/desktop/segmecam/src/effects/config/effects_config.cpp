#include "effects/config/effects_config.h"
#include "effects/presets.h"
#include <iostream>
#include <opencv2/core/ocl.hpp>

namespace segmecam {

EffectsConfiguration::EffectsConfiguration(BeautyState& beauty_state)
    : beauty_state_(beauty_state) {
}

EffectsConfiguration::~EffectsConfiguration() {
    // No cleanup needed
}

void EffectsConfiguration::ApplyBeautyPreset(int preset_index) {
    ApplyPreset(preset_index, beauty_state_);
    std::cout << "✨ Applied beauty preset " << preset_index << std::endl;
}

void EffectsConfiguration::GetCurrentBeautyState(BeautyState& state) const {
    state = beauty_state_;
}

void EffectsConfiguration::SetBeautyState(const BeautyState& state) {
    float prev_fx_adv_scale = beauty_state_.fx_adv_scale;
    beauty_state_ = state;
    beauty_state_.fx_adv_scale = prev_fx_adv_scale;
}

// Background settings
void EffectsConfiguration::SetBackgroundMode(int mode) {
    beauty_state_.bg_mode = std::clamp(mode, 0, 3);
}

void EffectsConfiguration::SetBlurStrength(int strength) {
    beauty_state_.blur_strength = std::max(1, strength);
    if ((beauty_state_.blur_strength % 2) == 0) beauty_state_.blur_strength++; // Ensure odd
}

void EffectsConfiguration::SetFeatherAmount(float feather_px) {
    beauty_state_.feather_px = std::max(0.0f, feather_px);
}

void EffectsConfiguration::SetBackgroundImage(const cv::Mat& image) {
    // Note: This needs access to background_image_ from the main manager
    // For now, this is a placeholder
}

void EffectsConfiguration::SetBackgroundImageFromPath(const std::string& path) {
    if (path.empty()) return;
    
    cv::Mat image = cv::imread(path);
    if (!image.empty()) {
        SetBackgroundImage(image);
        std::cout << "✅ Loaded background image from path: " << path << " (" 
                  << image.cols << "x" << image.rows << ")" << std::endl;
    } else {
        std::cout << "❌ Failed to load background image from path: " << path << std::endl;
    }
}

void EffectsConfiguration::SetSolidBackgroundColor(float r, float g, float b) {
    beauty_state_.solid_color[0] = std::clamp(r, 0.0f, 1.0f);
    beauty_state_.solid_color[1] = std::clamp(g, 0.0f, 1.0f);
    beauty_state_.solid_color[2] = std::clamp(b, 0.0f, 1.0f);
}

void EffectsConfiguration::SetShowMask(bool enabled) {
    beauty_state_.show_mask = enabled;
}

// Skin smoothing controls
void EffectsConfiguration::SetSkinSmoothingEnabled(bool enabled) {
    beauty_state_.fx_skin = enabled;
}

void EffectsConfiguration::SetSkinSmoothingStrength(float strength) {
    beauty_state_.fx_skin_amount = std::clamp(strength, 0.0f, 1.0f);
}

void EffectsConfiguration::SetSkinSmoothingAdvanced(bool advanced) {
    beauty_state_.fx_skin_adv = advanced;
}

void EffectsConfiguration::SetSkinSmoothingAmount(float amount) {
    beauty_state_.fx_skin_amount = std::clamp(amount, 0.0f, 1.0f);
}

void EffectsConfiguration::SetSkinSmoothingRadius(float radius_px) {
    beauty_state_.fx_skin_radius = std::max(1.0f, radius_px);
}

void EffectsConfiguration::SetSkinTexturePreservation(float texture_thresh) {
    beauty_state_.fx_skin_tex = std::clamp(texture_thresh, 0.0f, 1.0f);
}

void EffectsConfiguration::SetSkinEdgeFeather(float edge_feather_px) {
    beauty_state_.fx_skin_edge = std::max(0.0f, edge_feather_px);
}

// Wrinkle-aware controls
void EffectsConfiguration::SetWrinkleAwareEnabled(bool enabled) {
    beauty_state_.fx_skin_wrinkle = enabled;
}

void EffectsConfiguration::SetWrinkleGain(float gain) {
    beauty_state_.fx_skin_wrinkle_gain = std::max(0.0f, gain);
}

void EffectsConfiguration::SetSmileWrinkleGain(float gain) {
    beauty_state_.fx_skin_smile_wrinkle_gain = std::max(0.0f, gain);
}

void EffectsConfiguration::SetSmileBoost(float boost) {
    beauty_state_.fx_skin_smile_boost = std::clamp(boost, 0.0f, 2.0f);
}

void EffectsConfiguration::SetSquintBoost(float boost) {
    beauty_state_.fx_skin_squint_boost = std::clamp(boost, 0.0f, 2.0f);
}

void EffectsConfiguration::SetForeheadBoost(float boost) {
    beauty_state_.fx_skin_forehead_boost = std::clamp(boost, 0.0f, 2.0f);
}

void EffectsConfiguration::SetSuppressLowerFace(bool enabled) {
    beauty_state_.fx_wrinkle_suppress_lower = enabled;
}

void EffectsConfiguration::SetLowerFaceRatio(float ratio) {
    beauty_state_.fx_wrinkle_lower_ratio = std::clamp(ratio, 0.1f, 0.8f);
}

void EffectsConfiguration::SetIgnoreGlasses(bool enabled) {
    beauty_state_.fx_wrinkle_ignore_glasses = enabled;
}

void EffectsConfiguration::SetGlassesMargin(float margin_px) {
    beauty_state_.fx_wrinkle_glasses_margin = std::max(0.0f, margin_px);
}

void EffectsConfiguration::SetWrinkleSensitivity(float keep_ratio) {
    beauty_state_.fx_wrinkle_keep_ratio = std::clamp(keep_ratio, 0.01f, 1.0f);
}

void EffectsConfiguration::SetCustomWrinkleScales(bool enabled) {
    beauty_state_.fx_wrinkle_custom_scales = enabled;
}

void EffectsConfiguration::SetWrinkleMinWidth(float min_px) {
    beauty_state_.fx_wrinkle_min_px = std::max(1.0f, min_px);
}

void EffectsConfiguration::SetWrinkleMaxWidth(float max_px) {
    beauty_state_.fx_wrinkle_max_px = std::max(beauty_state_.fx_wrinkle_min_px, max_px);
}

void EffectsConfiguration::SetWrinkleSkinGate(bool enabled) {
    beauty_state_.fx_wrinkle_use_skin_gate = enabled;
}

void EffectsConfiguration::SetWrinkleMaskGain(float gain) {
    beauty_state_.fx_wrinkle_mask_gain = std::max(0.5f, gain);
}

void EffectsConfiguration::SetWrinkleBaselineBoost(float boost) {
    beauty_state_.fx_wrinkle_baseline = std::clamp(boost, 0.0f, 1.0f);
}

void EffectsConfiguration::SetWrinkleNegativeCap(float cap) {
    beauty_state_.fx_wrinkle_neg_cap = std::clamp(cap, 0.5f, 1.0f);
}

void EffectsConfiguration::SetWrinklePreview(bool enabled) {
    beauty_state_.fx_wrinkle_preview = enabled;
}

// Advanced processing controls
void EffectsConfiguration::SetProcessingScale(float scale) {
    beauty_state_.fx_adv_scale = std::clamp(scale, 0.4f, 1.0f);
}

void EffectsConfiguration::SetDetailPreservation(float preserve) {
    beauty_state_.fx_adv_detail_preserve = std::clamp(preserve, 0.0f, 0.5f);
}

// Lip effects controls
void EffectsConfiguration::SetLipstickEnabled(bool enabled) {
    beauty_state_.fx_lipstick = enabled;
}

void EffectsConfiguration::SetLipAlpha(float alpha) {
    beauty_state_.fx_lip_alpha = std::clamp(alpha, 0.0f, 1.0f);
}

void EffectsConfiguration::SetLipFeather(float feather_px) {
    beauty_state_.fx_lip_feather = std::max(0.0f, feather_px);
}

void EffectsConfiguration::SetLipLightness(float lightness) {
    beauty_state_.fx_lip_light = std::clamp(lightness, -1.0f, 1.0f);
}

void EffectsConfiguration::SetLipBandGrow(float band_px) {
    beauty_state_.fx_lip_band = std::max(0.0f, band_px);
}

void EffectsConfiguration::SetLipColor(float r, float g, float b) {
    beauty_state_.fx_lip_color[0] = std::clamp(r, 0.0f, 1.0f);
    beauty_state_.fx_lip_color[1] = std::clamp(g, 0.0f, 1.0f);
    beauty_state_.fx_lip_color[2] = std::clamp(b, 0.0f, 1.0f);
}

// Teeth whitening controls
void EffectsConfiguration::SetTeethWhiteningEnabled(bool enabled) {
    beauty_state_.fx_teeth = enabled;
}

void EffectsConfiguration::SetTeethWhiteningStrength(float strength) {
    beauty_state_.fx_teeth_strength = std::clamp(strength, 0.0f, 1.0f);
}

void EffectsConfiguration::SetTeethMargin(float margin_px) {
    beauty_state_.fx_teeth_margin = std::max(0.0f, margin_px);
}

// OpenCL acceleration
void EffectsConfiguration::SetOpenCLEnabled(bool enabled) {
    // Note: This needs access to state_.opencl_available from the main manager
    // For now, this is a placeholder
    cv::ocl::setUseOpenCL(enabled);
}

// Debug and visualization
void EffectsConfiguration::SetShowLandmarks(bool enabled) {
    // Note: This needs access to state_.show_landmarks from the main manager
    // For now, this is a placeholder
}

} // namespace segmecam