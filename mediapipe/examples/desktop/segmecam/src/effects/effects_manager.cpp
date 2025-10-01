#include "effects/effects_manager.h"
#include "effects/background/background_effects.h"
#include "effects/performance/performance_monitor.h"
#include "effects/config/effects_config.h"
#include "effects/face_processor.h"
#include "effects/advanced_skin_effects.h"
#include "effects/wrinkle_segmenter.h"
#include "render/segmecam_composite.h"
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
    
    // Initialize modular components
    background_effects_ = std::make_unique<BackgroundEffects>();
    performance_monitor_ = std::make_unique<PerformanceMonitor>();
    effects_config_ = std::make_unique<EffectsConfiguration>(beauty_state_);
    face_processor_ = std::make_unique<FaceProcessor>();
    wrinkle_segmenter_ = std::make_unique<WrinkleSegmenter>();
    // Wire up auto-scaling callback
    WireAutoScaleCallback();
}

EffectsManager::~EffectsManager() {
    Cleanup();
}

// Helper method to create SkinSmoothingConfig from current beauty state
SkinSmoothingConfig EffectsManager::CreateSkinSmoothingConfig(float scale) const {
    SkinSmoothingConfig config;
    config.amount = NormalizeAdvancedAmount(beauty_state_.fx_skin_amount);
    config.radius_px = beauty_state_.fx_skin_radius * scale;
    config.texture_thresh = beauty_state_.fx_skin_tex;
    config.edge_feather_px = beauty_state_.fx_skin_edge * scale;
    config.expression.smile_boost = beauty_state_.fx_skin_smile_boost;
    config.expression.squint_boost = beauty_state_.fx_skin_squint_boost;
    config.expression.forehead_boost = beauty_state_.fx_skin_forehead_boost;
    config.expression.forehead_margin_px = 10.0f * scale;
    // Use user-configured wrinkle gain to drive frequency-separation boosting
    config.boost_gain = beauty_state_.fx_skin_wrinkle_gain;
    config.smile_wrinkle_gain = beauty_state_.fx_skin_smile_wrinkle_gain;
    config.wrinkle_enabled = beauty_state_.fx_skin_wrinkle;
    config.wrinkle.region_gates.suppress_lower_face = beauty_state_.fx_wrinkle_suppress_lower;
    config.wrinkle.region_gates.lower_face_ratio = beauty_state_.fx_wrinkle_lower_ratio;
    config.wrinkle.region_gates.ignore_glasses = beauty_state_.fx_wrinkle_ignore_glasses;
    config.wrinkle.region_gates.glasses_margin_px = beauty_state_.fx_wrinkle_glasses_margin * scale;
    config.wrinkle.keep_ratio = beauty_state_.fx_wrinkle_keep_ratio;
    config.wrinkle.line_min_px = beauty_state_.fx_wrinkle_custom_scales ? beauty_state_.fx_wrinkle_min_px * scale : 1.5f;
    config.wrinkle.line_max_px = beauty_state_.fx_wrinkle_custom_scales ? beauty_state_.fx_wrinkle_max_px * scale : 3.0f;
    config.wrinkle.smile_wrinkle_gain = beauty_state_.fx_skin_smile_wrinkle_gain;
    config.wrinkle_preview = beauty_state_.fx_wrinkle_preview;
    config.baseline_boost = beauty_state_.fx_wrinkle_baseline;
    config.wrinkle.use_skin_gate = beauty_state_.fx_wrinkle_use_skin_gate;
    config.wrinkle.mask_gain = beauty_state_.fx_wrinkle_mask_gain;
    config.neg_atten_cap = beauty_state_.fx_wrinkle_neg_cap;
    // boost_gain is now properly set above from fx_skin_wrinkle_gain
    return config;
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

    if (wrinkle_segmenter_ && !wrinkle_segmenter_->IsLoaded()) {
        if (!wrinkle_segmenter_->Initialize()) {
            std::cout << "⚠️  Wrinkle segmentation model failed to load. Falling back to analytical wrinkle mask." << std::endl;
        } else {
            std::cout << "✅ Wrinkle segmentation model loaded successfully." << std::endl;
        }
    }

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
        cv::Mat result_rgb;
        cv::cvtColor(frame_bgr, result_rgb, cv::COLOR_BGR2RGB);
        return result_rgb;
    }
    
    static int debug_frame_count = 0;
    debug_frame_count++;
    
    LogDebugInputFrame(debug_frame_count, frame_bgr);
    
    auto start_time = std::chrono::steady_clock::now();
    state_.last_frame_width = frame_bgr.cols;
    state_.last_frame_height = frame_bgr.rows;
    
    cv::Mat processed_frame = frame_bgr.clone();
    
    try {
        ProcessFaceEffects(processed_frame, face_landmarks);
        cv::Mat result = ProcessBackgroundEffects(processed_frame, segmentation_mask);
        
        performance_monitor_->UpdatePerformanceTracking(start_time, state_.last_smoothing_time_ms,
                                                       state_.last_background_time_ms,
                                                       state_.total_processing_time_ms,
                                                       state_.frames_processed);
        
        if (config_.enable_performance_logging && performance_monitor_->ShouldLogPerformance()) {
            performance_monitor_->LogPerformanceStats(
                performance_monitor_->GetAverageProcessingTime(),
                state_.last_smoothing_time_ms,
                state_.last_background_time_ms,
                state_.frames_processed,
                state_.opencl_enabled
            );
        }
        LogDebugOutputFrame(debug_frame_count, result);
        
        // Convert BGR result to RGB for display
        cv::Mat result_rgb;
        cv::cvtColor(result, result_rgb, cv::COLOR_BGR2RGB);
        return result_rgb;
    } catch (const cv::Exception& e) {
        std::cerr << "❌ OpenCV Exception in ProcessFrame: " << e.what() << std::endl;
        std::cerr << "   Error code: " << e.code << ", Function: " << e.func << std::endl;
        // Return original frame converted to RGB as fallback
        cv::Mat result_rgb;
        cv::cvtColor(frame_bgr, result_rgb, cv::COLOR_BGR2RGB);
        return result_rgb;
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception in ProcessFrame: " << e.what() << std::endl;
        // Return original frame converted to RGB as fallback
        cv::Mat result_rgb;
        cv::cvtColor(frame_bgr, result_rgb, cv::COLOR_BGR2RGB);
        return result_rgb;
    }
}

