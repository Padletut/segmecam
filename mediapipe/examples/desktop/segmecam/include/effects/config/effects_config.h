#pragma once

#include "effects/presets.h"
#include <opencv2/opencv.hpp>

namespace segmecam {

/**
 * Effects Configuration Module
 *
 * Handles all configuration settings for beauty effects, background effects,
 * and other effect parameters.
 */
class EffectsConfiguration {
public:
    EffectsConfiguration(BeautyState& beauty_state);
    ~EffectsConfiguration();

    // Beauty presets
    void ApplyBeautyPreset(int preset_index);
    void GetCurrentBeautyState(BeautyState& state) const;
    void SetBeautyState(const BeautyState& state);

    // Background settings
    void SetBackgroundMode(int mode);
    void SetBlurStrength(int strength);
    void SetFeatherAmount(float feather_px);
    void SetBackgroundImage(const cv::Mat& image);
    void SetBackgroundImageFromPath(const std::string& path);
    void SetSolidBackgroundColor(float r, float g, float b);
    void SetShowMask(bool enabled);

    // Skin smoothing controls
    void SetSkinSmoothingEnabled(bool enabled);
    void SetSkinSmoothingStrength(float strength);
    void SetSkinSmoothingAdvanced(bool advanced);
    void SetSkinSmoothingAmount(float amount);
    void SetSkinSmoothingRadius(float radius_px);
    void SetSkinTexturePreservation(float texture_thresh);
    void SetSkinEdgeFeather(float edge_feather_px);

    // Wrinkle-aware controls
    void SetWrinkleAwareEnabled(bool enabled);
    void SetWrinkleGain(float gain);
    void SetSmileBoost(float boost);
    void SetSquintBoost(float boost);
    void SetForeheadBoost(float boost);
    void SetSuppressLowerFace(bool enabled);
    void SetLowerFaceRatio(float ratio);
    void SetIgnoreGlasses(bool enabled);
    void SetGlassesMargin(float margin_px);
    void SetWrinkleSensitivity(float keep_ratio);
    void SetCustomWrinkleScales(bool enabled);
    void SetWrinkleMinWidth(float min_px);
    void SetWrinkleMaxWidth(float max_px);
    void SetWrinkleSkinGate(bool enabled);
    void SetWrinkleMaskGain(float gain);
    void SetWrinkleBaselineBoost(float boost);
    void SetWrinkleNegativeCap(float cap);
    void SetWrinklePreview(bool enabled);

    // Advanced processing controls
    void SetProcessingScale(float scale);
    void SetDetailPreservation(float preserve);

    // Lip effects controls
    void SetLipstickEnabled(bool enabled);
    void SetLipAlpha(float alpha);
    void SetLipFeather(float feather_px);
    void SetLipLightness(float lightness);
    void SetLipBandGrow(float band_px);
    void SetLipColor(float r, float g, float b);

    // Teeth whitening controls
    void SetTeethWhiteningEnabled(bool enabled);
    void SetTeethWhiteningStrength(float strength);
    void SetTeethMargin(float margin_px);

    // OpenCL acceleration
    void SetOpenCLEnabled(bool enabled);

    // Debug and visualization
    void SetShowLandmarks(bool enabled);

private:
    BeautyState& beauty_state_;
};

} // namespace segmecam