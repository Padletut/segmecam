#include "include/effects/effects_manager.h"
#include "include/render/segmecam_composite.h"
#include "mediapipe/tasks/cc/vision/face_landmarker/face_landmarks_connections.h"

#include <iostream>
#include <chrono>
#include <algorithm>
#include <thread>
#include <opencv2/core/ocl.hpp>

namespace segmecam {

EffectsManager::EffectsManager() {
    // Initialize with default beauty state
    beauty_state_ = BeautyState{};
    last_perf_log_time_ = std::chrono::steady_clock::now();
}

EffectsManager::~EffectsManager() {
    Cleanup();
}

int EffectsManager::Initialize(const EffectsConfig& config) {
    config_ = config;
    state_ = EffectsState{}; // Reset state
    
    std::cout << "✨ Initializing Effects Manager..." << std::endl;
    
    // Enable OpenCV multi-threading optimizations
    int num_cores = std::thread::hardware_concurrency();
    if (num_cores > 1) {
        // Use all available cores for OpenCV operations
        cv::setNumThreads(num_cores);
        std::cout << "🧵 OpenCV multi-threading enabled with " << num_cores << " threads" << std::endl;
        
        // Enable parallel processing if available
        cv::setUseOptimized(true);
        std::cout << "⚡ OpenCV optimized operations enabled" << std::endl;
    } else {
        cv::setNumThreads(0); // Use OpenCV default
        std::cout << "🧵 OpenCV using default threading" << std::endl;
    }
    
    // Check OpenCL availability and enable by default if available
    state_.opencl_available = cv::ocl::haveOpenCL();
    
    // Auto-enable OpenCL if available (respecting user config if explicitly disabled)
    if (state_.opencl_available) {
        state_.opencl_enabled = config_.enable_opencl; // Use config preference
        std::cout << "🚀 OpenCL available for acceleration" << std::endl;
        
        if (state_.opencl_enabled) {
            cv::ocl::setUseOpenCL(true);
            std::cout << "✅ OpenCL acceleration enabled" << std::endl;
        } else {
            cv::ocl::setUseOpenCL(false);
            std::cout << "⚪ OpenCL acceleration disabled by configuration" << std::endl;
        }
    } else {
        state_.opencl_enabled = false;
        cv::ocl::setUseOpenCL(false);
        std::cout << "⚠️  OpenCL not available" << std::endl;
    }
    
    // Initialize default settings
    beauty_state_.fx_adv_scale = config_.default_processing_scale;
    
    // Performance optimization info
    std::cout << "🚀 Performance optimizations active:" << std::endl;
    std::cout << "   • Multi-threading: " << (num_cores > 1 ? "✅" : "❌") << std::endl;
    std::cout << "   • OpenCL acceleration: " << (state_.opencl_enabled ? "✅" : "❌") << std::endl;
    std::cout << "   • Adaptive resolution scaling: ✅" << std::endl;
    std::cout << "   • Optimized operations: ✅" << std::endl;
    
    state_.is_initialized = true;
    std::cout << "✅ Effects Manager initialized successfully!" << std::endl;
    
    return 0;
}

void EffectsManager::LogDebugInputFrame(int frame_count, const cv::Mat& frame_bgr) {
    if (frame_count <= 3 && !frame_bgr.empty()) {
        cv::Vec3b input_pixel = frame_bgr.at<cv::Vec3b>(frame_bgr.rows/2, frame_bgr.cols/2);
        std::cout << "🔍 EFFECTS INPUT Frame " << frame_count << " - BGR format: " << frame_bgr.type() 
                  << " center pixel: [" << (int)input_pixel[0] << "," << (int)input_pixel[1] << "," << (int)input_pixel[2] << "]" << std::endl;
    }
}

cv::Mat EffectsManager::ProcessFrame(const cv::Mat& frame_bgr,
                                    const cv::Mat& segmentation_mask,
                                    const mediapipe::NormalizedLandmarkList* face_landmarks) {
    if (!state_.is_initialized) {
        return frame_bgr.clone();
    }
    
    static int debug_frame_count = 0;
    debug_frame_count++;
    
    LogDebugInputFrame(debug_frame_count, frame_bgr);
    
    auto start_time = std::chrono::steady_clock::now();
    state_.last_frame_width = frame_bgr.cols;
    state_.last_frame_height = frame_bgr.rows;
    
    cv::Mat processed_frame = frame_bgr.clone();
    
    ProcessFaceEffects(processed_frame, face_landmarks);
    cv::Mat result = ProcessBackgroundEffects(processed_frame, segmentation_mask);
    
    UpdatePerformanceTracking(start_time);
    LogDebugOutputFrame(debug_frame_count, result);
    
    return result;
}

void EffectsManager::ProcessFaceEffects(cv::Mat& processed_frame, const mediapipe::NormalizedLandmarkList* face_landmarks) {
    if (config_.enable_face_effects && face_landmarks && face_landmarks->landmark_size() > 0) {
        auto smooth_start = std::chrono::steady_clock::now();
        ApplyFaceEffects(processed_frame, *face_landmarks);
        auto smooth_end = std::chrono::steady_clock::now();
        state_.last_smoothing_time_ms = std::chrono::duration<double, std::milli>(smooth_end - smooth_start).count();
        perf_sum_smooth_ms_ += state_.last_smoothing_time_ms;
    } else {
        state_.last_smoothing_time_ms = 0.0;
    }
}

cv::Mat EffectsManager::ProcessBackgroundEffects(const cv::Mat& processed_frame, const cv::Mat& segmentation_mask) {
    if (config_.enable_background_effects && !segmentation_mask.empty()) {
        auto bg_start = std::chrono::steady_clock::now();
        cv::Mat result = ApplyBackgroundEffect(processed_frame, segmentation_mask);
        auto bg_end = std::chrono::steady_clock::now();
        state_.last_background_time_ms = std::chrono::duration<double, std::milli>(bg_end - bg_start).count();
        perf_sum_bg_ms_ += state_.last_background_time_ms;
        return result;
    } else {
        // No background effects, return BGR frame as-is (ApplicationRun will convert to RGB for display)
        state_.last_background_time_ms = 0.0;
        return processed_frame.clone();
    }
}

void EffectsManager::UpdatePerformanceTracking(const std::chrono::steady_clock::time_point& start_time) {
    auto end_time = std::chrono::steady_clock::now();
    state_.total_processing_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    perf_sum_frame_ms_ += state_.total_processing_time_ms;
    state_.frames_processed++;
    perf_sum_frames_++;
    
    // Log performance if needed
    if (config_.enable_performance_logging && ShouldLogPerformance()) {
        LogPerformanceStats();
    }
}

void EffectsManager::LogDebugOutputFrame(int frame_count, const cv::Mat& result) {
    if (frame_count <= 3 && !result.empty()) {
        cv::Vec3b output_pixel = result.at<cv::Vec3b>(result.rows/2, result.cols/2);
        std::cout << "🔍 EFFECTS OUTPUT Frame " << frame_count << " - format: " << result.type() 
                  << " center pixel: [" << (int)output_pixel[0] << "," << (int)output_pixel[1] << "," << (int)output_pixel[2] << "]" 
                  << " (should be RGB)" << std::endl;
    }
}

cv::Mat EffectsManager::ApplyBackgroundEffect(const cv::Mat& frame_bgr, const cv::Mat& mask) {
    cv::Mat resized_mask = ResizeMaskIfNeeded(mask, frame_bgr.size());
    
    // Check if user wants to show mask visualization (overrides all other background effects)
    if (state_.show_mask && !resized_mask.empty()) {
        return VisualizeMask(resized_mask);
    }
    
    switch (beauty_state_.bg_mode) {
        case 1: // Blur
            return ApplyBlurBackground(frame_bgr, resized_mask, beauty_state_.blur_strength, beauty_state_.feather_px);
        case 2: // Image
            if (!background_image_.empty()) {
                return ApplyImageBackground(frame_bgr, resized_mask, background_image_);
            }
            break;
        case 3: // Solid Color
            {
                cv::Scalar solid_color = ConvertRGBColorToBGR(
                    beauty_state_.solid_color[0], 
                    beauty_state_.solid_color[1], 
                    beauty_state_.solid_color[2]
                );
                return ApplySolidBackground(frame_bgr, resized_mask, solid_color);
            }
        default: // None
            break;
    }
    
    // No background effect or fallback - convert BGR to RGB for display
    cv::Mat rgb;
    cv::cvtColor(frame_bgr, rgb, cv::COLOR_BGR2RGB);
    
    // Debug output for no-background path
    static int no_bg_debug_count = 0;
    no_bg_debug_count++;
    if (no_bg_debug_count <= 2 && !frame_bgr.empty() && !rgb.empty()) {
        cv::Vec3b bgr_pixel = frame_bgr.at<cv::Vec3b>(frame_bgr.rows/2, frame_bgr.cols/2);
        cv::Vec3b rgb_pixel = rgb.at<cv::Vec3b>(rgb.rows/2, rgb.cols/2);
        std::cout << "🔍 NO-BG CONVERSION " << no_bg_debug_count << " - Input BGR: [" 
                  << (int)bgr_pixel[0] << "," << (int)bgr_pixel[1] << "," << (int)bgr_pixel[2] 
                  << "] -> Output RGB: [" << (int)rgb_pixel[0] << "," << (int)rgb_pixel[1] << "," << (int)rgb_pixel[2] << "]" << std::endl;
    }
    
    return rgb;
}

cv::Mat EffectsManager::ApplyBlurBackground(const cv::Mat& frame_bgr, const cv::Mat& mask, 
                                           int blur_strength, float feather_px) {
    return CompositeBlurBackgroundBGR_Accel(frame_bgr, mask, blur_strength, feather_px, 
                                           state_.opencl_enabled, beauty_state_.fx_adv_scale);
}

cv::Mat EffectsManager::ApplyImageBackground(const cv::Mat& frame_bgr, const cv::Mat& mask, const cv::Mat& bg_image) {
    return CompositeImageBackgroundBGR_Accel(frame_bgr, mask, bg_image, 
                                           state_.opencl_enabled, beauty_state_.fx_adv_scale);
}

cv::Mat EffectsManager::ApplySolidBackground(const cv::Mat& frame_bgr, const cv::Mat& mask, const cv::Scalar& color) {
    return CompositeSolidBackgroundBGR_Accel(frame_bgr, mask, color, 
                                           state_.opencl_enabled, beauty_state_.fx_adv_scale);
}

void EffectsManager::ApplyFaceEffects(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks) {
    // Extract face regions from landmarks
    FaceRegions regions = ExtractFaceRegionsFromLandmarks(landmarks, frame_bgr.size());
    
    // Draw landmarks overlay if enabled
    if (state_.show_landmarks) {
        DrawLandmarks(frame_bgr, landmarks);
    }
    
    // Apply skin smoothing
    if (beauty_state_.fx_skin) {
        if (beauty_state_.fx_skin_adv) {
            ApplySkinSmoothingAdvanced(frame_bgr, regions, landmarks);
        } else {
            ApplySkinSmoothing(frame_bgr, regions);
        }
    }
    
    // Apply lip effects
    if (beauty_state_.fx_lipstick) {
        ApplyLipEffects(frame_bgr, regions, landmarks, frame_bgr.size());
    }
    
    // Apply teeth whitening
    if (beauty_state_.fx_teeth) {
        ApplyTeethWhitening(frame_bgr, regions);
    }
}

void EffectsManager::ApplySkinSmoothing(cv::Mat& frame_bgr, const FaceRegions& regions) {
    ApplySkinSmoothingBGR(frame_bgr, regions, beauty_state_.fx_skin_amount, state_.opencl_enabled);
}

void EffectsManager::ApplySkinSmoothingAdvanced(cv::Mat& frame_bgr, const FaceRegions& regions, 
                                               const mediapipe::NormalizedLandmarkList& landmarks) {
    // Use user-configured processing scale directly for stability
    float effective_scale = beauty_state_.fx_adv_scale;
    
    // Check if processing scale optimization should be used
    if (effective_scale < 0.999f) {
        ApplySkinSmoothingWithProcessingScale(frame_bgr, regions, landmarks);
    } else {
        // Full resolution processing
        ApplySkinSmoothingAdvBGR(
            frame_bgr, regions,
            beauty_state_.fx_skin_amount,
            beauty_state_.fx_skin_radius,
            beauty_state_.fx_skin_tex,
            beauty_state_.fx_skin_edge,
            &landmarks,
            beauty_state_.fx_skin_smile_boost,
            beauty_state_.fx_skin_squint_boost,
            beauty_state_.fx_skin_forehead_boost,
            beauty_state_.fx_skin_wrinkle_gain,
            beauty_state_.fx_wrinkle_suppress_lower,
            beauty_state_.fx_wrinkle_lower_ratio,
            beauty_state_.fx_wrinkle_ignore_glasses,
            beauty_state_.fx_wrinkle_glasses_margin,
            beauty_state_.fx_wrinkle_keep_ratio,
            beauty_state_.fx_wrinkle_custom_scales ? beauty_state_.fx_wrinkle_min_px : -1.0f,
            beauty_state_.fx_wrinkle_custom_scales ? beauty_state_.fx_wrinkle_max_px : -1.0f,
            8.0f, // forehead_margin_px
            beauty_state_.fx_wrinkle_preview,
            beauty_state_.fx_wrinkle_baseline,
            beauty_state_.fx_wrinkle_use_skin_gate,
            beauty_state_.fx_wrinkle_mask_gain,
            beauty_state_.fx_wrinkle_neg_cap
        );
    }
}

void EffectsManager::ApplyLipEffects(cv::Mat& frame_bgr, const FaceRegions& regions, 
                                    const mediapipe::NormalizedLandmarkList& landmarks, const cv::Size& frame_size) {
    cv::Scalar lip_color_bgr(
        beauty_state_.fx_lip_color[2] * 255, // B
        beauty_state_.fx_lip_color[1] * 255, // G  
        beauty_state_.fx_lip_color[0] * 255  // R
    );
    
    ApplyLipRefinerBGR(
        frame_bgr, regions, lip_color_bgr,
        beauty_state_.fx_lip_alpha,
        beauty_state_.fx_lip_feather,
        beauty_state_.fx_lip_light,
        beauty_state_.fx_lip_band,
        landmarks, frame_size
    );
}

void EffectsManager::ApplyTeethWhitening(cv::Mat& frame_bgr, const FaceRegions& regions) {
    ApplyTeethWhitenBGR(frame_bgr, regions, beauty_state_.fx_teeth_strength, beauty_state_.fx_teeth_margin);
}

void EffectsManager::ApplyBeautyPreset(int preset_index) {
    ApplyPreset(preset_index, beauty_state_);
    std::cout << "✨ Applied beauty preset " << preset_index << std::endl;
}

void EffectsManager::GetCurrentBeautyState(BeautyState& state) const {
    state = beauty_state_;
}

void EffectsManager::SetBeautyState(const BeautyState& state) {
    beauty_state_ = state;
    
    // Update OpenCL state if needed
    if (state_.opencl_available && state_.opencl_enabled != state_.opencl_enabled) {
        cv::ocl::setUseOpenCL(state_.opencl_enabled);
    }
}

// Background settings
void EffectsManager::SetBackgroundMode(int mode) {
    beauty_state_.bg_mode = std::clamp(mode, 0, 3);
}

void EffectsManager::SetBlurStrength(int strength) {
    beauty_state_.blur_strength = std::max(1, strength);
    if ((beauty_state_.blur_strength % 2) == 0) beauty_state_.blur_strength++; // Ensure odd
}

void EffectsManager::SetFeatherAmount(float feather_px) {
    beauty_state_.feather_px = std::max(0.0f, feather_px);
}

void EffectsManager::SetBackgroundImage(const cv::Mat& image) {
    if (!image.empty()) {
        background_image_ = image.clone();
    }
}

void EffectsManager::SetBackgroundImageFromPath(const std::string& path) {
    LoadBackgroundImage(path);
}

void EffectsManager::SetSolidBackgroundColor(float r, float g, float b) {
    beauty_state_.solid_color[0] = std::clamp(r, 0.0f, 1.0f);
    beauty_state_.solid_color[1] = std::clamp(g, 0.0f, 1.0f);
    beauty_state_.solid_color[2] = std::clamp(b, 0.0f, 1.0f);
}

void EffectsManager::SetShowMask(bool enabled) {
    state_.show_mask = enabled;
}

// Skin smoothing controls
void EffectsManager::SetSkinSmoothingEnabled(bool enabled) {
    beauty_state_.fx_skin = enabled;
}

void EffectsManager::SetSkinSmoothingStrength(float strength) {
    beauty_state_.fx_skin_amount = std::clamp(strength, 0.0f, 1.0f);
}

void EffectsManager::SetSkinSmoothingAdvanced(bool advanced) {
    beauty_state_.fx_skin_adv = advanced;
}

void EffectsManager::SetSkinSmoothingAmount(float amount) {
    beauty_state_.fx_skin_amount = std::clamp(amount, 0.0f, 1.0f);
}

void EffectsManager::SetSkinSmoothingRadius(float radius_px) {
    beauty_state_.fx_skin_radius = std::max(1.0f, radius_px);
}

void EffectsManager::SetSkinTexturePreservation(float texture_thresh) {
    beauty_state_.fx_skin_tex = std::clamp(texture_thresh, 0.0f, 1.0f);
}

void EffectsManager::SetSkinEdgeFeather(float edge_feather_px) {
    beauty_state_.fx_skin_edge = std::max(0.0f, edge_feather_px);
}

// Wrinkle-aware controls
void EffectsManager::SetWrinkleAwareEnabled(bool enabled) {
    beauty_state_.fx_skin_wrinkle = enabled;
}

void EffectsManager::SetWrinkleGain(float gain) {
    beauty_state_.fx_skin_wrinkle_gain = std::max(0.0f, gain);
}

void EffectsManager::SetSmileBoost(float boost) {
    beauty_state_.fx_skin_smile_boost = std::clamp(boost, 0.0f, 2.0f);
}

void EffectsManager::SetSquintBoost(float boost) {
    beauty_state_.fx_skin_squint_boost = std::clamp(boost, 0.0f, 2.0f);
}

void EffectsManager::SetForeheadBoost(float boost) {
    beauty_state_.fx_skin_forehead_boost = std::clamp(boost, 0.0f, 2.0f);
}

void EffectsManager::SetSuppressLowerFace(bool enabled) {
    beauty_state_.fx_wrinkle_suppress_lower = enabled;
}

void EffectsManager::SetLowerFaceRatio(float ratio) {
    beauty_state_.fx_wrinkle_lower_ratio = std::clamp(ratio, 0.1f, 0.8f);
}

void EffectsManager::SetIgnoreGlasses(bool enabled) {
    beauty_state_.fx_wrinkle_ignore_glasses = enabled;
}

void EffectsManager::SetGlassesMargin(float margin_px) {
    beauty_state_.fx_wrinkle_glasses_margin = std::max(0.0f, margin_px);
}

void EffectsManager::SetWrinkleSensitivity(float keep_ratio) {
    beauty_state_.fx_wrinkle_keep_ratio = std::clamp(keep_ratio, 0.01f, 1.0f);
}

void EffectsManager::SetCustomWrinkleScales(bool enabled) {
    beauty_state_.fx_wrinkle_custom_scales = enabled;
}

void EffectsManager::SetWrinkleMinWidth(float min_px) {
    beauty_state_.fx_wrinkle_min_px = std::max(1.0f, min_px);
}

void EffectsManager::SetWrinkleMaxWidth(float max_px) {
    beauty_state_.fx_wrinkle_max_px = std::max(beauty_state_.fx_wrinkle_min_px, max_px);
}

void EffectsManager::SetWrinkleSkinGate(bool enabled) {
    beauty_state_.fx_wrinkle_use_skin_gate = enabled;
}

void EffectsManager::SetWrinkleMaskGain(float gain) {
    beauty_state_.fx_wrinkle_mask_gain = std::max(0.5f, gain);
}

void EffectsManager::SetWrinkleBaselineBoost(float boost) {
    beauty_state_.fx_wrinkle_baseline = std::clamp(boost, 0.0f, 1.0f);
}

void EffectsManager::SetWrinkleNegativeCap(float cap) {
    beauty_state_.fx_wrinkle_neg_cap = std::clamp(cap, 0.5f, 1.0f);
}

void EffectsManager::SetWrinklePreview(bool enabled) {
    beauty_state_.fx_wrinkle_preview = enabled;
}

// Advanced processing controls
void EffectsManager::SetProcessingScale(float scale) {
    beauty_state_.fx_adv_scale = std::clamp(scale, 0.5f, 1.0f);
}

void EffectsManager::SetDetailPreservation(float preserve) {
    beauty_state_.fx_adv_detail_preserve = std::clamp(preserve, 0.0f, 0.5f);
}

// Lip effects controls
void EffectsManager::SetLipstickEnabled(bool enabled) {
    beauty_state_.fx_lipstick = enabled;
}

void EffectsManager::SetLipAlpha(float alpha) {
    beauty_state_.fx_lip_alpha = std::clamp(alpha, 0.0f, 1.0f);
}

void EffectsManager::SetLipFeather(float feather_px) {
    beauty_state_.fx_lip_feather = std::max(0.0f, feather_px);
}

void EffectsManager::SetLipLightness(float lightness) {
    beauty_state_.fx_lip_light = std::clamp(lightness, -1.0f, 1.0f);
}

void EffectsManager::SetLipBandGrow(float band_px) {
    beauty_state_.fx_lip_band = std::max(0.0f, band_px);
}

void EffectsManager::SetLipColor(float r, float g, float b) {
    beauty_state_.fx_lip_color[0] = std::clamp(r, 0.0f, 1.0f);
    beauty_state_.fx_lip_color[1] = std::clamp(g, 0.0f, 1.0f);
    beauty_state_.fx_lip_color[2] = std::clamp(b, 0.0f, 1.0f);
}

// Teeth whitening controls
void EffectsManager::SetTeethWhiteningEnabled(bool enabled) {
    beauty_state_.fx_teeth = enabled;
}

void EffectsManager::SetTeethWhiteningStrength(float strength) {
    beauty_state_.fx_teeth_strength = std::clamp(strength, 0.0f, 1.0f);
}

void EffectsManager::SetTeethMargin(float margin_px) {
    beauty_state_.fx_teeth_margin = std::max(0.0f, margin_px);
}

// OpenCL acceleration
void EffectsManager::SetOpenCLEnabled(bool enabled) {
    if (state_.opencl_available) {
        state_.opencl_enabled = enabled;
        cv::ocl::setUseOpenCL(enabled);
        std::cout << "🚀 OpenCL " << (enabled ? "enabled" : "disabled") << std::endl;
    }
}

// Debug and visualization
void EffectsManager::SetShowLandmarks(bool enabled) {
    state_.show_landmarks = enabled;
}

cv::Mat EffectsManager::VisualizeMask(const cv::Mat& mask) {
    return VisualizeMaskRGB(mask);
}

// Performance monitoring
void EffectsManager::UpdatePerformanceStats() {
    // Already updated in ProcessFrame
}

double EffectsManager::GetAverageProcessingTime() const {
    if (perf_sum_frames_ == 0) return 0.0;
    return perf_sum_frame_ms_ / perf_sum_frames_;
}

void EffectsManager::ResetPerformanceStats() {
    perf_sum_frame_ms_ = 0.0;
    perf_sum_smooth_ms_ = 0.0;
    perf_sum_bg_ms_ = 0.0;
    perf_sum_frames_ = 0;
    last_perf_log_time_ = std::chrono::steady_clock::now();
}

// Background image management
bool EffectsManager::LoadBackgroundImage(const std::string& path) {
    cv::Mat img = cv::imread(path, cv::IMREAD_COLOR);
    if (!img.empty()) {
        background_image_ = img;
        std::cout << "🖼️  Loaded background image: " << path << " (" << img.cols << "x" << img.rows << ")" << std::endl;
        return true;
    }
    std::cerr << "❌ Failed to load background image: " << path << std::endl;
    return false;
}

void EffectsManager::ClearBackgroundImage() {
    background_image_.release();
    std::cout << "🗑️  Background image cleared" << std::endl;
}

void EffectsManager::Cleanup() {
    if (!state_.is_initialized) return;
    
    std::cout << "🧹 Cleaning up Effects Manager..." << std::endl;
    
    // Clear background image
    background_image_.release();
    
    // Reset state
    state_ = EffectsState{};
    beauty_state_ = BeautyState{};
    
    std::cout << "✅ Effects Manager cleanup completed" << std::endl;
}

// Private helper methods
FaceRegions EffectsManager::ExtractFaceRegionsFromLandmarks(const mediapipe::NormalizedLandmarkList& landmarks, 
                                                           const cv::Size& frame_size) {
    FaceRegions regions;
    ExtractFaceRegions(landmarks, frame_size, &regions, false, false, false);
    return regions;
}

cv::Mat EffectsManager::ResizeMaskIfNeeded(const cv::Mat& mask, const cv::Size& target_size) {
    if (mask.empty()) return mask;
    
    if (mask.cols != target_size.width || mask.rows != target_size.height) {
        cv::Mat resized;
        cv::resize(mask, resized, target_size, 0, 0, cv::INTER_LINEAR);
        return resized;
    }
    
    return mask;
}

cv::Scalar EffectsManager::ConvertRGBColorToBGR(float r, float g, float b) {
    return cv::Scalar(
        std::clamp(b * 255.0f, 0.0f, 255.0f),
        std::clamp(g * 255.0f, 0.0f, 255.0f),
        std::clamp(r * 255.0f, 0.0f, 255.0f)
    );
}

void EffectsManager::DrawLandmarks(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks) {
    int W = frame_bgr.cols;
    int H = frame_bgr.rows;
    const int n = landmarks.landmark_size();
    
    // Draw individual landmark points
    for (int i = 0; i < n; ++i) {
        const auto& p = landmarks.landmark(i);
        float nx = p.x();
        float ny = p.y();
        int x = std::max(0, std::min(W - 1, (int)std::round(nx * W)));
        int y = std::max(0, std::min(H - 1, (int)std::round(ny * H)));
        cv::circle(frame_bgr, cv::Point(x, y), 1, cv::Scalar(0, 255, 0), cv::FILLED, cv::LINE_AA);
    }
    
    // Draw connections for different face parts
    using Conn = mediapipe::tasks::vision::face_landmarker::FaceLandmarksConnections;
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksLips, cv::Scalar(0, 128, 255));
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksFaceOval, cv::Scalar(0, 200, 255));
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksLeftEye, cv::Scalar(255, 200, 80));
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksRightEye, cv::Scalar(255, 200, 80));
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksLeftEyeBrow, cv::Scalar(180, 180, 255));
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksRightEyeBrow, cv::Scalar(180, 180, 255));
}

