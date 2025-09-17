#include "include/effects/effects_manager.h"
#include "segmecam_composite.h"
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

cv::Mat EffectsManager::ProcessFrame(const cv::Mat& frame_bgr,
                                    const cv::Mat& segmentation_mask,
                                    const mediapipe::NormalizedLandmarkList* face_landmarks) {
    if (!state_.is_initialized) {
        // Return BGR frame as-is when not initialized (ApplicationRun will convert to RGB for display)
        return frame_bgr.clone();
    }
    
    static int debug_frame_count = 0;
    debug_frame_count++;
    
    // Debug input frame on first few frames
    if (debug_frame_count <= 3 && !frame_bgr.empty()) {
        cv::Vec3b input_pixel = frame_bgr.at<cv::Vec3b>(frame_bgr.rows/2, frame_bgr.cols/2);
        std::cout << "🔍 EFFECTS INPUT Frame " << debug_frame_count << " - BGR format: " << frame_bgr.type() 
                  << " center pixel: [" << (int)input_pixel[0] << "," << (int)input_pixel[1] << "," << (int)input_pixel[2] << "]" << std::endl;
    }
    
    auto start_time = std::chrono::steady_clock::now();    // Track frame info
    state_.last_frame_width = frame_bgr.cols;
    state_.last_frame_height = frame_bgr.rows;
    
    // Work on a copy to avoid modifying the original
    cv::Mat processed_frame = frame_bgr.clone();
    
    // Apply face effects if landmarks are available
    if (config_.enable_face_effects && face_landmarks && face_landmarks->landmark_size() > 0) {
        auto smooth_start = std::chrono::steady_clock::now();
        ApplyFaceEffects(processed_frame, *face_landmarks);
        auto smooth_end = std::chrono::steady_clock::now();
        state_.last_smoothing_time_ms = std::chrono::duration<double, std::milli>(smooth_end - smooth_start).count();
        perf_sum_smooth_ms_ += state_.last_smoothing_time_ms;
    } else {
        state_.last_smoothing_time_ms = 0.0;
    }
    
    // Apply background effects
    cv::Mat result;
    if (config_.enable_background_effects && !segmentation_mask.empty()) {
        auto bg_start = std::chrono::steady_clock::now();
        result = ApplyBackgroundEffect(processed_frame, segmentation_mask);
        auto bg_end = std::chrono::steady_clock::now();
        state_.last_background_time_ms = std::chrono::duration<double, std::milli>(bg_end - bg_start).count();
        perf_sum_bg_ms_ += state_.last_background_time_ms;
    } else {
        // No background effects, return BGR frame as-is (ApplicationRun will convert to RGB for display)
        result = processed_frame.clone();
        state_.last_background_time_ms = 0.0;
    }
    
    // Update performance tracking
    auto end_time = std::chrono::steady_clock::now();
    state_.total_processing_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    perf_sum_frame_ms_ += state_.total_processing_time_ms;
    state_.frames_processed++;
    perf_sum_frames_++;
    
    // Log performance if needed
    if (config_.enable_performance_logging && ShouldLogPerformance()) {
        LogPerformanceStats();
    }
    
    // Debug output frame on first few frames
    if (debug_frame_count <= 3 && !result.empty()) {
        cv::Vec3b output_pixel = result.at<cv::Vec3b>(result.rows/2, result.cols/2);
        std::cout << "🔍 EFFECTS OUTPUT Frame " << debug_frame_count << " - format: " << result.type() 
                  << " center pixel: [" << (int)output_pixel[0] << "," << (int)output_pixel[1] << "," << (int)output_pixel[2] << "]" 
                  << " (should be RGB)" << std::endl;
    }
    
    return result;
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
    
    // Helper function to draw connections
    auto draw_conn = [&](int a, int b, const cv::Scalar& col) {
        if (a >= n || b >= n) return; // Safety check
        const auto& pa = landmarks.landmark(a);
        const auto& pb = landmarks.landmark(b);
        
        cv::Point pt_a((int)std::round(pa.x() * W), (int)std::round(pa.y() * H));
        cv::Point pt_b((int)std::round(pb.x() * W), (int)std::round(pb.y() * H));
        
        cv::line(frame_bgr, pt_a, pt_b, col, 1, cv::LINE_AA);
    };
    
    // Draw lip connections (matches original implementation)
    using Conn = mediapipe::tasks::vision::face_landmarker::FaceLandmarksConnections;
    for (const auto& e : Conn::kFaceLandmarksLips) {
        draw_conn(e[0], e[1], cv::Scalar(0, 128, 255)); // Orange color for lips
    }
    
    // Draw face oval connections 
    for (const auto& e : Conn::kFaceLandmarksFaceOval) {
        draw_conn(e[0], e[1], cv::Scalar(0, 200, 255)); // Yellow-orange for face outline
    }
    
    // Draw left eye connections
    for (const auto& e : Conn::kFaceLandmarksLeftEye) {
        draw_conn(e[0], e[1], cv::Scalar(255, 200, 80)); // Light blue for eyes
    }
    
    // Draw right eye connections  
    for (const auto& e : Conn::kFaceLandmarksRightEye) {
        draw_conn(e[0], e[1], cv::Scalar(255, 200, 80)); // Light blue for eyes
    }
    
    // Draw left eyebrow connections
    for (const auto& e : Conn::kFaceLandmarksLeftEyeBrow) {
        draw_conn(e[0], e[1], cv::Scalar(180, 180, 255)); // Light purple for eyebrows
    }
    
    // Draw right eyebrow connections
    for (const auto& e : Conn::kFaceLandmarksRightEyeBrow) {
        draw_conn(e[0], e[1], cv::Scalar(180, 180, 255)); // Light purple for eyebrows
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
    // Process a padded face ROI at reduced scale, then upsample and paste back
    cv::Rect face_bb = cv::boundingRect(regions.face_oval);
    int pad = std::max(8, (int)std::round(beauty_state_.fx_skin_edge + beauty_state_.fx_skin_radius * 2.0f));
    cv::Rect roi(face_bb.x - pad, face_bb.y - pad, face_bb.width + 2*pad, face_bb.height + 2*pad);
    roi &= cv::Rect(0, 0, frame_bgr.cols, frame_bgr.rows);
    
    if (roi.width < 8 || roi.height < 8) {
        // Fallback to full-res processing if ROI too small
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
        return;
    }
    
    // Helper lambdas for coordinate transformations
    auto shift_poly = [&](const std::vector<cv::Point>& poly) {
        std::vector<cv::Point> out; 
        out.reserve(poly.size()); 
        for (auto p: poly) { 
            out.emplace_back(p.x - roi.x, p.y - roi.y);
        } 
        return out; 
    };
    
    auto scale_poly = [&](const std::vector<cv::Point>& poly, float sc) {
        std::vector<cv::Point> out; 
        out.reserve(poly.size()); 
        for (auto p: poly) { 
            out.emplace_back((int)std::round(p.x*sc), (int)std::round(p.y*sc)); 
        } 
        return out; 
    };
    
    // Build FaceRegions in ROI coordinates, then scale
    FaceRegions fr_roi;
    fr_roi.face_oval = shift_poly(regions.face_oval);
    fr_roi.lips_outer = shift_poly(regions.lips_outer);
    fr_roi.lips_inner = shift_poly(regions.lips_inner);
    fr_roi.left_eye = shift_poly(regions.left_eye);
    fr_roi.right_eye = shift_poly(regions.right_eye);
    
    float sc = std::clamp(beauty_state_.fx_adv_scale, 0.5f, 1.0f);
    FaceRegions fr_small;
    fr_small.face_oval = scale_poly(fr_roi.face_oval, sc);
    fr_small.lips_outer = scale_poly(fr_roi.lips_outer, sc);
    fr_small.lips_inner = scale_poly(fr_roi.lips_inner, sc);
    fr_small.left_eye = scale_poly(fr_roi.left_eye, sc);
    fr_small.right_eye = scale_poly(fr_roi.right_eye, sc);
    
    // Transform landmarks to ROI coordinates
    mediapipe::NormalizedLandmarkList lms_roi = landmarks;
    for (int i = 0; i < lms_roi.landmark_size(); ++i) {
        auto* p = lms_roi.mutable_landmark(i);
        float px = p->x() * frame_bgr.cols;
        float py = p->y() * frame_bgr.rows;
        float xr = (px - roi.x) / (float)roi.width;
        float yr = (py - roi.y) / (float)roi.height;
        p->set_x(xr); 
        p->set_y(yr);
    }
    
    // Extract ROI, resize to small scale, process, then upsample back
    cv::Mat roi_bgr = frame_bgr(roi);
    cv::Mat small; 
    
    // Calculate exact target size to avoid rounding errors
    cv::Size target_size(std::max(1, (int)std::round(roi.width * sc)), 
                        std::max(1, (int)std::round(roi.height * sc)));
    cv::resize(roi_bgr, small, target_size, 0, 0, cv::INTER_AREA); // Always use INTER_AREA for stable downsampling
    
    // Apply skin smoothing on the downscaled image with scaled parameters
    ApplySkinSmoothingAdvBGR(
        small, fr_small,
        beauty_state_.fx_skin_amount,
        beauty_state_.fx_skin_radius * sc,  // Scale radius
        beauty_state_.fx_skin_tex,
        beauty_state_.fx_skin_edge * sc,    // Scale edge feather
        &lms_roi,
        beauty_state_.fx_skin_smile_boost,
        beauty_state_.fx_skin_squint_boost,
        beauty_state_.fx_skin_forehead_boost,
        beauty_state_.fx_skin_wrinkle_gain,
        beauty_state_.fx_wrinkle_suppress_lower,
        beauty_state_.fx_wrinkle_lower_ratio,
        beauty_state_.fx_wrinkle_ignore_glasses,
        beauty_state_.fx_wrinkle_glasses_margin * sc,  // Scale glasses margin
        beauty_state_.fx_wrinkle_keep_ratio,
        beauty_state_.fx_wrinkle_custom_scales ? beauty_state_.fx_wrinkle_min_px * sc : -1.0f,  // Scale min width
        beauty_state_.fx_wrinkle_custom_scales ? beauty_state_.fx_wrinkle_max_px * sc : -1.0f,  // Scale max width
        8.0f * sc,  // Scale forehead margin
        beauty_state_.fx_wrinkle_preview,
        beauty_state_.fx_wrinkle_baseline,
        beauty_state_.fx_wrinkle_use_skin_gate,
        beauty_state_.fx_wrinkle_mask_gain,
        beauty_state_.fx_wrinkle_neg_cap
    );
    
    // Upsample back to original ROI size using LANCZOS4 for better texture preservation
    cv::Mat up; 
    cv::resize(small, up, roi.size(), 0, 0, cv::INTER_LANCZOS4);
    
    // Optional detail re-injection to counter down/up sampling softness
    float dp = std::clamp(beauty_state_.fx_adv_detail_preserve, 0.0f, 0.5f);
    if (dp > 1e-3f) {
        // Build feathered face mask in ROI coordinates
        cv::Mat mask_roi_u8(roi.size(), CV_8U, cv::Scalar(0));
        if (!fr_roi.face_oval.empty()) cv::fillPoly(mask_roi_u8, std::vector<std::vector<cv::Point>>{fr_roi.face_oval}, cv::Scalar(255));
        if (!fr_roi.lips_outer.empty()) cv::fillPoly(mask_roi_u8, std::vector<std::vector<cv::Point>>{fr_roi.lips_outer}, cv::Scalar(0));
        if (!fr_roi.left_eye.empty()) cv::fillPoly(mask_roi_u8, std::vector<std::vector<cv::Point>>{fr_roi.left_eye}, cv::Scalar(0));
        if (!fr_roi.right_eye.empty()) cv::fillPoly(mask_roi_u8, std::vector<std::vector<cv::Point>>{fr_roi.right_eye}, cv::Scalar(0));
        
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
    
    // Copy processed ROI back to original frame
    up.copyTo(roi_bgr);
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
    
    // Add to FPS history
    fps_history_.push_back(current_fps);
    if (fps_history_.size() > FPS_HISTORY_SIZE) {
        fps_history_.erase(fps_history_.begin());
    }
    
    // Need at least 10 samples before adjusting (more stable baseline)
    if (fps_history_.size() < 10) {
        return;
    }
    
    // Only adjust every 5 seconds to avoid oscillation (increased from 2s)
    auto time_since_adjustment = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_scale_adjustment_).count();
    
    if (time_since_adjustment < 5000) {
        return;
    }
    
    // Calculate average FPS over history
    float avg_fps = 0.0f;
    for (float fps : fps_history_) {
        avg_fps += fps;
    }
    avg_fps /= fps_history_.size();
    
    // Calculate adjustment needed
    float fps_diff = target_fps_ - avg_fps;
    float current_scale = beauty_state_.fx_adv_scale;
    
    // Adjustment logic with ultra-conservative thresholds
    if (std::abs(fps_diff) > 2.0f) { // Require larger difference (was 0.5f, now 2.0f)
        float scale_adjustment = 0.0f;
        
        if (fps_diff > 3.0f) { // Too slow (actual FPS < target), reduce scale for performance
            scale_adjustment = -0.001f; // Reduce by 0.1% (ultra-fine adjustment)
            if (fps_diff > 6.0f) scale_adjustment = -0.002f; // Larger reduction for bigger difference (0.2%)
        } else if (fps_diff < -3.0f) { // Too fast (actual FPS > target), can increase quality
            scale_adjustment = 0.001f; // Increase by 0.1% (ultra-fine adjustment)
            if (fps_diff < -6.0f) scale_adjustment = 0.002f; // Larger increase (0.2%)
        }
        
        if (scale_adjustment != 0.0f) {
            float new_scale = std::clamp(current_scale + scale_adjustment, 0.4f, 1.0f);
            
            // Only apply if change is meaningful (ultra-fine threshold)
            if (std::abs(new_scale - current_scale) > 0.0005f) { // Even smaller threshold for ultra-fine adjustments
                beauty_state_.fx_adv_scale = new_scale;
                last_scale_adjustment_ = now;
                
                // Keep half the history for stability (don't clear completely)
                if (fps_history_.size() > 10) {
                    fps_history_.erase(fps_history_.begin(), fps_history_.begin() + fps_history_.size()/2);
                }
                
                // Scale change applied silently for cleaner output
            }
        }
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