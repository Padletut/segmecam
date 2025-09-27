#pragma once

#include <string>
#include <memory>
#include <opencv2/opencv.hpp>
#include "mediapipe/framework/formats/landmark.pb.h"
#include "effects/segmecam_face_effects.h"
#include "render/segmecam_composite.h"
#include "effects/presets.h"

// New modular includes
#include "effects/background/background_effects.h"
#include "effects/performance/performance_monitor.h"
#include "effects/config/effects_config.h"
#include "effects/face_processor.h"

namespace segmecam {

// Configuration for effects system
struct EffectsConfig {
    bool enable_opencl = true; // Default to enabled if available
    bool enable_face_effects = true;
    bool enable_background_effects = true;
    float default_processing_scale = 1.0f;
    bool enable_performance_logging = false;
    int performance_log_interval_ms = 5000;
};

// State tracking for effects system
struct EffectsState {
    bool is_initialized = false;
    bool opencl_available = false;
    bool opencl_enabled = false;

    // Last processed frame info
    int last_frame_width = 0;
    int last_frame_height = 0;

    // Performance tracking
    double last_smoothing_time_ms = 0.0;
    double last_background_time_ms = 0.0;
    double total_processing_time_ms = 0.0;
    int frames_processed = 0;
    
    // Debug state
    bool show_mask = false;
    bool show_landmarks = false;
    bool show_mesh = false;
    bool show_mesh_dense = false;
    
    // Latest facial expression metrics for debug display
    FacialExpressionMetrics last_facial_metrics;
};

// Comprehensive effects manager for background replacement, face effects, and image processing
class EffectsManager {
public:
    EffectsManager();
    ~EffectsManager();
    // Getter for current processing scale (for auto-scaling)
    float GetProcessingScale() const { return beauty_state_.fx_adv_scale; }
    
    // Getter for latest facial metrics (for debug display)
    const FacialExpressionMetrics& GetLastFacialMetrics() const { return state_.last_facial_metrics; }
    
    // Core lifecycle
    int Initialize(const EffectsConfig& config);
    void Cleanup();
    
    // Main processing pipeline
    cv::Mat ProcessFrame(const cv::Mat& frame_bgr, 
                        const cv::Mat& segmentation_mask,
                        const mediapipe::NormalizedLandmarkList* face_landmarks = nullptr,
                        const mediapipe::ClassificationList* blendshapes = nullptr);
    
    // Background effects - delegated to BackgroundEffects module
    cv::Mat ApplyBackgroundEffect(const cv::Mat& frame_bgr, const cv::Mat& mask) {
        return background_effects_->ApplyBackgroundEffect(frame_bgr, mask, beauty_state_, state_.opencl_enabled);
    }
    cv::Mat ApplyBlurBackground(const cv::Mat& frame_bgr, const cv::Mat& mask, int blur_strength, float feather_px, float scale) {
        return background_effects_->ApplyBlurBackground(frame_bgr, mask, blur_strength, feather_px, state_.opencl_enabled, scale);
    }
    cv::Mat ApplyImageBackground(const cv::Mat& frame_bgr, const cv::Mat& mask, const cv::Mat& bg_image, float scale) {
        return background_effects_->ApplyImageBackground(frame_bgr, mask, bg_image, state_.opencl_enabled, scale);
    }
    cv::Mat ApplySolidBackground(const cv::Mat& frame_bgr, const cv::Mat& mask, const cv::Scalar& color, float scale) {
        return background_effects_->ApplySolidBackground(frame_bgr, mask, color, state_.opencl_enabled, scale);
    }
    
    // Face effects
    void ApplyFaceEffects(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks, const mediapipe::ClassificationList* blendshapes = nullptr);
    void ApplySkinSmoothing(cv::Mat& frame_bgr, const FaceRegions& regions);
    void ApplySkinSmoothingAdvanced(cv::Mat& frame_bgr, const FaceRegions& regions, 
                                   const mediapipe::NormalizedLandmarkList& landmarks, const mediapipe::ClassificationList* blendshapes = nullptr);
    void ApplyLipEffects(cv::Mat& frame_bgr, const FaceRegions& regions, 
                        const mediapipe::NormalizedLandmarkList& landmarks, const cv::Size& frame_size);
    void ApplyTeethWhitening(cv::Mat& frame_bgr, const FaceRegions& regions);
    
    // Beauty presets - delegated to EffectsConfiguration module
    void ApplyBeautyPreset(int preset_index) {
        effects_config_->ApplyBeautyPreset(preset_index);
    }
    void GetCurrentBeautyState(BeautyState& state) const {
        effects_config_->GetCurrentBeautyState(state);
    }
    void SetBeautyState(const BeautyState& state) {
        effects_config_->SetBeautyState(state);
    }
    