template<size_t N>
void EffectsManager::DrawConnections(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks, 
                                    const std::array<std::array<int, 2>, N>& connections, const cv::Scalar& color) {
    int W = frame_bgr.cols;
    int H = frame_bgr.rows;
    const int n = landmarks.landmark_size();
    
    for (const auto& e : connections) {
        if (e[0] >= n || e[1] >= n) continue; // Safety check
        const auto& pa = landmarks.landmark(e[0]);
        const auto& pb = landmarks.landmark(e[1]);
        
        cv::Point pt_a((int)std::round(pa.x() * W), (int)std::round(pa.y() * H));
        cv::Point pt_b((int)std::round(pb.x() * W), (int)std::round(pb.y() * H));
        
        cv::line(frame_bgr, pt_a, pt_b, color, 1, cv::LINE_AA);
    }
}

void EffectsManager::LogPerformanceStats() {
    if (perf_sum_frames_ == 0) return;
    
    double avg_frame = perf_sum_frame_ms_ / perf_sum_frames_;
    double avg_smooth = perf_sum_smooth_ms_ / perf_sum_frames_;
    double avg_bg = perf_sum_bg_ms_ / perf_sum_frames_;
    
    std::cout << "📊 Effects Performance [" << perf_sum_frames_ << " frames]:" << std::endl;
    std::cout << "  Total: " << avg_frame << "ms" << std::endl;
    std::cout << "  Smoothing: " << avg_smooth << "ms" << std::endl; 
    std::cout << "  Background: " << avg_bg << "ms" << std::endl;
    if (state_.opencl_enabled) {
        std::cout << "  OpenCL: enabled" << std::endl;
    }
    
    // Reset for next interval
    ResetPerformanceStats();
}

