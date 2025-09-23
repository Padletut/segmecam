#include "include/application/application_sync.h"
#include "include/effects/effects_manager.h"
#include "include/application/app_state.h"

#include <iostream>

namespace segmecam {

void ApplicationSync::SyncSettingsToEffectsManager(EffectsManager& effects_manager, const AppState& app_state) {
    // Background effects settings
    effects_manager.SetBackgroundMode(app_state.bg_mode);
    effects_manager.SetBlurStrength(app_state.blur_strength);
    effects_manager.SetFeatherAmount(app_state.feather_px);
    effects_manager.SetSolidBackgroundColor(app_state.solid_color[0], app_state.solid_color[1], app_state.solid_color[2]);
    effects_manager.SetShowMask(app_state.show_mask);
    effects_manager.SetShowLandmarks(app_state.show_landmarks);

    // Sync background image if available
    if (!app_state.bg_image.empty()) {
        effects_manager.SetBackgroundImage(app_state.bg_image);
    }

    // Beauty effects settings
    effects_manager.SetSkinSmoothingEnabled(app_state.fx_skin);
    effects_manager.SetSkinSmoothingStrength(app_state.fx_skin_strength);
    effects_manager.SetSkinSmoothingAdvanced(app_state.fx_skin_adv);
    effects_manager.SetSkinSmoothingAmount(app_state.fx_skin_amount);
    effects_manager.SetSkinSmoothingRadius(app_state.fx_skin_radius);
    effects_manager.SetSkinTexturePreservation(app_state.fx_skin_tex);
    effects_manager.SetSkinEdgeFeather(app_state.fx_skin_edge);

    // Wrinkle-aware settings
    effects_manager.SetWrinkleAwareEnabled(app_state.fx_skin_wrinkle);
    effects_manager.SetWrinkleGain(app_state.fx_skin_wrinkle_gain);
    effects_manager.SetSmileBoost(app_state.fx_skin_smile_boost);
    effects_manager.SetSquintBoost(app_state.fx_skin_squint_boost);
    effects_manager.SetForeheadBoost(app_state.fx_skin_forehead_boost);
    effects_manager.SetSuppressLowerFace(app_state.fx_wrinkle_suppress_lower);
    effects_manager.SetLowerFaceRatio(app_state.fx_wrinkle_lower_ratio);
    effects_manager.SetIgnoreGlasses(app_state.fx_wrinkle_ignore_glasses);
    effects_manager.SetGlassesMargin(app_state.fx_wrinkle_glasses_margin);
    effects_manager.SetWrinkleSensitivity(app_state.fx_wrinkle_keep_ratio);
    effects_manager.SetCustomWrinkleScales(app_state.fx_wrinkle_custom_scales);
    effects_manager.SetWrinkleMinWidth(app_state.fx_wrinkle_min_px);
    effects_manager.SetWrinkleMaxWidth(app_state.fx_wrinkle_max_px);
    effects_manager.SetWrinkleSkinGate(app_state.fx_wrinkle_use_skin_gate);
    effects_manager.SetWrinkleMaskGain(app_state.fx_wrinkle_mask_gain);
    effects_manager.SetWrinkleBaselineBoost(app_state.fx_wrinkle_baseline);
    effects_manager.SetWrinkleNegativeCap(app_state.fx_wrinkle_neg_cap);
    effects_manager.SetWrinklePreview(app_state.fx_wrinkle_preview);

    // Processing scale settings
    effects_manager.SetProcessingScale(app_state.fx_adv_scale);
    effects_manager.SetDetailPreservation(app_state.fx_adv_detail_preserve);

    // Auto processing scale settings
    effects_manager.SetAutoProcessingScaleEnabled(app_state.auto_processing_scale);
    effects_manager.SetTargetFPS(app_state.target_fps);

    // Lip effects settings
    effects_manager.SetLipstickEnabled(app_state.fx_lipstick);
    effects_manager.SetLipAlpha(app_state.fx_lip_alpha);
    effects_manager.SetLipFeather(app_state.fx_lip_feather);
    effects_manager.SetLipLightness(app_state.fx_lip_light);
    effects_manager.SetLipBandGrow(app_state.fx_lip_band);
    effects_manager.SetLipColor(app_state.fx_lip_color[0], app_state.fx_lip_color[1], app_state.fx_lip_color[2]);

    // Teeth whitening settings
    effects_manager.SetTeethWhiteningEnabled(app_state.fx_teeth);
    effects_manager.SetTeethWhiteningStrength(app_state.fx_teeth_strength);
    effects_manager.SetTeethMargin(app_state.fx_teeth_margin);
}

void ApplicationSync::SyncStatusFromEffectsManager(const EffectsManager& effects_manager, AppState& app_state) {
    // Sync OpenCL availability status (detected by EffectsManager)
    bool prev_opencl_available = app_state.opencl_available;
    app_state.opencl_available = effects_manager.IsOpenCLAvailable();

    // Enable OpenCL by default when first detected as available
    if (!prev_opencl_available && app_state.opencl_available) {
        app_state.use_opencl = true;
        std::cout << "OpenCL detected and enabled by default for acceleration" << std::endl;
    }
}

} // namespace segmecam