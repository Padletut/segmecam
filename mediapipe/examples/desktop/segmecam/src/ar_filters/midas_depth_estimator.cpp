// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0

#include "include/ar_filters/midas_depth_estimator.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <opencv2/imgproc.hpp>

namespace segmecam {
namespace ar_filters {

namespace {
constexpr char kDefaultModelName[] = "midas_v21_small_256.onnx";

// MiDaS v2.1 Small model expects 256x256 RGB input
constexpr int kModelInputSize = 256;

// ImageNet normalization (MiDaS is pretrained on ImageNet)
constexpr float kMean[3] = {0.485f, 0.456f, 0.406f};  // RGB
constexpr float kStd[3] = {0.229f, 0.224f, 0.225f};   // RGB

} // namespace

MidasDepthEstimator::MidasDepthEstimator() 
    : input_size_(kModelInputSize, kModelInputSize) {
    try {
        env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "MidasDepthEstimator");
    } catch (const Ort::Exception& e) {
        std::cerr << "❌ Failed to initialize ONNX Runtime environment: " << e.what() << std::endl;
    }
}

std::filesystem::path MidasDepthEstimator::ResolveModelPath(const std::string& override_path) const {
    // If override path provided, use it directly
    if (!override_path.empty()) {
        std::filesystem::path p(override_path);
        if (std::filesystem::exists(p)) {
            return p;
        }
        std::cerr << "⚠️  Override model path does not exist: " << override_path << std::endl;
    }
    
    // Search in standard locations
    std::vector<std::filesystem::path> search_paths = {
        std::filesystem::path("models") / kDefaultModelName,
        std::filesystem::path("assets") / "models" / kDefaultModelName,
        std::filesystem::path("../models") / kDefaultModelName,
        std::filesystem::path("../../models") / kDefaultModelName,
    };
    
    for (const auto& path : search_paths) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    
    return {};
}

bool MidasDepthEstimator::Initialize(const std::string& override_path) {
    std::filesystem::path model_path = ResolveModelPath(override_path);
    if (model_path.empty()) {
        if (!warned_missing_model_) {
            std::cerr << "⚠️  MidasDepthEstimator: Unable to locate ONNX model '" 
                     << kDefaultModelName << "'. Depth estimation disabled." << std::endl;
            std::cerr << "    Download from: https://github.com/isl-org/MiDaS/releases/download/v2_1/model-small.onnx" 
                     << std::endl;
            warned_missing_model_ = true;
        }
        return false;
    }
    
    if (!env_) {
        std::cerr << "❌ MidasDepthEstimator: ONNX Runtime environment not initialized" << std::endl;
        return false;
    }
    
    try {
        session_options_ = std::make_unique<Ort::SessionOptions>();
        session_options_->SetIntraOpNumThreads(2);  // MiDaS benefits from multi-threading
        session_options_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        
        // Try to enable CUDA provider for GPU acceleration
        try {
            OrtCUDAProviderOptions cuda_options;
            cuda_options.device_id = 0;
            cuda_options.arena_extend_strategy = 0;
            cuda_options.gpu_mem_limit = 2ULL * 1024 * 1024 * 1024;  // 2GB
            cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;
            cuda_options.do_copy_in_default_stream = 1;
            
            session_options_->AppendExecutionProvider_CUDA(cuda_options);
            use_cuda_ = true;
            std::cout << "🚀 MiDaS depth estimation using CUDA execution provider" << std::endl;
        } catch (const Ort::Exception& cuda_error) {
            std::cerr << "⚠️  CUDA not available for depth estimation, using CPU: " 
                     << cuda_error.what() << std::endl;
            use_cuda_ = false;
        }
        
        // Create session
#ifdef _WIN32
        std::wstring wide_path(model_path.wstring());
        session_ = std::make_unique<Ort::Session>(*env_, wide_path.c_str(), *session_options_);
#else
        session_ = std::make_unique<Ort::Session>(*env_, model_path.c_str(), *session_options_);
#endif
        
        // Get input/output names
        Ort::AllocatorWithDefaultOptions allocator;
        auto input_name_ptr = session_->GetInputNameAllocated(0, allocator);
        auto output_name_ptr = session_->GetOutputNameAllocated(0, allocator);
        input_name_ = input_name_ptr.get();
        output_name_ = output_name_ptr.get();
        
        model_path_string_ = model_path.string();
        
        std::cout << "✅ MiDaS depth estimator initialized: " << model_path_string_ << std::endl;
        std::cout << "   Input: " << input_name_ << " (" << input_size_.width << "x" 
                 << input_size_.height << ")" << std::endl;
        std::cout << "   Output: " << output_name_ << std::endl;
        std::cout << "   Provider: " << (use_cuda_ ? "CUDA (GPU)" : "CPU") << std::endl;
        
        return true;
        
    } catch (const Ort::Exception& e) {
        std::cerr << "❌ Failed to load MiDaS model: " << e.what() << std::endl;
        return false;
    }
}

std::vector<float> MidasDepthEstimator::PreprocessImage(const cv::Mat& frame_rgb) const {
    // Resize to 256x256
    cv::Mat resized;
    cv::resize(frame_rgb, resized, input_size_, 0, 0, cv::INTER_LINEAR);
    
    // Convert to float [0, 1]
    cv::Mat float_img;
    resized.convertTo(float_img, CV_32FC3, 1.0 / 255.0);
    
    // ImageNet normalization
    std::vector<cv::Mat> channels(3);
    cv::split(float_img, channels);
    
    for (int c = 0; c < 3; ++c) {
        channels[c] = (channels[c] - kMean[c]) / kStd[c];
    }
    
    // Convert to CHW format (channels first)
    // ONNX expects [1, 3, 256, 256]
    std::vector<float> input_data;
    input_data.reserve(3 * kModelInputSize * kModelInputSize);
    
    for (int c = 0; c < 3; ++c) {
        const float* ptr = channels[c].ptr<float>();
        input_data.insert(input_data.end(), ptr, ptr + kModelInputSize * kModelInputSize);
    }
    
    return input_data;
}

cv::Mat MidasDepthEstimator::PostprocessDepth(const std::vector<float>& output_data,
                                              const cv::Size& target_size) const {
    // Create 256x256 depth map
    cv::Mat depth_map(input_size_, CV_32F);
    std::copy(output_data.begin(), output_data.end(), depth_map.ptr<float>());
    
    // MiDaS outputs inverse depth (higher = closer)
    // Normalize to [0, 1] range
    double min_val, max_val;
    cv::minMaxLoc(depth_map, &min_val, &max_val);
    
    if (max_val > min_val) {
        depth_map = (depth_map - min_val) / (max_val - min_val);
    }
    
    // Resize to target size
    if (target_size != input_size_) {
        cv::resize(depth_map, depth_map, target_size, 0, 0, cv::INTER_LINEAR);
    }
    
    return depth_map;
}

cv::Mat MidasDepthEstimator::EstimateDepthMap(const cv::Mat& frame_rgb) {
    if (!IsLoaded() || frame_rgb.empty()) {
        return cv::Mat();
    }
    
    // Validate input format
    if (frame_rgb.type() != CV_8UC3) {
        if (!warned_inference_failure_) {
            std::cerr << "⚠️  MidasDepthEstimator: Expected CV_8UC3 RGB image" << std::endl;
            warned_inference_failure_ = true;
        }
        return cv::Mat();
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Preprocess image
        std::vector<float> input_data = PreprocessImage(frame_rgb);
        
        // Create input tensor
        std::vector<int64_t> input_shape = {1, 3, kModelInputSize, kModelInputSize};
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info_, input_data.data(), input_data.size(),
            input_shape.data(), input_shape.size()
        );
        
        // Run inference
        const char* input_names[] = {input_name_.c_str()};
        const char* output_names[] = {output_name_.c_str()};
        
        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_names, &input_tensor, 1,
            output_names, 1
        );
        