bool EffectsManager::ShouldLogPerformance() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_perf_log_time_).count();
    return elapsed >= config_.performance_log_interval_ms;
}

void EffectsManager::ApplySkinSmoothingWithProcessingScale(cv::Mat& frame_bgr, const FaceRegions& regions, 
                                                          const mediapipe::NormalizedLandmarkList& landmarks) {
    cv::Rect roi = CalculateProcessingROI(regions, frame_bgr.size());
    if (roi.width < 8 || roi.height < 8) {
        ApplyFullResolutionSkinSmoothing(frame_bgr, regions, landmarks);
        return;
    }
    
    FaceRegions fr_small = TransformFaceRegionsToScaledROI(regions, roi);
    mediapipe::NormalizedLandmarkList lms_roi = TransformLandmarksToROI(landmarks, roi, frame_bgr.size());
    
    ProcessAndUpsampleROI(frame_bgr, roi, fr_small, lms_roi);
}

cv::Rect EffectsManager::CalculateProcessingROI(const FaceRegions& regions, const cv::Size& frame_size) {
    cv::Rect face_bb = cv::boundingRect(regions.face_oval);
    int pad = std::max(8, (int)std::round(beauty_state_.fx_skin_edge + beauty_state_.fx_skin_radius * 2.0f));
    cv::Rect roi(face_bb.x - pad, face_bb.y - pad, face_bb.width + 2*pad, face_bb.height + 2*pad);
    roi &= cv::Rect(0, 0, frame_size.width, frame_size.height);
    return roi;
}