    // Background settings
    void SetBackgroundMode(int mode) {
        effects_config_->SetBackgroundMode(mode);
    }
    void SetBlurStrength(int strength) {
        effects_config_->SetBlurStrength(strength);
    }
    void SetFeatherAmount(float feather_px) {
        effects_config_->SetFeatherAmount(feather_px);
    }
    void SetBackgroundImage(const cv::Mat& image) {
        background_image_ = image.clone();
        effects_config_->SetBackgroundImage(image);
        if (background_effects_) {
            background_effects_->SetBackgroundImage(image);
        }
    }
    void SetBackgroundImageFromPath(const std::string& path) {
        effects_config_->SetBackgroundImageFromPath(path);
        // The effects_config_->SetBackgroundImageFromPath will call SetBackgroundImage internally,
        // which will set it on background_effects_
    }
    void SetSolidBackgroundColor(float r, float g, float b) {
        effects_config_->SetSolidBackgroundColor(r, g, b);
    }
    void SetShowMask(bool enabled) {
        effects_config_->SetShowMask(enabled);
    }
    
    // Skin smoothing controls
    void SetSkinSmoothingEnabled(bool enabled) {
        effects_config_->SetSkinSmoothingEnabled(enabled);
        beauty_state_.fx_skin = enabled;
    }
    void SetSkinSmoothingStrength(float strength) {
        effects_config_->SetSkinSmoothingStrength(strength);
    }
    void SetSkinSmoothingAdvanced(bool advanced) {
        effects_config_->SetSkinSmoothingAdvanced(advanced);
        beauty_state_.fx_skin_adv = advanced;
    }
    void SetSkinSmoothingAmount(float amount) {
        effects_config_->SetSkinSmoothingAmount(amount);
        beauty_state_.fx_skin_amount = amount;
    }
    void SetSkinSmoothingRadius(float radius_px) {
        effects_config_->SetSkinSmoothingRadius(radius_px);
    }
    void SetSkinTexturePreservation(float texture_thresh) {
        effects_config_->SetSkinTexturePreservation(texture_thresh);
    }
    void SetSkinEdgeFeather(float edge_feather_px) {
        effects_config_->SetSkinEdgeFeather(edge_feather_px);
    }
    
    // Wrinkle-aware controls
    void SetWrinkleAwareEnabled(bool enabled) {
        effects_config_->SetWrinkleAwareEnabled(enabled);
    }
    void SetWrinkleGain(float gain) {
        effects_config_->SetWrinkleGain(gain);
    }
    void SetSmileWrinkleGain(float gain) {
        effects_config_->SetSmileWrinkleGain(gain);
    }
    void SetSmileBoost(float boost) {
        effects_config_->SetSmileBoost(boost);
    }
    void SetSquintBoost(float boost) {
        effects_config_->SetSquintBoost(boost);
    }
    void SetForeheadBoost(float boost) {
        effects_config_->SetForeheadBoost(boost);
    }
    void SetSuppressLowerFace(bool enabled) {
        effects_config_->SetSuppressLowerFace(enabled);
    }
    void SetLowerFaceRatio(float ratio) {
        effects_config_->SetLowerFaceRatio(ratio);
    }
    void SetIgnoreGlasses(bool enabled) {
        effects_config_->SetIgnoreGlasses(enabled);
    }
    void SetGlassesMargin(float margin_px) {
        effects_config_->SetGlassesMargin(margin_px);
    }
    void SetWrinkleSensitivity(float keep_ratio) {
        effects_config_->SetWrinkleSensitivity(keep_ratio);
    }
    void SetCustomWrinkleScales(bool enabled) {
        effects_config_->SetCustomWrinkleScales(enabled);
    }
    void SetWrinkleMinWidth(float min_px) {
        effects_config_->SetWrinkleMinWidth(min_px);
    }
    void SetWrinkleMaxWidth(float max_px) {
        effects_config_->SetWrinkleMaxWidth(max_px);
    }
    void SetWrinkleSkinGate(bool enabled) {
        effects_config_->SetWrinkleSkinGate(enabled);
    }
    void SetWrinkleMaskGain(float gain) {
        effects_config_->SetWrinkleMaskGain(gain);
    }
    void SetWrinkleBaselineBoost(float boost) {
        effects_config_->SetWrinkleBaselineBoost(boost);
    }
    void SetWrinkleNegativeCap(float cap) {
        effects_config_->SetWrinkleNegativeCap(cap);
    }
    void SetWrinklePreview(bool enabled) {
        effects_config_->SetWrinklePreview(enabled);
        beauty_state_.fx_wrinkle_preview = enabled;
    }
    
