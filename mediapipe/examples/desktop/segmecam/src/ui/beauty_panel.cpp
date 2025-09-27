#include "include/ui/ui_panels.h"
#include "include/effects/presets.h"
#include <iostream>

namespace segmecam {

// Beauty Panel Implementation
BeautyPanel::BeautyPanel(AppState& state, EffectsManager& effects_mgr)
    : UIPanel("Beauty"), state_(state), effects_mgr_(effects_mgr) {
}

void BeautyPanel::Render() {
    if (!visible_) return;
    
    if (ImGui::CollapsingHeader("Beauty Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderPresets();
        RenderPerformanceControls();
        RenderSkinSmoothing();
        RenderLipEffects();
        RenderTeethWhitening();
    }
}

void BeautyPanel::RenderPresets() {
    ImGui::Text("Beauty Presets");
    ImGui::TextDisabled("Quick apply (save profile to persist)");
    
    const char* preset_names[] = {"Default", "Natural", "Studio", "Glam", "Meeting"};
    static int ui_preset_idx = 0;
    
    ImGui::Combo("Preset", &ui_preset_idx, preset_names, IM_ARRAYSIZE(preset_names));
    ImGui::SameLine();
    
    if (ImGui::Button("Apply##preset")) {
        ApplyBeautyPreset(ui_preset_idx, preset_names[ui_preset_idx]);
    }
    
    ImGui::Separator();
}

void BeautyPanel::RenderPerformanceControls() {
    if (ImGui::Checkbox("OpenCL", &state_.use_opencl)) {
        effects_mgr_.SetBeautyState(CreateBeautyStateFromAppState());
    }
    ImGui::Separator();
}

void BeautyPanel::RenderSkinSmoothing() {
    ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    if (ImGui::CollapsingHeader("Skin Smoothing")) {
        bool changed = false;
        changed |= ImGui::Checkbox("Enable##skin", &state_.fx_skin);
        ImGui::SameLine();
        changed |= ImGui::Checkbox("Advanced", &state_.fx_skin_adv);

        if (state_.fx_skin) {
            if (!state_.fx_skin_adv) {
                // Simple mode
                changed |= ImGui::SliderFloat("Strength", &state_.fx_skin_strength, 0.0f, 1.0f);
            } else {
                // Advanced mode
                changed |= RenderAdvancedSkinControls();
            }
        }
        if (changed) {
            std::cout << "[DEBUG] UI: Beauty state changed, updating effects with smile_wrinkle_gain=" << state_.fx_skin_smile_wrinkle_gain << std::endl;
            effects_mgr_.SetBeautyState(CreateBeautyStateFromAppState());
        }
    }
}

bool BeautyPanel::RenderAdvancedSkinControls() {
    bool changed = false;
    changed |= ImGui::SliderFloat("Amount##skin", &state_.fx_skin_amount, 0.0f, 1.0f);
    changed |= ImGui::SliderFloat("Radius (px)", &state_.fx_skin_radius, 1.0f, 20.0f);
    changed |= ImGui::SliderFloat("Texture keep (0..1)", &state_.fx_skin_tex, 0.0f, 1.0f);
    changed |= ImGui::SliderFloat("Edge feather (px)", &state_.fx_skin_edge, 2.0f, 40.0f);

    ImGui::Separator();
    changed |= RenderWrinkleControls();
    return changed;
}

bool BeautyPanel::RenderWrinkleControls() {
    bool changed = false;
    changed |= ImGui::Checkbox("Wrinkle-aware", &state_.fx_skin_wrinkle);

    if (state_.fx_skin_wrinkle) {
        ImGui::Indent();

        // Sensitivity & boosts
        changed |= ImGui::SliderFloat("Wrinkle gain", &state_.fx_skin_wrinkle_gain, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Smile Wrinkle Suppression", &state_.fx_skin_smile_wrinkle_gain, 0.0f, 1.0f);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::cout << "[DEBUG] UI: Smile Wrinkle Suppression changed to " << state_.fx_skin_smile_wrinkle_gain << std::endl;
        }
        changed |= ImGui::SliderFloat("Smile boost", &state_.fx_skin_smile_boost, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Squint boost", &state_.fx_skin_squint_boost, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Forehead boost", &state_.fx_skin_forehead_boost, 0.0f, 2.0f);

        ImGui::Separator();

        // Region controls
        changed |= ImGui::Checkbox("Suppress chin/stubble", &state_.fx_wrinkle_suppress_lower);
        if (state_.fx_wrinkle_suppress_lower) {
            changed |= ImGui::SliderFloat("Lower-face ratio", &state_.fx_wrinkle_lower_ratio, 0.25f, 0.65f);
        }

        changed |= ImGui::Checkbox("Ignore glasses", &state_.fx_wrinkle_ignore_glasses);
        if (state_.fx_wrinkle_ignore_glasses) {
            changed |= ImGui::SliderFloat("Glasses margin (px)", &state_.fx_wrinkle_glasses_margin, 0.0f, 30.0f);
        }

        ImGui::Separator();

        // Advanced masking
        changed |= ImGui::SliderFloat("Wrinkle sensitivity", &state_.fx_wrinkle_keep_ratio, 0.05f, 10.60f);
        changed |= ImGui::Checkbox("Custom line width", &state_.fx_wrinkle_custom_scales);

        if (state_.fx_wrinkle_custom_scales) {
            changed |= ImGui::SliderFloat("Min width (px)", &state_.fx_wrinkle_min_px, 1.0f, 10.0f);
            changed |= ImGui::SliderFloat("Max width (px)", &state_.fx_wrinkle_max_px, 2.0f, 16.0f);
            if (state_.fx_wrinkle_max_px < state_.fx_wrinkle_min_px) {
                state_.fx_wrinkle_max_px = state_.fx_wrinkle_min_px;
            }
        }

        changed |= ImGui::Checkbox("Skin gate (YCbCr)", &state_.fx_wrinkle_use_skin_gate);
        if (state_.fx_wrinkle_use_skin_gate) {
            ImGui::Indent();
            changed |= ImGui::SliderFloat("Mask gain", &state_.fx_wrinkle_mask_gain, 0.5f, 3.0f);
            ImGui::Unindent();
        }

        changed |= ImGui::SliderFloat("Baseline boost", &state_.fx_wrinkle_baseline, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Neg atten cap", &state_.fx_wrinkle_neg_cap, 0.6f, 1.0f);

        ImGui::Separator();
        bool preview_changed = ImGui::Checkbox("Wrinkle-only preview", &state_.fx_wrinkle_preview);
        changed |= preview_changed;
        if (preview_changed) {
            effects_mgr_.SetWrinklePreview(state_.fx_wrinkle_preview);
        }

        ImGui::Unindent();
    }
    return changed;
}

void BeautyPanel::RenderLipEffects() {
    ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
    if (ImGui::CollapsingHeader("Lip Effects")) {
        RenderLipControls();
        RenderLipColorPresets();
    }
}

void BeautyPanel::RenderLipControls() {
    ImGui::Checkbox("Enable##lipstick", &state_.fx_lipstick);
    
    if (state_.fx_lipstick) {
        RenderLipSliders();
    }
}

void BeautyPanel::RenderLipSliders() {
    ImGui::SliderFloat("Alpha##lip", &state_.fx_lip_alpha, 0.0f, 1.0f);
    ImGui::SameLine();
    if (ImGui::Button("?##lip_alpha")) {
        ImGui::SetTooltip("Opacity of the lipstick effect");
    }
    
    ImGui::ColorEdit3("Color##lip", state_.fx_lip_color);
    ImGui::SliderFloat("Feather (px)", &state_.fx_lip_feather, 0.0f, 20.0f);
    ImGui::SameLine();
    if (ImGui::Button("?##lip_feather")) {
        ImGui::SetTooltip("Edge softening for natural blending");
    }
    
    ImGui::SliderFloat("Lightness", &state_.fx_lip_light, -1.0f, 1.0f);
    ImGui::SameLine();
    if (ImGui::Button("?##lip_light")) {
        ImGui::SetTooltip("Brightness adjustment: negative for darker, positive for lighter");
    }
    
    ImGui::SliderFloat("Band grow (px)", &state_.fx_lip_band, 0.0f, 12.0f);
    ImGui::SameLine();
    if (ImGui::Button("?##lip_band")) {
        ImGui::SetTooltip("Expand the lip detection area");
    }
}

void BeautyPanel::RenderLipColorPresets() {
    if (state_.fx_lipstick) {
        ImGui::Separator();
        ImGui::Text("Color Presets:");
        
        ApplyLipColorPreset("Classic Red", 0.8f, 0.1f, 0.3f);
        ImGui::SameLine();
        ApplyLipColorPreset("Pink", 1.0f, 0.4f, 0.6f);
        ImGui::SameLine();
        ApplyLipColorPreset("Berry", 0.6f, 0.2f, 0.4f);
        ImGui::SameLine();
        ApplyLipColorPreset("Natural", 0.9f, 0.6f, 0.5f);
    }
}

void BeautyPanel::ApplyLipColorPreset(const char* name, float r, float g, float b) {
    if (ImGui::Button(name)) {
        state_.fx_lip_color[0] = r;
        state_.fx_lip_color[1] = g;
        state_.fx_lip_color[2] = b;
    }
}

void BeautyPanel::RenderTeethWhitening() {
    ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
    if (ImGui::CollapsingHeader("Teeth Whitening")) {
        RenderTeethControls();
        RenderTeethPresets();
        RenderTeethTips();
    }
}

void BeautyPanel::RenderTeethControls() {
    ImGui::Checkbox("Enable##teeth", &state_.fx_teeth);
    
    if (state_.fx_teeth) {
        RenderTeethSliders();
    }
}

void BeautyPanel::RenderTeethSliders() {
    ImGui::SliderFloat("Amount##teeth", &state_.fx_teeth_strength, 0.0f, 1.0f);
    ImGui::SameLine();
    if (ImGui::Button("?##teeth_strength")) {
        ImGui::SetTooltip("Whitening intensity: 0=none, 1=maximum");
    }
    
    ImGui::SliderFloat("Avoid lips (px)", &state_.fx_teeth_margin, 0.0f, 12.0f);
    ImGui::SameLine();
    if (ImGui::Button("?##teeth_margin")) {
        ImGui::SetTooltip("Margin to avoid whitening lip areas");
    }
}

void BeautyPanel::RenderTeethPresets() {
    if (state_.fx_teeth) {
        ImGui::Separator();
        ImGui::Text("Presets:");
        
        ApplyTeethPreset("Subtle", 0.3f, 3.0f);
        ImGui::SameLine();
        ApplyTeethPreset("Medium", 0.5f, 3.0f);
        ImGui::SameLine();
        ApplyTeethPreset("Strong", 0.8f, 4.0f);
        ImGui::SameLine();
        ApplyTeethPreset("Reset", 0.5f, 3.0f);
    }
}

void BeautyPanel::ApplyTeethPreset(const char* name, float strength, float margin) {
    if (ImGui::Button(name)) {
        state_.fx_teeth_strength = strength;
        state_.fx_teeth_margin = margin;
    }
}

void BeautyPanel::RenderTeethTips() {
    if (state_.fx_teeth) {
        ImGui::Separator();
        ImGui::TextDisabled("Tips:");
        ImGui::TextDisabled("• Use subtle settings for natural results");
        ImGui::TextDisabled("• Increase margin if lips get whitened");
        ImGui::TextDisabled("• Works best with good lighting");
    }
}

void BeautyPanel::ApplyBeautyPreset(int preset_index, const char* preset_name) {
    BeautyState bs = CreateBeautyStateFromAppState();
    ApplyPreset(preset_index, bs);
    CopyBeautyStateToAppState(bs);
    std::cout << "Applied beauty preset: " << preset_name << std::endl;
}

BeautyState BeautyPanel::CreateBeautyStateFromAppState() {
    BeautyState bs;
    
    // Copy background settings
    bs.bg_mode = state_.bg_mode;
    bs.blur_strength = state_.blur_strength;
    bs.feather_px = state_.feather_px;
    bs.show_mask = state_.show_mask;
    
    // Copy beauty settings
    CopyBeautyFieldsToBeautyState(bs);
    
    return bs;
}

void BeautyPanel::CopyBeautyFieldsToBeautyState(BeautyState& bs) {
    // Basic skin settings
    bs.fx_skin = state_.fx_skin;
    bs.fx_skin_adv = state_.fx_skin_adv;
    // In simple mode, use fx_skin_strength; in advanced mode, use fx_skin_amount
    bs.fx_skin_amount = state_.fx_skin_adv ? state_.fx_skin_amount : state_.fx_skin_strength;
    bs.fx_skin_radius = state_.fx_skin_radius;
    bs.fx_skin_tex = state_.fx_skin_tex;
    bs.fx_skin_edge = state_.fx_skin_edge;
    bs.fx_adv_scale = state_.fx_adv_scale;
    bs.fx_adv_detail_preserve = state_.fx_adv_detail_preserve;

    
    // Wrinkle settings
    CopyWrinkleFieldsToBeautyState(bs);
    
    // Lip settings
    CopyLipFieldsToBeautyState(bs);
    
    // Teeth settings
    CopyTeethFieldsToBeautyState(bs);
}

void BeautyPanel::CopyWrinkleFieldsToBeautyState(BeautyState& bs) {
    bs.fx_skin_wrinkle = state_.fx_skin_wrinkle;
    bs.fx_skin_smile_boost = state_.fx_skin_smile_boost;
    bs.fx_skin_squint_boost = state_.fx_skin_squint_boost;
    bs.fx_skin_forehead_boost = state_.fx_skin_forehead_boost;
    bs.fx_skin_wrinkle_gain = state_.fx_skin_wrinkle_gain;
    bs.fx_skin_smile_wrinkle_gain = state_.fx_skin_smile_wrinkle_gain;
    bs.fx_wrinkle_suppress_lower = state_.fx_wrinkle_suppress_lower;
    bs.fx_wrinkle_lower_ratio = state_.fx_wrinkle_lower_ratio;
    bs.fx_wrinkle_ignore_glasses = state_.fx_wrinkle_ignore_glasses;
    bs.fx_wrinkle_glasses_margin = state_.fx_wrinkle_glasses_margin;
    bs.fx_wrinkle_keep_ratio = state_.fx_wrinkle_keep_ratio;
    bs.fx_wrinkle_custom_scales = state_.fx_wrinkle_custom_scales;
    bs.fx_wrinkle_min_px = state_.fx_wrinkle_min_px;
    bs.fx_wrinkle_max_px = state_.fx_wrinkle_max_px;
    bs.fx_wrinkle_preview = state_.fx_wrinkle_preview;
    bs.fx_wrinkle_use_skin_gate = state_.fx_wrinkle_use_skin_gate;
    bs.fx_wrinkle_mask_gain = state_.fx_wrinkle_mask_gain;
    bs.fx_wrinkle_baseline = state_.fx_wrinkle_baseline;
    bs.fx_wrinkle_neg_cap = state_.fx_wrinkle_neg_cap;
}

void BeautyPanel::CopyLipFieldsToBeautyState(BeautyState& bs) {
    bs.fx_lipstick = state_.fx_lipstick;
    bs.fx_lip_alpha = state_.fx_lip_alpha;
    bs.fx_lip_feather = state_.fx_lip_feather;
    bs.fx_lip_light = state_.fx_lip_light;
    bs.fx_lip_band = state_.fx_lip_band;
    bs.fx_lip_color[0] = state_.fx_lip_color[0];
    bs.fx_lip_color[1] = state_.fx_lip_color[1];
    bs.fx_lip_color[2] = state_.fx_lip_color[2];
}

void BeautyPanel::CopyTeethFieldsToBeautyState(BeautyState& bs) {
    bs.fx_teeth = state_.fx_teeth;
    bs.fx_teeth_strength = state_.fx_teeth_strength;
    bs.fx_teeth_margin = state_.fx_teeth_margin;
}

void BeautyPanel::CopyBeautyStateToAppState(const BeautyState& bs) {
    // Copy background settings
    state_.bg_mode = bs.bg_mode;
    state_.blur_strength = bs.blur_strength;
    state_.feather_px = bs.feather_px;
    state_.show_mask = bs.show_mask;
    
    // Copy beauty settings
    CopyBeautyStateFieldsToAppState(bs);
}

void BeautyPanel::CopyBeautyStateFieldsToAppState(const BeautyState& bs) {
    // Basic skin settings
    state_.fx_skin = bs.fx_skin;
    state_.fx_skin_adv = bs.fx_skin_adv;
    // In simple mode, sync to fx_skin_strength; in advanced mode, sync to fx_skin_amount
    if (state_.fx_skin_adv) {
        state_.fx_skin_amount = bs.fx_skin_amount;
    } else {
        state_.fx_skin_strength = bs.fx_skin_amount;
    }
    state_.fx_skin_radius = bs.fx_skin_radius;
    state_.fx_skin_tex = bs.fx_skin_tex;
    state_.fx_skin_edge = bs.fx_skin_edge;
    state_.fx_adv_scale = bs.fx_adv_scale;
    state_.fx_adv_detail_preserve = bs.fx_adv_detail_preserve;
    
    // Wrinkle settings
    CopyWrinkleStateToAppState(bs);
    
    // Lip settings
    CopyLipStateToAppState(bs);
    
    // Teeth settings
    CopyTeethStateToAppState(bs);
}

void BeautyPanel::CopyWrinkleStateToAppState(const BeautyState& bs) {
    state_.fx_skin_wrinkle = bs.fx_skin_wrinkle;
    state_.fx_skin_smile_boost = bs.fx_skin_smile_boost;
    state_.fx_skin_squint_boost = bs.fx_skin_squint_boost;
    state_.fx_skin_forehead_boost = bs.fx_skin_forehead_boost;
    state_.fx_skin_wrinkle_gain = bs.fx_skin_wrinkle_gain;
    state_.fx_skin_smile_wrinkle_gain = bs.fx_skin_smile_wrinkle_gain;
    state_.fx_wrinkle_suppress_lower = bs.fx_wrinkle_suppress_lower;
    state_.fx_wrinkle_lower_ratio = bs.fx_wrinkle_lower_ratio;
    state_.fx_wrinkle_ignore_glasses = bs.fx_wrinkle_ignore_glasses;
    state_.fx_wrinkle_glasses_margin = bs.fx_wrinkle_glasses_margin;
    state_.fx_wrinkle_keep_ratio = bs.fx_wrinkle_keep_ratio;
    state_.fx_wrinkle_custom_scales = bs.fx_wrinkle_custom_scales;
    state_.fx_wrinkle_min_px = bs.fx_wrinkle_min_px;
    state_.fx_wrinkle_max_px = bs.fx_wrinkle_max_px;
    state_.fx_wrinkle_preview = bs.fx_wrinkle_preview;
    state_.fx_wrinkle_use_skin_gate = bs.fx_wrinkle_use_skin_gate;
    state_.fx_wrinkle_mask_gain = bs.fx_wrinkle_mask_gain;
    state_.fx_wrinkle_baseline = bs.fx_wrinkle_baseline;
    state_.fx_wrinkle_neg_cap = bs.fx_wrinkle_neg_cap;
}

void BeautyPanel::CopyLipStateToAppState(const BeautyState& bs) {
    state_.fx_lipstick = bs.fx_lipstick;
    state_.fx_lip_alpha = bs.fx_lip_alpha;
    state_.fx_lip_feather = bs.fx_lip_feather;
    state_.fx_lip_light = bs.fx_lip_light;
    state_.fx_lip_band = bs.fx_lip_band;
    state_.fx_lip_color[0] = bs.fx_lip_color[0];
    state_.fx_lip_color[1] = bs.fx_lip_color[1];
    state_.fx_lip_color[2] = bs.fx_lip_color[2];
}

void BeautyPanel::CopyTeethStateToAppState(const BeautyState& bs) {
    state_.fx_teeth = bs.fx_teeth;
    state_.fx_teeth_strength = bs.fx_teeth_strength;
    state_.fx_teeth_margin = bs.fx_teeth_margin;
}

} // namespace segmecam