void EffectsManager::ApplyFullResolutionSkinSmoothing(cv::Mat& frame_bgr, const FaceRegions& regions, 
                                                     const mediapipe::NormalizedLandmarkList& landmarks) {
    ApplySkinSmoothingAdvBGR(
        frame_bgr, regions,
        beauty_state_.fx_skin_amount, beauty_state_.fx_skin_radius, beauty_state_.fx_skin_tex, beauty_state_.fx_skin_edge,
        &landmarks, beauty_state_.fx_skin_smile_boost, beauty_state_.fx_skin_squint_boost, beauty_state_.fx_skin_forehead_boost,
        beauty_state_.fx_skin_wrinkle_gain, beauty_state_.fx_wrinkle_suppress_lower, beauty_state_.fx_wrinkle_lower_ratio,
        beauty_state_.fx_wrinkle_ignore_glasses, beauty_state_.fx_wrinkle_glasses_margin, beauty_state_.fx_wrinkle_keep_ratio,
        beauty_state_.fx_wrinkle_custom_scales ? beauty_state_.fx_wrinkle_min_px : -1.0f,
        beauty_state_.fx_wrinkle_custom_scales ? beauty_state_.fx_wrinkle_max_px : -1.0f,
        8.0f, beauty_state_.fx_wrinkle_preview, beauty_state_.fx_wrinkle_baseline,
        beauty_state_.fx_wrinkle_use_skin_gate, beauty_state_.fx_wrinkle_mask_gain, beauty_state_.fx_wrinkle_neg_cap
    );
}

