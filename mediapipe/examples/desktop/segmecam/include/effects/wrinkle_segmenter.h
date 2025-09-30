#pragma once

#include <filesystem>
#include <string>
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

#include "include/effects/face_regions.h"

namespace segmecam {

class WrinkleSegmenter {
public:
    WrinkleSegmenter();

    // Load the ONNX wrinkle segmentation model. Optional override path can be provided.
    bool Initialize(const std::string& override_path = "");

    bool IsLoaded() const { return !net_.empty(); }
    std::string ModelPath() const { return model_path_string_; }

    // Generate a wrinkle probability mask in CV_32F [0,1] matching frame size.
    cv::Mat PredictMask(const cv::Mat& frame_bgr, const FaceRegions& regions) const;

private:
    std::filesystem::path ResolveModelPath(const std::string& override_path) const;
    cv::Mat PostProcessMask(const cv::Mat& raw_mask, const cv::Size& target_size, const FaceRegions& regions) const;

    mutable cv::dnn::Net net_;
    cv::Size input_size_;
    std::string model_path_string_;
    mutable bool warned_missing_model_ = false;
    mutable bool warned_inference_failure_ = false;
};

} // namespace segmecam
