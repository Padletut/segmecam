#include "include/ui/ui_panels.h"
#include "presets.h"
#include <iostream>

namespace segmecam {

// Beauty Panel Implementation
BeautyPanel::BeautyPanel(AppState& state)
    : UIPanel("Beauty"), state_(state) {
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
        // Create a BeautyState from current AppState
        segmecam::BeautyState bs;
        
        // Copy current background settings
        bs.bg_mode = state_.bg_mode;
        bs.blur_strength = state_.blur_strength;
        bs.feather_px = state_.feather_px;
        bs.show_mask = state_.show_mask;
        
        // Copy current beauty settings (only matching fields)
        bs.fx_skin = state_.fx_skin;
        bs.fx_skin_adv = state_.fx_skin_adv;
        bs.fx_skin_amount = state_.fx_skin_amount;
        bs.fx_skin_radius = state_.fx_skin_radius;
        bs.fx_skin_tex = state_.fx_skin_tex;
        bs.fx_skin_edge = state_.fx_skin_edge;
        bs.fx_adv_scale = state_.fx_adv_scale;
        bs.fx_adv_detail_preserve = state_.fx_adv_detail_preserve;
        
        // Wrinkle settings
        bs.fx_skin_wrinkle = state_.fx_skin_wrinkle;
        bs.fx_skin_smile_boost = state_.fx_skin_smile_boost;
        bs.fx_skin_squint_boost = state_.fx_skin_squint_boost;
        bs.fx_skin_forehead_boost = state_.fx_skin_forehead_boost;
        bs.fx_skin_wrinkle_gain = state_.fx_skin_wrinkle_gain;
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
        
        // Lip settings
        bs.fx_lipstick = state_.fx_lipstick;
        bs.fx_lip_alpha = state_.fx_lip_alpha;
        bs.fx_lip_feather = state_.fx_lip_feather;
        bs.fx_lip_light = state_.fx_lip_light;
        bs.fx_lip_band = state_.fx_lip_band;
        bs.fx_lip_color[0] = state_.fx_lip_color[0];
        bs.fx_lip_color[1] = state_.fx_lip_color[1];
        bs.fx_lip_color[2] = state_.fx_lip_color[2];
        
        // Teeth settings
        bs.fx_teeth = state_.fx_teeth;
        bs.fx_teeth_strength = state_.fx_teeth_strength;
        bs.fx_teeth_margin = state_.fx_teeth_margin;
        
        // Apply preset
        ApplyPreset(ui_preset_idx, bs);
        
        // Copy back to AppState (only matching fields)
        state_.bg_mode = bs.bg_mode;
        state_.blur_strength = bs.blur_strength;
        state_.feather_px = bs.feather_px;
        state_.show_mask = bs.show_mask;
        
        state_.fx_skin = bs.fx_skin;
        state_.fx_skin_adv = bs.fx_skin_adv;
        state_.fx_skin_amount = bs.fx_skin_amount;
        state_.fx_skin_radius = bs.fx_skin_radius;
        state_.fx_skin_tex = bs.fx_skin_tex;
        state_.fx_skin_edge = bs.fx_skin_edge;
        state_.fx_adv_scale = bs.fx_adv_scale;
        state_.fx_adv_detail_preserve = bs.fx_adv_detail_preserve;
        
        state_.fx_skin_wrinkle = bs.fx_skin_wrinkle;
        state_.fx_skin_smile_boost = bs.fx_skin_smile_boost;
        state_.fx_skin_squint_boost = bs.fx_skin_squint_boost;
        state_.fx_skin_forehead_boost = bs.fx_skin_forehead_boost;
        state_.fx_skin_wrinkle_gain = bs.fx_skin_wrinkle_gain;
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
        
        state_.fx_lipstick = bs.fx_lipstick;
        state_.fx_lip_alpha = bs.fx_lip_alpha;
        state_.fx_lip_feather = bs.fx_lip_feather;
        state_.fx_lip_light = bs.fx_lip_light;
        state_.fx_lip_band = bs.fx_lip_band;
        state_.fx_lip_color[0] = bs.fx_lip_color[0];
        state_.fx_lip_color[1] = bs.fx_lip_color[1];
        state_.fx_lip_color[2] = bs.fx_lip_color[2];
        
        state_.fx_teeth = bs.fx_teeth;
        state_.fx_teeth_strength = bs.fx_teeth_strength;
        state_.fx_teeth_margin = bs.fx_teeth_margin;
        
        std::cout << "Applied beauty preset: " << preset_names[ui_preset_idx] << std::endl;
    }
    