FaceRegions EffectsManager::TransformFaceRegionsToScaledROI(const FaceRegions& regions, const cv::Rect& roi) {
    float sc = std::clamp(beauty_state_.fx_adv_scale, 0.5f, 1.0f);
    
    // Transform to ROI coordinates, then scale
    FaceRegions fr_roi = ShiftFaceRegionsToROI(regions, roi);
    FaceRegions fr_small = ScaleFaceRegions(fr_roi, sc);
    return fr_small;
}

FaceRegions EffectsManager::ShiftFaceRegionsToROI(const FaceRegions& regions, const cv::Rect& roi) {
    FaceRegions fr_roi;
    fr_roi.face_oval = ShiftPolygon(regions.face_oval, -roi.x, -roi.y);
    fr_roi.lips_outer = ShiftPolygon(regions.lips_outer, -roi.x, -roi.y);
    fr_roi.lips_inner = ShiftPolygon(regions.lips_inner, -roi.x, -roi.y);
    fr_roi.left_eye = ShiftPolygon(regions.left_eye, -roi.x, -roi.y);
    fr_roi.right_eye = ShiftPolygon(regions.right_eye, -roi.x, -roi.y);
    return fr_roi;
}

FaceRegions EffectsManager::ScaleFaceRegions(const FaceRegions& regions, float scale) {
    FaceRegions fr_scaled;
    fr_scaled.face_oval = ScalePolygon(regions.face_oval, scale);
    fr_scaled.lips_outer = ScalePolygon(regions.lips_outer, scale);
    fr_scaled.lips_inner = ScalePolygon(regions.lips_inner, scale);
    fr_scaled.left_eye = ScalePolygon(regions.left_eye, scale);
    fr_scaled.right_eye = ScalePolygon(regions.right_eye, scale);
    return fr_scaled;
}

