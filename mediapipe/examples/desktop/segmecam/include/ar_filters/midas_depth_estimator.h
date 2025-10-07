// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// MidasDepthEstimator - Monocular depth estimation using MiDaS ONNX model
//
// Provides real-time depth estimation from single camera frames using
// the MiDaS v2.1 Small model. Used for accurate AR filter depth tracking.

#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <opencv2/core.hpp>
#include <onnxruntime_cxx_api.h>

namespace segmecam {
namespace ar_filters {

/**
 * MiDaS depth estimation for monocular AR depth tracking
 * 
 * Uses MiDaS v2.1 Small model (256x256 input) for real-time performance:
 * - Model: ~22MB ONNX file
 * - Input: 256x256 RGB image
 * - Output: 256x256 inverse depth map (higher values = closer)
 * - Performance: ~8ms on CUDA GPU, ~30ms on CPU
 * 
 * Usage:
 *   MidasDepthEstimator estimator;
 *   estimator.Initialize();
 *   float depth = estimator.EstimateFaceDepth(frame_rgb, face_bbox);
 */
class MidasDepthEstimator {
public:
    MidasDepthEstimator();
    ~MidasDepthEstimator() = default;
    
    /**
     * Initialize MiDaS depth estimator
     * @param override_path Optional custom model path (default: models/midas_v21_small_256.onnx)
     * @return true if model loaded successfully
     */
    bool Initialize(const std::string& override_path = "");
    
    /**
     * Check if model is loaded and ready
     */
    bool IsLoaded() const { return session_ != nullptr; }
    
    /**
     * Get loaded model path
     */
    std::string ModelPath() const { return model_path_string_; }
    
    /**
     * Estimate depth map for entire frame
     * @param frame_rgb Input frame in RGB format (any size, will be resized)
     * @return Depth map (CV_32F, same size as input, normalized 0-1, higher=closer)
     */
    cv::Mat EstimateDepthMap(const cv::Mat& frame_rgb);
    
    /**
     * Estimate average depth for a specific region (e.g., face bounding box)
     * @param frame_rgb Input frame in RGB format
     * @param region Region of interest (normalized 0-1 coordinates)
     * @return Average depth in region (0-1, higher=closer) or -1.0 on error
     */
    float EstimateRegionDepth(const cv::Mat& frame_rgb, const cv::Rect2f& region);
    
    /**
     * Estimate face depth from face landmarks (nose tip position)
     * @param frame_rgb Input frame in RGB format
     * @param nose_x Nose tip X coordinate (normalized 0-1)
     * @param nose_y Nose tip Y coordinate (normalized 0-1)
     * @param sample_radius_px Sampling radius around nose tip (default: 10 pixels)
     * @return Average depth around nose tip (0-1, higher=closer) or -1.0 on error
     */
    float EstimateFaceDepth(const cv::Mat& frame_rgb, float nose_x, float nose_y, 
                           int sample_radius_px = 10);
    
    /**
     * Convert inverse depth to metric depth (meters)
     * @param inverse_depth Inverse depth from model (0-1)
     * @param calibration_depth Known depth in meters (for calibration)
     * @param calibration_inverse Inverse depth value at calibration_depth
     * @return Estimated depth in meters
     */
    float InverseDepthToMeters(float inverse_depth, 
                              float calibration_depth = 1.0f,
                              float calibration_inverse = 0.5f) const;
    
    /**
     * Get model input size
     */
    cv::Size GetInputSize() const { return input_size_; }
    
    /**
     * Get performance statistics
     */
    struct PerformanceStats {
        int inference_count = 0;
        double average_inference_time_ms = 0.0;
        double total_inference_time_ms = 0.0;
        bool using_cuda = false;
    };
    
    PerformanceStats GetPerformanceStats() const {
        return {
            inference_count_,
            inference_count_ > 0 ? total_inference_time_ / inference_count_ : 0.0,
            total_inference_time_,
            use_cuda_
        };
    }
    
    /**
     * Reset performance statistics
     */
    void ResetStats() {
        inference_count_ = 0;
        total_inference_time_ = 0.0;
    }

private:
    // ONNX Runtime components
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::SessionOptions> session_options_;
    Ort::MemoryInfo memory_info_{Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)};
    
    // Model configuration
    cv::Size input_size_;          // Model input size (256x256 for Small model)
    std::string model_path_string_;
    std::string input_name_;
    std::string output_name_;
    bool use_cuda_ = false;
    
    // Cached depth map (to avoid recomputing for multiple queries)
    cv::Mat cached_depth_map_;
    int64_t cached_frame_id_ = -1;
    
    // Performance tracking
    mutable int inference_count_ = 0;
    mutable double total_inference_time_ = 0.0;
    
    // Warning flags
    mutable bool warned_missing_model_ = false;
    mutable bool warned_inference_failure_ = false;
    
    /**
     * Resolve model path (search in models/ and assets/ directories)
     */
    std::filesystem::path ResolveModelPath(const std::string& override_path) const;
    
    /**
     * Preprocess image for MiDaS inference
     * - Resize to 256x256
     * - Normalize to [0, 1]
     * - Convert to CHW format (channels first)
     */
    std::vector<float> PreprocessImage(const cv::Mat& frame_rgb) const;
    
    /**
     * Postprocess MiDaS output
     * - Reshape to 256x256
     * - Normalize to [0, 1] range
     * - Resize to original frame size
     */
    cv::Mat PostprocessDepth(const std::vector<float>& output_data, 
                            const cv::Size& target_size) const;
};

} // namespace ar_filters
} // namespace segmecam