    // Advanced processing controls
    void SetProcessingScale(float scale) {
        effects_config_->SetProcessingScale(scale);
        beauty_state_.fx_adv_scale = std::clamp(scale, 0.4f, 1.0f);
    }
    void SetDetailPreservation(float preserve) {
        effects_config_->SetDetailPreservation(preserve);
    }
    
    // Lip effects controls
    void SetLipstickEnabled(bool enabled) {
        effects_config_->SetLipstickEnabled(enabled);
    }
    void SetLipAlpha(float alpha) {
        effects_config_->SetLipAlpha(alpha);
    }
    void SetLipFeather(float feather_px) {
        effects_config_->SetLipFeather(feather_px);
    }
    void SetLipLightness(float lightness) {
        effects_config_->SetLipLightness(lightness);
    }
    void SetLipBandGrow(float band_px) {
        effects_config_->SetLipBandGrow(band_px);
    }
    void SetLipColor(float r, float g, float b) {
        effects_config_->SetLipColor(r, g, b);
    }
    
    // Teeth whitening controls
    void SetTeethWhiteningEnabled(bool enabled) {
        effects_config_->SetTeethWhiteningEnabled(enabled);
    }
    void SetTeethWhiteningStrength(float strength) {
        effects_config_->SetTeethWhiteningStrength(strength);
    }
    void SetTeethMargin(float margin_px) {
        effects_config_->SetTeethMargin(margin_px);
    }
    
    // OpenCL acceleration
    void SetOpenCLEnabled(bool enabled) {
        effects_config_->SetOpenCLEnabled(enabled);
    }
    bool IsOpenCLAvailable() const { return state_.opencl_available; }
    bool IsOpenCLEnabled() const { return state_.opencl_enabled; }
    
    // Auto processing scale
    void SetAutoProcessingScaleEnabled(bool enabled) {
        performance_monitor_->SetAutoProcessingScaleEnabled(enabled);
    }
    void SetTargetFPS(float target_fps) {
        performance_monitor_->SetTargetFPS(target_fps);
    }
    void UpdateAutoProcessingScale(float current_fps) {
        performance_monitor_->UpdateAutoProcessingScale(current_fps);
    }

    // Wire up callback after construction
    void WireAutoScaleCallback() {
        performance_monitor_->SetProcessingScaleCallback([this](float scale) {
            this->SetProcessingScale(scale);
        });
        performance_monitor_->SetProcessingScaleGetter([this]() {
            return this->GetProcessingScale();
        });
    }
    void UpdateTargetFPSFromCamera(float camera_fps) {
        performance_monitor_->UpdateTargetFPSFromCamera(camera_fps);
    }
    bool IsAutoProcessingScaleEnabled() const {
        return performance_monitor_->IsAutoProcessingScaleEnabled();
    }
    float GetCurrentFPS() const {
        return performance_monitor_->GetCurrentFPS();
    }
    float GetTargetFPS() const {
        return performance_monitor_->GetTargetFPS();
    }
    
    // Debug and visualization - delegated to EffectsConfiguration module
    void SetShowLandmarks(bool enabled) {
        state_.show_landmarks = enabled;
    }
    void SetShowMesh(bool enabled) {
        state_.show_mesh = enabled;
    }
    void SetShowMeshDense(bool enabled) {
        state_.show_mesh_dense = enabled;
    }
    void DrawLandmarks(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks) {
        face_processor_->DrawLandmarks(frame_bgr, landmarks);
    }
    void DrawMesh(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks) {
        face_processor_->DrawMesh(frame_bgr, landmarks, state_.show_mesh_dense);
    }
    cv::Mat VisualizeMask(const cv::Mat& mask) {
        return VisualizeMaskRGB(mask);
    }
    
    // Performance monitoring
    void UpdatePerformanceStats();
    double GetLastSmoothingTime() const { return state_.last_smoothing_time_ms; }
    double GetLastBackgroundTime() const { return state_.last_background_time_ms; }
    double GetAverageProcessingTime() const {
        return performance_monitor_->GetAverageProcessingTime();
    }
    void ResetPerformanceStats() {
        performance_monitor_->ResetPerformanceStats();
    }
    
    // State access
    const EffectsState& GetState() const { return state_; }
    const EffectsConfig& GetConfig() const { return config_; }
    
    // Background image management
    bool LoadBackgroundImage(const std::string& path);
    void ClearBackgroundImage();
    bool HasBackgroundImage() const { return !background_image_.empty(); }

private:
    // Configuration and state
    EffectsConfig config_;
    EffectsState state_;
    BeautyState beauty_state_;
    
    // Background image storage
    cv::Mat background_image_;
    
    // New modular components
    std::unique_ptr<BackgroundEffects> background_effects_;
    std::unique_ptr<PerformanceMonitor> performance_monitor_;
    std::unique_ptr<EffectsConfiguration> effects_config_;
    std::unique_ptr<FaceProcessor> face_processor_;
    