std::vector<cv::Point> EffectsManager::ShiftPolygon(const std::vector<cv::Point>& poly, int dx, int dy) {
    std::vector<cv::Point> out;
    out.reserve(poly.size());
    for (auto p : poly) {
        out.emplace_back(p.x + dx, p.y + dy);
    }
    return out;
}

std::vector<cv::Point> EffectsManager::ScalePolygon(const std::vector<cv::Point>& poly, float scale) {
    std::vector<cv::Point> out;
    out.reserve(poly.size());
    for (auto p : poly) {
        out.emplace_back((int)std::round(p.x * scale), (int)std::round(p.y * scale));
    }
    return out;
}

mediapipe::NormalizedLandmarkList EffectsManager::TransformLandmarksToROI(const mediapipe::NormalizedLandmarkList& landmarks, 
                                                                         const cv::Rect& roi, const cv::Size& frame_size) {
    mediapipe::NormalizedLandmarkList lms_roi = landmarks;
    for (int i = 0; i < lms_roi.landmark_size(); ++i) {
        auto* p = lms_roi.mutable_landmark(i);
        float px = p->x() * frame_size.width;
        float py = p->y() * frame_size.height;
        float xr = (px - roi.x) / (float)roi.width;
        float yr = (py - roi.y) / (float)roi.height;
        p->set_x(xr);
        p->set_y(yr);
    }
    return lms_roi;
}

