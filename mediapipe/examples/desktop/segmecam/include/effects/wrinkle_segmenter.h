#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <opencv2/core.hpp>
#include <onnxruntime_cxx_api.h>

#include "include/effects/face_regions.h"

namespace segmecam {

class WrinkleSegmenter {
public:
    WrinkleSegmenter();

    // Load the ONNX wrinkle segmentation model. Optional override path can be provided.
    bool Initialize(const std::string& override_path = "");

    bool IsLoaded() const { return session_ != nullptr; }
    std::string ModelPath() const { return model_path_string_; }

    // Generate a wrinkle probability mask in CV_32F [0,1] matching frame size.
    // processing_scale allows callers to match the skin-smoothing resolution (1.0 = full size).
    cv::Mat PredictMask(const cv::Mat& frame_bgr, const FaceRegions& regions, float processing_scale = 1.0f);

private:
    std::filesystem::path ResolveModelPath(const std::string& override_path) const;
    cv::Mat PostProcessMask(const cv::Mat& raw_mask, const cv::Size& target_size, const FaceRegions& regions) const;

    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::SessionOptions> session_options_;
    Ort::MemoryInfo memory_info_{Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)};
    cv::Size input_size_;
    std::string model_path_string_;
    std::string input_name_;
    std::string output_name_;
    bool use_cuda_ = false;
    mutable bool warned_missing_model_ = false;
    mutable bool warned_inference_failure_ = false;
    
    // Performance tracking
    mutable int inference_count_ = 0;
    mutable double total_inference_time_ = 0.0;
};

} // namespace segmecam