void EffectsManager::ProcessFaceEffects(cv::Mat& processed_frame, const mediapipe::NormalizedLandmarkList* face_landmarks) {
    if (config_.enable_face_effects && face_landmarks && face_landmarks->landmark_size() > 0) {
        try {
            auto smooth_start = std::chrono::steady_clock::now();
            ApplyFaceEffects(processed_frame, *face_landmarks);
            auto smooth_end = std::chrono::steady_clock::now();
            state_.last_smoothing_time_ms = std::chrono::duration<double, std::milli>(smooth_end - smooth_start).count();
        } catch (const cv::Exception& e) {
            std::cerr << "❌ OpenCV Exception in ProcessFaceEffects: " << e.what() << std::endl;
            std::cerr << "   Error code: " << e.code << ", Function: " << e.func << std::endl;
            state_.last_smoothing_time_ms = 0.0;
        } catch (const std::exception& e) {
            std::cerr << "❌ Exception in ProcessFaceEffects: " << e.what() << std::endl;
            state_.last_smoothing_time_ms = 0.0;
        }
    } else {
        state_.last_smoothing_time_ms = 0.0;
    }
}

cv::Mat EffectsManager::ProcessBackgroundEffects(const cv::Mat& processed_frame, const cv::Mat& segmentation_mask) {
    if (config_.enable_background_effects && !segmentation_mask.empty()) {
        try {
            auto bg_start = std::chrono::steady_clock::now();
            cv::Mat result = background_effects_->ApplyBackgroundEffect(processed_frame, segmentation_mask, beauty_state_, state_.opencl_enabled);
            auto bg_end = std::chrono::steady_clock::now();
            state_.last_background_time_ms = std::chrono::duration<double, std::milli>(bg_end - bg_start).count();
            return result;
        } catch (const cv::Exception& e) {
            std::cerr << "❌ OpenCV Exception in ProcessBackgroundEffects: " << e.what() << std::endl;
            std::cerr << "   Error code: " << e.code << ", Function: " << e.func << std::endl;
            // Return original frame as fallback
            return processed_frame.clone();
        } catch (const std::exception& e) {
            std::cerr << "❌ Exception in ProcessBackgroundEffects: " << e.what() << std::endl;
            // Return original frame as fallback
            return processed_frame.clone();
        }
    } else {
        // No background effects, return BGR frame as-is (ApplicationRun will convert to RGB for display)
        state_.last_background_time_ms = 0.0;
        return processed_frame.clone();
    }
}