void EffectsManager::ProcessAndUpsampleROI(cv::Mat& frame_bgr, const cv::Rect& roi, const FaceRegions& fr_small, 
                                          const mediapipe::NormalizedLandmarkList& lms_roi) {
    cv::Mat roi_bgr = frame_bgr(roi);
    cv::Mat small = DownscaleROI(roi_bgr);
    
    ApplySkinSmoothingToScaledImage(small, fr_small, lms_roi);
    
    cv::Mat up = UpsampleProcessedImage(small, roi.size());
    ApplyDetailPreservationIfNeeded(up, roi_bgr, fr_small);
    
    up.copyTo(roi_bgr);
}

cv::Mat EffectsManager::DownscaleROI(const cv::Mat& roi_bgr) {
    float sc = std::clamp(beauty_state_.fx_adv_scale, 0.5f, 1.0f);
    cv::Size target_size(std::max(1, (int)std::round(roi_bgr.cols * sc)), 
                        std::max(1, (int)std::round(roi_bgr.rows * sc)));
    cv::Mat small;
    cv::resize(roi_bgr, small, target_size, 0, 0, cv::INTER_AREA);
    return small;
}

void EffectsManager::ApplySkinSmoothingToScaledImage(cv::Mat& small, const FaceRegions& fr_small, 
                                                    const mediapipe::NormalizedLandmarkList& lms_roi) {
    float sc = std::clamp(beauty_state_.fx_adv_scale, 0.5f, 1.0f);
    ApplySkinSmoothingAdvBGR(
        small, fr_small,
        beauty_state_.fx_skin_amount,
        beauty_state_.fx_skin_radius * sc,
        beauty_state_.fx_skin_tex,
        beauty_state_.fx_skin_edge * sc,
        &lms_roi,
        beauty_state_.fx_skin_smile_boost,
        beauty_state_.fx_skin_squint_boost,
        beauty_state_.fx_skin_forehead_boost,
        beauty_state_.fx_skin_wrinkle_gain,
        beauty_state_.fx_wrinkle_suppress_lower,
        beauty_state_.fx_wrinkle_lower_ratio,
        beauty_state_.fx_wrinkle_ignore_glasses,
        beauty_state_.fx_wrinkle_glasses_margin * sc,
        beauty_state_.fx_wrinkle_keep_ratio,
        beauty_state_.fx_wrinkle_custom_scales ? beauty_state_.fx_wrinkle_min_px * sc : -1.0f,
        beauty_state_.fx_wrinkle_custom_scales ? beauty_state_.fx_wrinkle_max_px * sc : -1.0f,
        8.0f * sc,
        beauty_state_.fx_wrinkle_preview,
        beauty_state_.fx_wrinkle_baseline,
        beauty_state_.fx_wrinkle_use_skin_gate,
        beauty_state_.fx_wrinkle_mask_gain,
        beauty_state_.fx_wrinkle_neg_cap
    );
}

cv::Mat EffectsManager::UpsampleProcessedImage(const cv::Mat& small, const cv::Size& target_size) {
    cv::Mat up;
    cv::resize(small, up, target_size, 0, 0, cv::INTER_LANCZOS4);
    return up;
}

void EffectsManager::ApplyDetailPreservationIfNeeded(cv::Mat& up, const cv::Mat& roi_bgr, const FaceRegions& fr_roi) {
    float dp = std::clamp(beauty_state_.fx_adv_detail_preserve, 0.0f, 0.5f);
    if (dp > 1e-3f) {
        ApplyDetailPreservation(up, roi_bgr, fr_roi, dp);
    }
}

void EffectsManager::ApplyDetailPreservation(cv::Mat& up, const cv::Mat& roi_bgr, const FaceRegions& fr_roi, float dp) {
    cv::Mat mask_roi_u8 = CreateFaceMask(fr_roi, up.size());
    
    int fk = std::max(3, (int)std::round(beauty_state_.fx_skin_edge) | 1);
    cv::GaussianBlur(mask_roi_u8, mask_roi_u8, cv::Size(fk, fk), 0);
    cv::Mat mask_f;
    mask_roi_u8.convertTo(mask_f, CV_32F, 1.0/255.0);
    
    // Unsharp masking: add fraction of high-frequency detail from original ROI
    cv::Mat base;
    cv::GaussianBlur(roi_bgr, base, cv::Size(0,0), 0.8);
    cv::Mat roi32, base32;
    roi_bgr.convertTo(roi32, CV_32F, 1.0/255.0);
    base.convertTo(base32, CV_32F, 1.0/255.0);
    cv::Mat hi = roi32 - base32; // High frequency component
    
    std::vector<cv::Mat> uch(3), hi_ch(3);
    cv::split(up, uch);
    cv::split(hi, hi_ch);
    
    for (int i = 0; i < 3; ++i) {
        cv::Mat u32;
        uch[i].convertTo(u32, CV_32F, 1.0/255.0);
        cv::Mat out32 = u32 + hi_ch[i].mul(mask_f * dp);
        out32 = cv::min(cv::max(out32, 0.0f), 1.0f);
        out32.convertTo(uch[i], CV_8U, 255.0);
    }
    cv::merge(uch, up);
}

