#include "include/effects/wrinkle_segmenter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>
#include <numeric>

#include <opencv2/imgproc.hpp>

namespace segmecam {

namespace {
constexpr char kDefaultModelName[] = "wrinkle_model_v3_128x128.onnx";

cv::Mat Sigmoid(const cv::Mat& input) {
    cv::Mat neg;
    cv::multiply(input, cv::Scalar(-1.0f), neg);
    cv::Mat exp_neg;
    cv::exp(neg, exp_neg);
    cv::Mat denom;
    cv::add(exp_neg, cv::Scalar(1.0f), denom);
    cv::Mat sigmoid;
    cv::divide(1.0f, denom, sigmoid);
    return sigmoid;
}

void SubtractFeatureRegion(cv::Mat& mask, const std::vector<cv::Point>& polygon) {
    if (polygon.empty() || mask.empty()) {
        return;
    }
    cv::Mat feature_mask(mask.size(), CV_8U, cv::Scalar(0));
    std::vector<std::vector<cv::Point>> polys{polygon};
    cv::fillPoly(feature_mask, polys, cv::Scalar(255));
    cv::Mat feature_mask_f;
    feature_mask.convertTo(feature_mask_f, CV_32F, 1.0f / 255.0f);
    cv::Mat inverse = cv::Mat::ones(mask.size(), CV_32F) - feature_mask_f;
    cv::multiply(mask, inverse, mask);
}

} // namespace

WrinkleSegmenter::WrinkleSegmenter() : input_size_(128, 128) {
    try {
        env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "WrinkleSegmenter");
    } catch (const Ort::Exception& e) {
        std::cerr << "❌ Failed to initialize ONNX Runtime environment: " << e.what() << std::endl;
    }
}

bool WrinkleSegmenter::Initialize(const std::string& override_path) {
    std::filesystem::path model_path = ResolveModelPath(override_path);
    if (model_path.empty()) {
        if (!warned_missing_model_) {
            std::cerr << "⚠️  WrinkleSegmenter: Unable to locate ONNX model. Wrinkle segmentation disabled." << std::endl;
            warned_missing_model_ = true;
        }
        return false;
    }

    if (!env_) {
        std::cerr << "❌ WrinkleSegmenter: ONNX Runtime environment not initialized" << std::endl;
        return false;
    }

    try {
        session_options_ = std::make_unique<Ort::SessionOptions>();
        session_options_->SetIntraOpNumThreads(1);
        session_options_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

        // Try to enable CUDA provider
        try {
            OrtCUDAProviderOptions cuda_options;
            cuda_options.device_id = 0;
            cuda_options.arena_extend_strategy = 0;
            cuda_options.gpu_mem_limit = 2ULL * 1024 * 1024 * 1024; // 2GB
            cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;
            cuda_options.do_copy_in_default_stream = 1;
            
            session_options_->AppendExecutionProvider_CUDA(cuda_options);
            use_cuda_ = true;
            std::cout << "🚀 Wrinkle segmentation using CUDA execution provider" << std::endl;
        } catch (const Ort::Exception& cuda_error) {
            std::cerr << "⚠️ CUDA not available for wrinkle segmentation, using CPU: " << cuda_error.what() << std::endl;
            use_cuda_ = false;
        }

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
        std::cout << "✨ Wrinkle segmentation model loaded from: " << model_path_string_ << std::endl;
        std::cout << "   Input: " << input_name_ << ", Output: " << output_name_ << std::endl;
        return true;
    } catch (const Ort::Exception& e) {
        std::cerr << "❌ WrinkleSegmenter: Failed to load model '" << model_path.string() << "': " << e.what() << std::endl;
        session_.reset();
        return false;
    }
}

std::filesystem::path WrinkleSegmenter::ResolveModelPath(const std::string& override_path) const {
    std::vector<std::filesystem::path> candidates;
    if (!override_path.empty()) {
        candidates.emplace_back(override_path);
    }
    candidates.emplace_back("models" / std::filesystem::path(kDefaultModelName));
    candidates.emplace_back("mediapipe/examples/desktop/segmecam/models" / std::filesystem::path(kDefaultModelName));
    candidates.emplace_back(std::filesystem::path("..") / "models" / kDefaultModelName);

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (!candidate.empty() && std::filesystem::exists(candidate, ec) && !std::filesystem::is_directory(candidate, ec)) {
            std::error_code canonical_ec;
            auto canonical_path = std::filesystem::canonical(candidate, canonical_ec);
            if (!canonical_ec) {
                return canonical_path;
            }
            return candidate;
        }
    }
    return {};
}