void EffectsManager::LogDebugOutputFrame(int frame_count, const cv::Mat& result) {
    if (frame_count <= 3 && !result.empty()) {
        cv::Vec3b output_pixel = result.at<cv::Vec3b>(result.rows/2, result.cols/2);
        std::cout << "🔍 EFFECTS OUTPUT Frame " << frame_count << " - format: " << result.type() 
                  << " center pixel: [" << (int)output_pixel[0] << "," << (int)output_pixel[1] << "," << (int)output_pixel[2] << "]" 
                  << " (RGB for display)" << std::endl;
    }
}







void EffectsManager::ApplyFaceEffects(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks) {
    // Extract face regions from landmarks
    FaceRegions regions = face_processor_->ExtractFaceRegionsFromLandmarks(landmarks, frame_bgr.size());
    
    // Draw landmarks overlay if enabled
    if (state_.show_landmarks) {
        face_processor_->DrawLandmarks(frame_bgr, landmarks);
    }
    
    // Draw mesh overlay if enabled
    if (state_.show_mesh) {
        face_processor_->DrawMesh(frame_bgr, landmarks, state_.show_mesh_dense);
    }
    
    // Draw facemask overlay if enabled
    if (state_.show_facemask) {
        cv::Mat base_weight = ::BuildSkinWeightMap(regions, frame_bgr.size(), 2.0f, 0.5f, frame_bgr);
        cv::Mat facemask = ::CreateRefinedFaceMask(regions, frame_bgr.size(), base_weight);
        
        // Convert to 8-bit for visualization
        cv::Mat facemask_8u;
        facemask.convertTo(facemask_8u, CV_8U, 255.0);
        
        // Apply color map to make it visible (green for face mask)
        cv::Mat colored_mask;
        cv::applyColorMap(facemask_8u, colored_mask, cv::COLORMAP_VIRIDIS);
        
        // Blend with original frame
        cv::Mat overlay;
        cv::addWeighted(frame_bgr, 0.7, colored_mask, 0.3, 0, overlay);
        frame_bgr = overlay;
    }

    cv::Mat wrinkle_mask;
    cv::Mat wrinkle_mask_for_display;
    const bool segmentation_requested = state_.show_wrinkle_segmentation;
    bool need_wrinkle_mask = segmentation_requested ||
                             (beauty_state_.fx_skin && (beauty_state_.fx_skin_adv || beauty_state_.fx_wrinkle_preview) &&
                              (beauty_state_.fx_skin_wrinkle || beauty_state_.fx_wrinkle_preview));
    if (need_wrinkle_mask) {
        if (wrinkle_segmenter_ && wrinkle_segmenter_->IsLoaded()) {
            // Optimize: Only run inference every N frames to reduce lag
            bool should_run_inference = (wrinkle_frame_counter_ % kWrinkleSegmentationFrameSkip == 0);
            
            if (should_run_inference || cached_wrinkle_mask_.empty()) {
                float processing_scale = 1.0f;
                if (beauty_state_.fx_skin && beauty_state_.fx_skin_adv) {
                    processing_scale = std::clamp(GetProcessingScale(), 0.3f, 1.0f);
                }
                wrinkle_mask = wrinkle_segmenter_->PredictMask(frame_bgr, regions, processing_scale);
                
                // Cache the result for reuse in skipped frames
                if (!wrinkle_mask.empty()) {
                    cached_wrinkle_mask_ = wrinkle_mask.clone();
                }
            } else {
                // Reuse cached mask from previous inference
                if (!cached_wrinkle_mask_.empty() && 
                    cached_wrinkle_mask_.size() == frame_bgr.size()) {
                    wrinkle_mask = cached_wrinkle_mask_.clone();
                }
            }
            
            wrinkle_frame_counter_++;
        }

        if (wrinkle_mask.empty()) {
            WrinkleMaskConfig mask_config;
            mask_config.min_scale_px = beauty_state_.fx_wrinkle_custom_scales ? beauty_state_.fx_wrinkle_min_px : 1.5f;
            mask_config.max_scale_px = beauty_state_.fx_wrinkle_custom_scales ? beauty_state_.fx_wrinkle_max_px : 3.0f;
            mask_config.suppress_lower_face = beauty_state_.fx_wrinkle_suppress_lower;
            mask_config.lower_face_ratio = beauty_state_.fx_wrinkle_lower_ratio;
            mask_config.ignore_glasses = beauty_state_.fx_wrinkle_ignore_glasses;
            mask_config.glasses_margin_px = beauty_state_.fx_wrinkle_glasses_margin;
            mask_config.keep_ratio = beauty_state_.fx_wrinkle_keep_ratio;
            mask_config.use_skin_gate = beauty_state_.fx_wrinkle_use_skin_gate;
            mask_config.mask_gain = beauty_state_.fx_wrinkle_mask_gain;
            wrinkle_mask = BuildWrinkleLineMask(frame_bgr, regions, mask_config);
        }

        if (!wrinkle_mask.empty()) {
            // Lighter temporal smoothing for faster face tracking response
            static cv::Mat prev_wrinkle_mask;
            if (!prev_wrinkle_mask.empty() &&
                prev_wrinkle_mask.size() == wrinkle_mask.size() &&
                prev_wrinkle_mask.type() == wrinkle_mask.type()) {
                cv::Mat smoothed;
                // 70% current frame, 30% previous frame (faster response than old 25/50/25 split)
                cv::addWeighted(wrinkle_mask, 0.70, prev_wrinkle_mask, 0.30, 0.0, smoothed);
                wrinkle_mask = smoothed;
            }
            prev_wrinkle_mask = wrinkle_mask.clone();
            cv::GaussianBlur(wrinkle_mask, wrinkle_mask, cv::Size(0, 0), 1.5);

            // Suppress wrinkle detections in high-risk regions (eyes, brows, mouth)
            cv::Mat refined_regions = CreateRefinedFaceMask(regions, wrinkle_mask.size(), cv::Mat());
            if (!refined_regions.empty()) {
                cv::Mat refined_f32;
                if (refined_regions.type() != CV_32F) {
                    refined_regions.convertTo(refined_f32, CV_32F);
                } else {
                    refined_f32 = refined_regions;
                }
                if (refined_f32.size() != wrinkle_mask.size()) {
                    cv::resize(refined_f32, refined_f32, wrinkle_mask.size(), 0, 0, cv::INTER_LINEAR);
                }
                cv::multiply(wrinkle_mask, refined_f32, wrinkle_mask);
            }

            // Additional eye safety margin: explicitly zero a band around the eyes
            auto zero_eye_band = [&](const std::vector<cv::Point>& eye, float inflate_px, float drop_px) {
                if (eye.empty()) return;
                cv::Rect eye_rect = cv::boundingRect(eye);
                eye_rect.x = std::max(0, (int)std::round(eye_rect.x - inflate_px));
                eye_rect.width = std::min(wrinkle_mask.cols - eye_rect.x,
                                          (int)std::round(eye_rect.width + 2 * inflate_px));
                int expand_top = (int)std::round(inflate_px * 0.6f);
                int expand_bottom = (int)std::round(drop_px);
                eye_rect.y = std::max(0, eye_rect.y - expand_top);
                eye_rect.height = std::min(wrinkle_mask.rows - eye_rect.y,
                                           eye_rect.height + expand_top + expand_bottom);
                if (eye_rect.width > 0 && eye_rect.height > 0) {
                    cv::rectangle(wrinkle_mask, eye_rect, cv::Scalar(0.0f), cv::FILLED);
                }
            };

            zero_eye_band(regions.left_eye, 14.0f, 18.0f);
            zero_eye_band(regions.right_eye, 14.0f, 18.0f);
        }

        if (segmentation_requested && !wrinkle_mask.empty()) {
            wrinkle_mask_for_display = wrinkle_mask.clone();
        }
    }

    // Apply skin smoothing
    cv::Mat wrinkle_inpaint_debug;
    cv::Mat* debug_mask_ptr = (state_.show_wrinkle_inpaint ? &wrinkle_inpaint_debug : nullptr);
    if (debug_mask_ptr) {
        debug_mask_ptr->release();
    }

    if (beauty_state_.fx_skin) {
        if (beauty_state_.fx_skin_adv || beauty_state_.fx_wrinkle_preview) {
            ApplySkinSmoothingAdvanced(frame_bgr, regions, landmarks, wrinkle_mask, debug_mask_ptr);
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

    const cv::Mat& overlay_mask = !wrinkle_mask_for_display.empty() ? wrinkle_mask_for_display : wrinkle_mask;
    if (state_.show_wrinkle_segmentation && !overlay_mask.empty()) {
        double min_val = 0.0, max_val = 0.0;
        cv::minMaxLoc(overlay_mask, &min_val, &max_val);

        if (max_val - min_val > 1e-6) {
            double cutoff = min_val + (max_val - min_val) * 0.4;  // keep only stronger responses

            cv::Mat mask_binary_float;
            cv::threshold(overlay_mask, mask_binary_float, cutoff, 1.0, cv::THRESH_BINARY);

            cv::Mat mask_binary_u8;
            mask_binary_float.convertTo(mask_binary_u8, CV_8U, 255.0);

            if (cv::countNonZero(mask_binary_u8) > 0) {
                cv::Mat normalized_u8;
                overlay_mask.convertTo(normalized_u8, CV_8U, 255.0 / (max_val - min_val), -min_val * 255.0 / (max_val - min_val));

                cv::Mat colored;
                cv::applyColorMap(normalized_u8, colored, cv::COLORMAP_MAGMA);

                cv::Mat highlight;
                cv::addWeighted(frame_bgr, 1.0, colored, 0.35, 0.0, highlight);
                highlight.copyTo(frame_bgr, mask_binary_u8);
            }
        }
    }

    if (state_.show_wrinkle_inpaint && debug_mask_ptr && !debug_mask_ptr->empty()) {
        cv::Mat mask_display;
        if (debug_mask_ptr->type() != CV_32F) {
            debug_mask_ptr->convertTo(mask_display, CV_32F, 1.0 / 255.0);
        } else {
            mask_display = *debug_mask_ptr;
        }
        double min_val = 0.0, max_val = 0.0;
        cv::minMaxLoc(mask_display, &min_val, &max_val);
        if (max_val > min_val + 1e-6) {
            cv::Mat normalized_u8;
            mask_display.convertTo(normalized_u8, CV_8U, 255.0 / (max_val - min_val), -min_val * 255.0 / (max_val - min_val));
            cv::Mat colored;
            cv::applyColorMap(normalized_u8, colored, cv::COLORMAP_JET);
            cv::Mat highlight;
            cv::addWeighted(frame_bgr, 1.0, colored, 0.35, 0.0, highlight);
            cv::Mat binary;
            cv::threshold(mask_display, binary, 1e-3, 1.0, cv::THRESH_BINARY);
            cv::Mat binary_u8;
            binary.convertTo(binary_u8, CV_8U, 255.0);
            highlight.copyTo(frame_bgr, binary_u8);
        }
    }
}

void EffectsManager::ApplySkinSmoothing(cv::Mat& frame_bgr, const FaceRegions& regions) {
    ApplySkinSmoothingBGR(frame_bgr, regions, NormalizeAdvancedAmount(beauty_state_.fx_skin_amount), state_.opencl_enabled);
}

void EffectsManager::ApplySkinSmoothingAdvanced(cv::Mat& frame_bgr, const FaceRegions& regions,
                                               const mediapipe::NormalizedLandmarkList& landmarks,
                                               const cv::Mat& wrinkle_mask,
                                               cv::Mat* wrinkle_inpaint_debug) {
    const bool wrinkle_processing_requested = beauty_state_.fx_skin_wrinkle ||
                                              beauty_state_.fx_wrinkle_preview ||
                                              beauty_state_.fx_skin_smile_wrinkle_gain > 1e-3f;
    if (beauty_state_.fx_skin_amount < kMinSkinSmoothingAmount && !wrinkle_processing_requested) {
        return;
    }
    // Use user-configured processing scale directly for stability
    float effective_scale = beauty_state_.fx_adv_scale;
    
    // Check if processing scale optimization should be used
    if (effective_scale < 1.000f) {
        ApplySkinSmoothingWithProcessingScale(frame_bgr, regions, landmarks, wrinkle_mask);
    } else {
        // Full resolution processing
        SkinSmoothingConfig config = CreateSkinSmoothingConfig();
        FacialExpressionMetrics metrics = ApplySkinSmoothingAdvBGR(frame_bgr, regions, config, &landmarks, wrinkle_mask, wrinkle_inpaint_debug);
        // Store metrics for debug display
        state_.last_facial_metrics = metrics;
    }
}

void EffectsManager::ApplyLipEffects(cv::Mat& frame_bgr, const FaceRegions& regions, 
                                    const mediapipe::NormalizedLandmarkList& landmarks, const cv::Size& frame_size) {
    cv::Scalar lip_color_bgr(
        beauty_state_.fx_lip_color[2] * 255, // B
        beauty_state_.fx_lip_color[1] * 255, // G  
        beauty_state_.fx_lip_color[0] * 255  // R
    );
    
    LipRefinerParams params = {
        lip_color_bgr,
        beauty_state_.fx_lip_alpha,
        beauty_state_.fx_lip_feather,
        beauty_state_.fx_lip_light,
        beauty_state_.fx_lip_band,
        landmarks,
        frame_size
    };
    
    ApplyLipRefinerBGR(frame_bgr, regions, params);
}

void EffectsManager::ApplyTeethWhitening(cv::Mat& frame_bgr, const FaceRegions& regions) {
    ApplyTeethWhitenBGR(frame_bgr, regions, beauty_state_.fx_teeth_strength, beauty_state_.fx_teeth_margin);
}

void EffectsManager::Cleanup() {
    if (!state_.is_initialized) return;
    
    std::cout << "🧹 Cleaning up Effects Manager..." << std::endl;
    
    // Reset state
    state_ = EffectsState{};
    beauty_state_ = BeautyState{};
    if (wrinkle_segmenter_) {
        wrinkle_segmenter_ = std::make_unique<WrinkleSegmenter>();
    }

    std::cout << "✅ Effects Manager cleanup completed" << std::endl;
}

// Processing scale optimization for skin smoothing
void EffectsManager::ApplySkinSmoothingWithProcessingScale(cv::Mat& frame_bgr, const FaceRegions& regions,
                                                          const mediapipe::NormalizedLandmarkList& landmarks,
                                                          const cv::Mat& wrinkle_mask) {
    // Delegate to face processor with current beauty state
    face_processor_->ApplySkinSmoothingWithProcessingScale(frame_bgr, regions, landmarks, beauty_state_, wrinkle_mask);
    
    // Extract and store facial metrics for debug display
    FacialExpressionMetrics metrics = ExtractFacialExpressions(&landmarks, frame_bgr.cols, frame_bgr.rows);
    state_.last_facial_metrics = metrics;
}


} // namespace segmecam