cv::Mat EffectsManager::CreateFaceMask(const FaceRegions& fr_roi, const cv::Size& size) {
    cv::Mat mask(size, CV_8U, cv::Scalar(0));
    if (!fr_roi.face_oval.empty()) cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{fr_roi.face_oval}, cv::Scalar(255));
    if (!fr_roi.lips_outer.empty()) cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{fr_roi.lips_outer}, cv::Scalar(0));
    if (!fr_roi.left_eye.empty()) cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{fr_roi.left_eye}, cv::Scalar(0));
    if (!fr_roi.right_eye.empty()) cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{fr_roi.right_eye}, cv::Scalar(0));
    return mask;
}

// Auto processing scale methods
void EffectsManager::SetAutoProcessingScaleEnabled(bool enabled) {
    if (auto_scale_enabled_ == enabled) {
        return; // No change, don't reset timers
    }
    
    auto_scale_enabled_ = enabled;
    if (enabled) {
        fps_history_.clear();
        last_fps_update_ = std::chrono::steady_clock::now();
        last_scale_adjustment_ = std::chrono::steady_clock::now();
        std::cout << "[AutoScale] Enabled with target FPS: " << target_fps_ << std::endl;
    }
}

void EffectsManager::SetTargetFPS(float target_fps) {
    float new_target = std::clamp(target_fps, 5.0f, 60.0f);
    if (std::abs(target_fps_ - new_target) < 0.1f) {
        return; // No significant change
    }
    target_fps_ = new_target;
}

void EffectsManager::UpdateTargetFPSFromCamera(float camera_fps) {
    float target_fps;
    if (camera_fps > 15.0f) {
        target_fps = 15.0f - 1.0f; // Target 14 FPS for high frame rate cameras
    } else {
        target_fps = camera_fps - 1.0f; // Target camera_fps - 1 for lower frame rates
    }
    
    // Ensure minimum target of 5 FPS
    target_fps = std::max(target_fps, 5.0f);
    
    SetTargetFPS(target_fps);
}

void EffectsManager::UpdateAutoProcessingScale(float current_fps) {
    if (!auto_scale_enabled_) {
        return;
    }
    
    current_fps_ = current_fps;
    auto now = std::chrono::steady_clock::now();
    
    UpdateFPSHistory(current_fps);
    
    if (!HasEnoughFPSSamples() || !ShouldAdjustScale(now)) {
        return;
    }
    
    float avg_fps = CalculateAverageFPS();
    float scale_adjustment = CalculateScaleAdjustment(avg_fps);
    
    if (scale_adjustment != 0.0f) {
        ApplyScaleAdjustment(scale_adjustment, now);
    }
}

void EffectsManager::UpdateFPSHistory(float current_fps) {
    fps_history_.push_back(current_fps);
    if (fps_history_.size() > FPS_HISTORY_SIZE) {
        fps_history_.erase(fps_history_.begin());
    }
}

bool EffectsManager::HasEnoughFPSSamples() const {
    return fps_history_.size() >= 10;
}

bool EffectsManager::ShouldAdjustScale(const std::chrono::steady_clock::time_point& now) const {
    auto time_since_adjustment = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_scale_adjustment_).count();
    return time_since_adjustment >= 5000;
}

float EffectsManager::CalculateAverageFPS() const {
    float avg_fps = 0.0f;
    for (float fps : fps_history_) {
        avg_fps += fps;
    }
    return avg_fps / fps_history_.size();
}

float EffectsManager::CalculateScaleAdjustment(float avg_fps) const {
    float fps_diff = target_fps_ - avg_fps;
    
    if (std::abs(fps_diff) <= 2.0f) {
        return 0.0f;
    }
    
    if (fps_diff > 3.0f) {
        return fps_diff > 6.0f ? -0.002f : -0.001f;
    } else if (fps_diff < -3.0f) {
        return fps_diff < -6.0f ? 0.002f : 0.001f;
    }
    
    return 0.0f;
}

void EffectsManager::ApplyScaleAdjustment(float scale_adjustment, const std::chrono::steady_clock::time_point& now) {
    float current_scale = beauty_state_.fx_adv_scale;
    float new_scale = std::clamp(current_scale + scale_adjustment, 0.4f, 1.0f);
    
    if (std::abs(new_scale - current_scale) > 0.0005f) {
        beauty_state_.fx_adv_scale = new_scale;
        last_scale_adjustment_ = now;
        
        TrimFPSHistoryForStability();
    }
}

void EffectsManager::TrimFPSHistoryForStability() {
    if (fps_history_.size() > 10) {
        fps_history_.erase(fps_history_.begin(), fps_history_.begin() + fps_history_.size()/2);
    }
}

bool EffectsManager::IsAutoProcessingScaleEnabled() const {
    return auto_scale_enabled_;
}

float EffectsManager::GetCurrentFPS() const {
    return current_fps_;
}

float EffectsManager::GetTargetFPS() const {
    return target_fps_;
}

float EffectsManager::GetProcessingScale() const {
    return beauty_state_.fx_adv_scale;
}

} // namespace segmecam