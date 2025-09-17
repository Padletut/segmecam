#pragma once

#include <string>
#include <memory>
#include <opencv2/opencv.hpp>
#include "mediapipe/framework/formats/landmark.pb.h"
#include "segmecam_face_effects.h"
#include "segmecam_composite.h"
#include "presets.h"

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
};

// Comprehensive effects manager for background replacement, face effects, and image processing
class EffectsManager {
public:
    EffectsManager();
    ~EffectsManager();
    
    // Core lifecycle
    int Initialize(const EffectsConfig& config);
    void Cleanup();
    
    // Main processing pipeline
    cv::Mat ProcessFrame(const cv::Mat& frame_bgr, 
                        const cv::Mat& segmentation_mask,
                        const mediapipe::NormalizedLandmarkList* face_landmarks = nullptr);
    
    // Background effects
    cv::Mat ApplyBackgroundEffect(const cv::Mat& frame_bgr, const cv::Mat& mask);
    cv::Mat ApplyBlurBackground(const cv::Mat& frame_bgr, const cv::Mat& mask, int blur_strength, float feather_px);
    cv::Mat ApplyImageBackground(const cv::Mat& frame_bgr, const cv::Mat& mask, const cv::Mat& bg_image);
    cv::Mat ApplySolidBackground(const cv::Mat& frame_bgr, const cv::Mat& mask, const cv::Scalar& color);
    
    // Face effects
    void ApplyFaceEffects(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks);
    void ApplySkinSmoothing(cv::Mat& frame_bgr, const FaceRegions& regions);
    void ApplySkinSmoothingAdvanced(cv::Mat& frame_bgr, const FaceRegions& regions, 
                                   const mediapipe::NormalizedLandmarkList& landmarks);
    void ApplyLipEffects(cv::Mat& frame_bgr, const FaceRegions& regions, 
                        const mediapipe::NormalizedLandmarkList& landmarks, const cv::Size& frame_size);
    void ApplyTeethWhitening(cv::Mat& frame_bgr, const FaceRegions& regions);
    
    // Beauty presets
    void ApplyBeautyPreset(int preset_index);
    void GetCurrentBeautyState(BeautyState& state) const;
    void SetBeautyState(const BeautyState& state);
    
    // Background settings
    void SetBackgroundMode(int mode); // 0=None, 1=Blur, 2=Image, 3=Solid
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
    bool IsOpenCLAvailable() const { return state_.opencl_available; }
    bool IsOpenCLEnabled() const { return state_.opencl_enabled; }
    
    // Auto processing scale
    void SetAutoProcessingScaleEnabled(bool enabled);
    void SetTargetFPS(float target_fps);
    void UpdateAutoProcessingScale(float current_fps);
    void UpdateTargetFPSFromCamera(float camera_fps); // NEW: Auto-set target based on camera
    bool IsAutoProcessingScaleEnabled() const;
    float GetCurrentFPS() const;
    float GetTargetFPS() const;
    float GetProcessingScale() const;
    
    // Debug and visualization
    void SetShowLandmarks(bool enabled);
    void DrawLandmarks(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks);
    cv::Mat VisualizeMask(const cv::Mat& mask);
    
    // Performance monitoring
    void UpdatePerformanceStats();
    double GetLastSmoothingTime() const { return state_.last_smoothing_time_ms; }
    double GetLastBackgroundTime() const { return state_.last_background_time_ms; }
    double GetAverageProcessingTime() const;
    void ResetPerformanceStats();
    
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
    
    // Performance tracking
    std::chrono::steady_clock::time_point last_perf_log_time_;
    double perf_sum_frame_ms_ = 0.0;
    double perf_sum_smooth_ms_ = 0.0;
    double perf_sum_bg_ms_ = 0.0;
    uint32_t perf_sum_frames_ = 0;
    
    // Auto processing scale
    bool auto_scale_enabled_ = false;
    float target_fps_ = 14.5f;
    float current_fps_ = 0.0f;
    std::chrono::steady_clock::time_point last_fps_update_;
    std::chrono::steady_clock::time_point last_scale_adjustment_;
    std::vector<float> fps_history_;
    static constexpr size_t FPS_HISTORY_SIZE = 10;
    
    // Helper methods
    FaceRegions ExtractFaceRegionsFromLandmarks(const mediapipe::NormalizedLandmarkList& landmarks, 
                                               const cv::Size& frame_size);
    cv::Mat ResizeMaskIfNeeded(const cv::Mat& mask, const cv::Size& target_size);
    cv::Scalar ConvertRGBColorToBGR(float r, float g, float b);
    void LogPerformanceStats();
    bool ShouldLogPerformance();
    
    // Processing scale optimization for skin smoothing
    void ApplySkinSmoothingWithProcessingScale(cv::Mat& frame_bgr, const FaceRegions& regions, 
                                              const mediapipe::NormalizedLandmarkList& landmarks);
};

} // namespace segmecam