namespace {

FaceRegions ScaleFaceRegions(const FaceRegions& regions, float scale) {
    FaceRegions scaled;
    auto scale_poly = [&](const std::vector<cv::Point>& in) {
        std::vector<cv::Point> out;
        out.reserve(in.size());
        for (const auto& p : in) {
            out.emplace_back(static_cast<int>(std::round(p.x * scale)),
                             static_cast<int>(std::round(p.y * scale)));
        }
        return out;
    };

    scaled.face_oval = scale_poly(regions.face_oval);
    scaled.lips_outer = scale_poly(regions.lips_outer);
    scaled.lips_inner = scale_poly(regions.lips_inner);
    scaled.left_eye = scale_poly(regions.left_eye);
    scaled.right_eye = scale_poly(regions.right_eye);
    return scaled;
}

} // namespace

cv::Mat WrinkleSegmenter::PredictMask(const cv::Mat& frame_bgr,
                                      const FaceRegions& regions,
                                      float processing_scale) {
    if (!IsLoaded()) {
        return cv::Mat();
    }

    float scale = std::clamp(processing_scale, 0.3f, 1.0f);
    bool use_scaled_path = scale < 0.999f;

    cv::Mat scaled_frame;
    FaceRegions scaled_regions = regions;
    if (use_scaled_path) {
        cv::resize(frame_bgr, scaled_frame, cv::Size(), scale, scale, cv::INTER_AREA);
        scaled_regions = ScaleFaceRegions(regions, scale);
    }

    const cv::Mat& inference_frame = use_scaled_path ? scaled_frame : frame_bgr;
    const FaceRegions& inference_regions = use_scaled_path ? scaled_regions : regions;

    auto compute_face_roi = [&](const FaceRegions& regs, const cv::Size& frame_size) -> cv::Rect {
        if (regs.face_oval.empty()) {
            return cv::Rect(0, 0, frame_size.width, frame_size.height);
        }

        cv::Rect base = cv::boundingRect(regs.face_oval);
        int margin = static_cast<int>(std::round(std::max(base.width, base.height) * 0.15f));

        base.x = std::max(0, base.x - margin);
        base.y = std::max(0, base.y - margin);
        base.width = std::min(frame_size.width - base.x, base.width + margin * 2);
        base.height = std::min(frame_size.height - base.y, base.height + margin * 2);

        if (base.width <= 0 || base.height <= 0) {
            return cv::Rect(0, 0, frame_size.width, frame_size.height);
        }
        return base;
    };

    const cv::Rect face_roi = compute_face_roi(inference_regions, inference_frame.size());
    cv::Mat face_region = inference_frame(face_roi);

    try {
        // Start timing
        auto start_time = std::chrono::high_resolution_clock::now();
        
        cv::Mat resized;
        cv::resize(face_region, resized, input_size_);

        // Convert BGR to RGB and normalize to [0,1]
        cv::Mat rgb;
        cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
        rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

        // Create NCHW tensor (1, 3, H, W)
        std::vector<float> input_tensor_values(1 * 3 * input_size_.height * input_size_.width);
        for (int c = 0; c < 3; ++c) {
            for (int h = 0; h < input_size_.height; ++h) {
                for (int w = 0; w < input_size_.width; ++w) {
                    int tensor_idx = c * (input_size_.height * input_size_.width) + h * input_size_.width + w;
                    input_tensor_values[tensor_idx] = rgb.at<cv::Vec3f>(h, w)[c];
                }
            }
        }

        // Create input tensor
        std::vector<int64_t> input_shape = {1, 3, input_size_.height, input_size_.width};
        auto input_tensor = Ort::Value::CreateTensor<float>(
            memory_info_, 
            input_tensor_values.data(), 
            input_tensor_values.size(),
            input_shape.data(), 
            input_shape.size()
        );

        // Run inference
        const char* input_names[] = {input_name_.c_str()};
        const char* output_names[] = {output_name_.c_str()};
        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_names, &input_tensor, 1,
            output_names, 1
        );

        // Get output tensor
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
        
        // Extract H, W from output shape (should be [1, 1, H, W])
        int out_h = static_cast<int>(output_shape[2]);
        int out_w = static_cast<int>(output_shape[3]);
        
        // Create cv::Mat from raw logits
        cv::Mat raw(out_h, out_w, CV_32F, output_data);
        cv::Mat raw_clone = raw.clone(); // Clone to own the data

        // Apply sigmoid to get probabilities
        cv::Mat prob = Sigmoid(raw_clone);
        
        // End timing
        auto end_time = std::chrono::high_resolution_clock::now();
        double inference_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        // Update performance tracking
        inference_count_++;
        total_inference_time_ += inference_ms;
        double avg_inference_ms = total_inference_time_ / inference_count_;
        
        // Log performance every 30 inferences
        if (inference_count_ % 30 == 0) {
            std::cout << "🔍 Wrinkle model inference #" << inference_count_ 
                      << ": " << inference_ms << "ms (avg: " << avg_inference_ms << "ms)"
                      << (use_cuda_ ? " [GPU]" : " [CPU]") << std::endl;
        }
        
        cv::Mat prob_resized;
        cv::resize(prob, prob_resized, face_region.size(), 0, 0, cv::INTER_LINEAR);

        cv::Mat mask_full(inference_frame.size(), CV_32F, cv::Scalar(0.0f));
        prob_resized.copyTo(mask_full(face_roi));

        cv::Mat processed = PostProcessMask(mask_full, inference_frame.size(), inference_regions);

        if (use_scaled_path) {
            cv::Mat upscaled;
            cv::resize(processed, upscaled, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
            cv::GaussianBlur(upscaled, upscaled, cv::Size(0, 0), 1.0);
            // Final gating at original resolution for consistency
            return PostProcessMask(upscaled, frame_bgr.size(), regions);
        }

        return processed;
    } catch (const Ort::Exception& e) {
        if (!warned_inference_failure_) {
            std::cerr << "❌ WrinkleSegmenter: ONNX Runtime inference failed: " << e.what() << std::endl;
            warned_inference_failure_ = true;
        }
        return cv::Mat();
    }
}