    ImGui::Separator();
}

void BeautyPanel::RenderPerformanceControls() {
    ImGui::Checkbox("OpenCL", &state_.use_opencl);
    ImGui::SameLine();
    ImGui::Checkbox("Perf log", &state_.perf_log);
    
    if (state_.perf_log) {
        ImGui::SameLine();
        ImGui::SliderInt("Interval (ms)", &state_.perf_log_interval_ms, 500, 10000);
    }
    
    ImGui::Separator();
}

void BeautyPanel::RenderSkinSmoothing() {
    ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    if (ImGui::CollapsingHeader("Skin Smoothing")) {
        ImGui::Checkbox("Enable##skin", &state_.fx_skin);
        ImGui::SameLine();
        ImGui::Checkbox("Advanced", &state_.fx_skin_adv);
        
        if (state_.fx_skin) {
            if (!state_.fx_skin_adv) {
                // Simple mode
                ImGui::SliderFloat("Strength", &state_.fx_skin_strength, 0.0f, 1.0f);
            } else {
                // Advanced mode
                RenderAdvancedSkinControls();
            }
        }
    }
}

void BeautyPanel::RenderAdvancedSkinControls() {
    ImGui::SliderFloat("Amount##skin", &state_.fx_skin_amount, 0.0f, 1.0f);
    ImGui::SliderFloat("Radius (px)", &state_.fx_skin_radius, 1.0f, 20.0f);
    ImGui::SliderFloat("Texture keep (0..1)", &state_.fx_skin_tex, 0.05f, 1.0f);
    ImGui::SliderFloat("Edge feather (px)", &state_.fx_skin_edge, 2.0f, 40.0f);
    
    ImGui::Separator();
    RenderWrinkleControls();
}

void BeautyPanel::RenderWrinkleControls() {
    ImGui::Checkbox("Wrinkle-aware", &state_.fx_skin_wrinkle);
    
    if (state_.fx_skin_wrinkle) {
        ImGui::Indent();
        
        // Sensitivity & boosts
        ImGui::SliderFloat("Wrinkle gain", &state_.fx_skin_wrinkle_gain, 0.0f, 8.0f);
        ImGui::SliderFloat("Smile boost", &state_.fx_skin_smile_boost, 0.0f, 1.0f);
        ImGui::SliderFloat("Squint boost", &state_.fx_skin_squint_boost, 0.0f, 1.0f);
        ImGui::SliderFloat("Forehead boost", &state_.fx_skin_forehead_boost, 0.0f, 2.0f);
        
        ImGui::Separator();
        
        // Region controls
        ImGui::Checkbox("Suppress chin/stubble", &state_.fx_wrinkle_suppress_lower);
        if (state_.fx_wrinkle_suppress_lower) {
            ImGui::SliderFloat("Lower-face ratio", &state_.fx_wrinkle_lower_ratio, 0.25f, 0.65f);
        }
        
        ImGui::Checkbox("Ignore glasses", &state_.fx_wrinkle_ignore_glasses);
        if (state_.fx_wrinkle_ignore_glasses) {
            ImGui::SliderFloat("Glasses margin (px)", &state_.fx_wrinkle_glasses_margin, 0.0f, 30.0f);
        }
        
        ImGui::Separator();
        
        // Advanced masking
        ImGui::SliderFloat("Wrinkle sensitivity", &state_.fx_wrinkle_keep_ratio, 0.05f, 10.60f);
        ImGui::Checkbox("Custom line width", &state_.fx_wrinkle_custom_scales);
        
        if (state_.fx_wrinkle_custom_scales) {
            ImGui::SliderFloat("Min width (px)", &state_.fx_wrinkle_min_px, 1.0f, 10.0f);
            ImGui::SliderFloat("Max width (px)", &state_.fx_wrinkle_max_px, 2.0f, 16.0f);
            if (state_.fx_wrinkle_max_px < state_.fx_wrinkle_min_px) {
                state_.fx_wrinkle_max_px = state_.fx_wrinkle_min_px;
            }
        }
        
        ImGui::Checkbox("Skin gate (YCbCr)", &state_.fx_wrinkle_use_skin_gate);
        if (state_.fx_wrinkle_use_skin_gate) {
            ImGui::Indent();
            ImGui::SliderFloat("Mask gain", &state_.fx_wrinkle_mask_gain, 0.5f, 3.0f);
            ImGui::Unindent();
        }
        
        ImGui::SliderFloat("Baseline boost", &state_.fx_wrinkle_baseline, 0.0f, 1.0f);
        ImGui::SliderFloat("Neg atten cap", &state_.fx_wrinkle_neg_cap, 0.6f, 1.0f);
        
        ImGui::Separator();
        ImGui::Checkbox("Wrinkle-only preview", &state_.fx_wrinkle_preview);
        
        ImGui::Unindent();
    }
}