    // Helper methods
    FaceRegions ExtractFaceRegionsFromLandmarks(const mediapipe::NormalizedLandmarkList& landmarks, 
                                               const cv::Size& frame_size);
    cv::Mat ResizeMaskIfNeeded(const cv::Mat& mask, const cv::Size& target_size);
    cv::Scalar ConvertRGBColorToBGR(float r, float g, float b);
    void LogPerformanceStats();
    bool ShouldLogPerformance();
    
    // ProcessFrame helper methods
    void LogDebugInputFrame(int frame_count, const cv::Mat& frame_bgr);
    void ProcessFaceEffects(cv::Mat& processed_frame, const mediapipe::NormalizedLandmarkList* face_landmarks, const mediapipe::ClassificationList* blendshapes = nullptr);
    cv::Mat ProcessBackgroundEffects(const cv::Mat& processed_frame, const cv::Mat& segmentation_mask);
    void UpdatePerformanceTracking(const std::chrono::steady_clock::time_point& start_time);
    void LogDebugOutputFrame(int frame_count, const cv::Mat& result);
    
    // Landmark drawing helpers
    template<size_t N>
    void DrawConnections(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks, 
                        const std::array<std::array<int, 2>, N>& connections, const cv::Scalar& color);
    
    // Processing scale optimization for skin smoothing
    void ApplySkinSmoothingWithProcessingScale(cv::Mat& frame_bgr, const FaceRegions& regions, 
                                              const mediapipe::NormalizedLandmarkList& landmarks, const mediapipe::ClassificationList* blendshapes = nullptr);
    cv::Rect CalculateProcessingROI(const FaceRegions& regions, const cv::Size& frame_size);
    void ApplyFullResolutionSkinSmoothing(cv::Mat& frame_bgr, const FaceRegions& regions, 
                                         const mediapipe::NormalizedLandmarkList& landmarks);
    
    // Helper method to create SkinSmoothingConfig from current beauty state
    SkinSmoothingConfig CreateSkinSmoothingConfig(float scale = 1.0f) const;
    
    FaceRegions TransformFaceRegionsToScaledROI(const FaceRegions& regions, const cv::Rect& roi);
    FaceRegions ShiftFaceRegionsToROI(const FaceRegions& regions, const cv::Rect& roi);
    FaceRegions ScaleFaceRegions(const FaceRegions& regions, float scale);
    std::vector<cv::Point> ShiftPolygon(const std::vector<cv::Point>& poly, int dx, int dy);
    std::vector<cv::Point> ScalePolygon(const std::vector<cv::Point>& poly, float scale);
    mediapipe::NormalizedLandmarkList TransformLandmarksToROI(const mediapipe::NormalizedLandmarkList& landmarks, 
                                                             const cv::Rect& roi, const cv::Size& frame_size);
    void ProcessAndUpsampleROI(cv::Mat& frame_bgr, const cv::Rect& roi, const FaceRegions& fr_small, 
                              const mediapipe::NormalizedLandmarkList& lms_roi);
    cv::Mat DownscaleROI(const cv::Mat& roi_bgr);
    void ApplySkinSmoothingToScaledImage(cv::Mat& small, const FaceRegions& fr_small, 
                                        const mediapipe::NormalizedLandmarkList& lms_roi);
    cv::Mat UpsampleProcessedImage(const cv::Mat& small, const cv::Size& target_size);
    void ApplyDetailPreservationIfNeeded(cv::Mat& up, const cv::Mat& roi_bgr, const FaceRegions& fr_roi);
    void ApplyDetailPreservation(cv::Mat& up, const cv::Mat& roi_bgr, const FaceRegions& fr_roi, float dp);
    cv::Mat CreateFaceMask(const FaceRegions& fr_roi, const cv::Size& size);
    
    // Auto processing scale helpers
    void UpdateFPSHistory(float current_fps);
    bool HasEnoughFPSSamples() const;
    bool ShouldAdjustScale(const std::chrono::steady_clock::time_point& now) const;
    float CalculateAverageFPS() const;
    float CalculateScaleAdjustment(float avg_fps) const;
    void ApplyScaleAdjustment(float scale_adjustment, const std::chrono::steady_clock::time_point& now);
    void TrimFPSHistoryForStability();
    
    // Background effect helper methods
    cv::Mat ApplyMaskVisualization(const cv::Mat& resized_mask);
    cv::Mat ApplyBackgroundModeEffect(const cv::Mat& frame_bgr, const cv::Mat& resized_mask);
    cv::Mat ApplyDefaultBackgroundEffect(const cv::Mat& frame_bgr);
};

} // namespace segmecam