        // Extract output data
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
        
        size_t output_size = kModelInputSize * kModelInputSize;
        std::vector<float> output_vec(output_data, output_data + output_size);
        
        // Postprocess depth map
        cv::Mat depth_map = PostprocessDepth(output_vec, frame_rgb.size());
        
        // Update performance stats
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        total_inference_time_ += elapsed_ms;
        inference_count_++;
        
        return depth_map;
        
    } catch (const Ort::Exception& e) {
        if (!warned_inference_failure_) {
            std::cerr << "❌ MidasDepthEstimator: Inference failed: " << e.what() << std::endl;
            warned_inference_failure_ = true;
        }
        return cv::Mat();
    }
}

float MidasDepthEstimator::EstimateRegionDepth(const cv::Mat& frame_rgb, 
                                               const cv::Rect2f& region) {
    cv::Mat depth_map = EstimateDepthMap(frame_rgb);
    if (depth_map.empty()) {
        return -1.0f;
    }
    
    // Convert normalized region to pixel coordinates
    int x = static_cast<int>(region.x * depth_map.cols);
    int y = static_cast<int>(region.y * depth_map.rows);
    int w = static_cast<int>(region.width * depth_map.cols);
    int h = static_cast<int>(region.height * depth_map.rows);
    
    // Clamp to image bounds
    x = std::max(0, std::min(x, depth_map.cols - 1));
    y = std::max(0, std::min(y, depth_map.rows - 1));
    w = std::max(1, std::min(w, depth_map.cols - x));
    h = std::max(1, std::min(h, depth_map.rows - y));
    
    // Extract region and compute average depth
    cv::Rect roi(x, y, w, h);
    cv::Mat region_depth = depth_map(roi);
    
    return static_cast<float>(cv::mean(region_depth)[0]);
}