void BeautyPanel::RenderLipEffects() {
    ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
    if (ImGui::CollapsingHeader("Lip Effects")) {
        ImGui::Checkbox("Enable##lipstick", &state_.fx_lipstick);
        
        if (state_.fx_lipstick) {
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
            
            // Lip color presets
            ImGui::Separator();
            ImGui::Text("Color Presets:");
            
            if (ImGui::Button("Classic Red")) {
                state_.fx_lip_color[0] = 0.8f;
                state_.fx_lip_color[1] = 0.1f;
                state_.fx_lip_color[2] = 0.3f;
            }
            ImGui::SameLine();
            
            if (ImGui::Button("Pink")) {
                state_.fx_lip_color[0] = 1.0f;
                state_.fx_lip_color[1] = 0.4f;
                state_.fx_lip_color[2] = 0.6f;
            }
            ImGui::SameLine();
            
            if (ImGui::Button("Berry")) {
                state_.fx_lip_color[0] = 0.6f;
                state_.fx_lip_color[1] = 0.2f;
                state_.fx_lip_color[2] = 0.4f;
            }
            ImGui::SameLine();
            
            if (ImGui::Button("Natural")) {
                state_.fx_lip_color[0] = 0.9f;
                state_.fx_lip_color[1] = 0.6f;
                state_.fx_lip_color[2] = 0.5f;
            }
        }
    }
}

void BeautyPanel::RenderTeethWhitening() {
    ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
    if (ImGui::CollapsingHeader("Teeth Whitening")) {
        ImGui::Checkbox("Enable##teeth", &state_.fx_teeth);
        
        if (state_.fx_teeth) {
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
            
            // Teeth whitening presets
            ImGui::Separator();
            ImGui::Text("Presets:");
            
            if (ImGui::Button("Subtle")) {
                state_.fx_teeth_strength = 0.3f;
                state_.fx_teeth_margin = 3.0f;
            }
            ImGui::SameLine();
            
            if (ImGui::Button("Medium")) {
                state_.fx_teeth_strength = 0.5f;
                state_.fx_teeth_margin = 3.0f;
            }
            ImGui::SameLine();
            
            if (ImGui::Button("Strong")) {
                state_.fx_teeth_strength = 0.8f;
                state_.fx_teeth_margin = 4.0f;
            }
            ImGui::SameLine();
            
            if (ImGui::Button("Reset")) {
                state_.fx_teeth_strength = 0.5f;
                state_.fx_teeth_margin = 3.0f;
            }
            
            ImGui::Separator();
            ImGui::TextDisabled("Tips:");
            ImGui::TextDisabled("• Use subtle settings for natural results");
            ImGui::TextDisabled("• Increase margin if lips get whitened");
            ImGui::TextDisabled("• Works best with good lighting");
        }
    }
}

} // namespace segmecam