cv::Mat WrinkleSegmenter::PostProcessMask(const cv::Mat& raw_mask, const cv::Size& target_size, const FaceRegions& regions) const {
    if (raw_mask.empty()) {
        return cv::Mat();
    }

    cv::Mat mask_float;
    if (raw_mask.type() == CV_32F) {
        mask_float = raw_mask.clone();
    } else {
        raw_mask.convertTo(mask_float, CV_32F);
    }

    cv::min(mask_float, 1.0f, mask_float);
    cv::max(mask_float, 0.0f, mask_float);

    if (!regions.face_oval.empty()) {
        cv::Mat face_gate(target_size, CV_8U, cv::Scalar(0));
        std::vector<std::vector<cv::Point>> polys{regions.face_oval};
        cv::fillPoly(face_gate, polys, cv::Scalar(255));
        cv::Mat face_gate_f;
        face_gate.convertTo(face_gate_f, CV_32F, 1.0f / 255.0f);
        cv::multiply(mask_float, face_gate_f, mask_float);
    }

    // Remove features that should remain sharp
    SubtractFeatureRegion(mask_float, regions.left_eye);
    SubtractFeatureRegion(mask_float, regions.right_eye);
    SubtractFeatureRegion(mask_float, regions.lips_outer);
    SubtractFeatureRegion(mask_float, regions.lips_inner);

    // Filter out small scattered regions that cause wobbling
    // Convert to binary mask for connected components analysis
    cv::Mat mask_binary;
    cv::threshold(mask_float, mask_binary, 0.054f, 1.0f, cv::THRESH_BINARY);
    mask_binary.convertTo(mask_binary, CV_8U, 255.0);
    
    // Find connected components
    cv::Mat labels, stats, centroids;
    int num_labels = cv::connectedComponentsWithStats(mask_binary, labels, stats, centroids, 8, CV_32S);
    
    // Calculate minimum area threshold (e.g., 0.1% of frame area)
    // Adjust this value: higher = removes more small regions, lower = keeps more
    int min_area = static_cast<int>(target_size.width * target_size.height * 0.00013f); // 0.013% of frame
    
    // Create filtered mask: only keep components larger than min_area
    cv::Mat filtered_mask = cv::Mat::zeros(target_size, CV_8U);
    for (int i = 1; i < num_labels; i++) { // Skip background (label 0)
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area >= min_area) {
            // Keep this component
            cv::Mat component_mask = (labels == i);
            filtered_mask.setTo(255, component_mask);
        }
    }
    
    // Convert back to float and apply to original mask
    cv::Mat filtered_mask_f;
    filtered_mask.convertTo(filtered_mask_f, CV_32F, 1.0f / 255.0f);
    cv::multiply(mask_float, filtered_mask_f, mask_float);

    cv::GaussianBlur(mask_float, mask_float, cv::Size(0, 0), 1.2);
    return mask_float;
}

} // namespace segmecam