float MidasDepthEstimator::EstimateFaceDepth(const cv::Mat& frame_rgb, 
                                             float nose_x, float nose_y,
                                             int sample_radius_px) {
    cv::Mat depth_map = EstimateDepthMap(frame_rgb);
    if (depth_map.empty()) {
        return -1.0f;
    }
    
    // Convert normalized coordinates to pixels
    int center_x = static_cast<int>(nose_x * depth_map.cols);
    int center_y = static_cast<int>(nose_y * depth_map.rows);
    
    // Sample circular region around nose tip
    std::vector<float> samples;
    samples.reserve(sample_radius_px * sample_radius_px * 4);
    
    for (int dy = -sample_radius_px; dy <= sample_radius_px; ++dy) {
        for (int dx = -sample_radius_px; dx <= sample_radius_px; ++dx) {
            // Check if within circle
            if (dx * dx + dy * dy > sample_radius_px * sample_radius_px) {
                continue;
            }
            
            int px = center_x + dx;
            int py = center_y + dy;
            
            // Check bounds
            if (px >= 0 && px < depth_map.cols && py >= 0 && py < depth_map.rows) {
                samples.push_back(depth_map.at<float>(py, px));
            }
        }
    }
    
    if (samples.empty()) {
        return -1.0f;
    }
    
    // Return median depth (more robust than mean)
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

float MidasDepthEstimator::InverseDepthToMeters(float inverse_depth,
                                                float calibration_depth,
                                                float calibration_inverse) const {
    // MiDaS outputs relative inverse depth (not metric)
    // We need calibration to convert to meters
    // 
    // If at distance D meters, inverse depth is I:
    //   inverse_depth ∝ 1 / distance
    //   distance = D * (calibration_inverse / inverse_depth)
    
    if (inverse_depth <= 0.0f || calibration_inverse <= 0.0f) {
        return -1.0f;
    }
    
    return calibration_depth * (calibration_inverse / inverse_depth);
}

} // namespace ar_filters
} // namespace segmecam
