#include "include/effects/wrinkle_segmenter.h"

#include <iostream>
#include <vector>

#include <opencv2/imgproc.hpp>
#include <google/protobuf/stubs/common.h>

namespace segmecam {

namespace {
constexpr char kDefaultModelName[] = "wrinkle_model_v3_128x128.onnx";

// Ensure protobuf runtime is initialised exactly once before using OpenCV's
// ONNX importer. Without this, certain runtimes crash inside protobuf's
// thread-local arena setup when loading large ONNX graphs.
void EnsureProtobufInitialized() {
    static const bool initialized = [] {
        GOOGLE_PROTOBUF_VERIFY_VERSION;
        return true;
    }();
    (void)initialized;
}

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

WrinkleSegmenter::WrinkleSegmenter() : input_size_(128, 128) {}

bool WrinkleSegmenter::Initialize(const std::string& override_path) {
    std::filesystem::path model_path = ResolveModelPath(override_path);
    if (model_path.empty()) {
        if (!warned_missing_model_) {
            std::cerr << "⚠️  WrinkleSegmenter: Unable to locate ONNX model. Wrinkle segmentation disabled." << std::endl;
            warned_missing_model_ = true;
        }
        return false;
    }

    const int prev_threads = cv::getNumThreads();
    const bool prev_opt = cv::useOptimized();
    auto restore_flags = [&]() {
        cv::setUseOptimized(prev_opt);
        cv::setNumThreads(prev_threads);
    };

    try {
        cv::setUseOptimized(false);
        cv::setNumThreads(1);

        EnsureProtobufInitialized();

        net_ = cv::dnn::readNetFromONNX(model_path.string());
        restore_flags();

        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        model_path_string_ = model_path.string();
        std::cout << "✨ Wrinkle segmentation model loaded from: " << model_path_string_ << std::endl;
        return true;
    } catch (const cv::Exception& e) {
        restore_flags();
        std::cerr << "❌ WrinkleSegmenter: Failed to load model '" << model_path.string() << "': " << e.what() << std::endl;
        net_ = cv::dnn::Net();
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

cv::Mat WrinkleSegmenter::PredictMask(const cv::Mat& frame_bgr, const FaceRegions& regions) const {
    if (frame_bgr.empty() || !IsLoaded()) {
        return cv::Mat();
    }

    auto compute_face_roi = [&]() -> cv::Rect {
        if (regions.face_oval.empty()) {
            return cv::Rect(0, 0, frame_bgr.cols, frame_bgr.rows);
        }

        cv::Rect base = cv::boundingRect(regions.face_oval);
        int margin = static_cast<int>(std::round(std::max(base.width, base.height) * 0.15f));

        base.x = std::max(0, base.x - margin);
        base.y = std::max(0, base.y - margin);
        base.width = std::min(frame_bgr.cols - base.x, base.width + margin * 2);
        base.height = std::min(frame_bgr.rows - base.y, base.height + margin * 2);

        if (base.width <= 0 || base.height <= 0) {
            return cv::Rect(0, 0, frame_bgr.cols, frame_bgr.rows);
        }
        return base;
    };

    const cv::Rect face_roi = compute_face_roi();
    cv::Mat face_region = frame_bgr(face_roi);

    try {
        cv::Mat resized;
        cv::resize(face_region, resized, input_size_);

        cv::Mat blob = cv::dnn::blobFromImage(resized, 1.0 / 255.0, input_size_, cv::Scalar(), true, false, CV_32F);
        net_.setInput(blob);
        cv::Mat output = net_.forward();

        cv::Mat raw;
        if (output.dims == 4 && output.size[0] == 1) {
            int channels = output.size[1];
            int height = output.size[2];
            int width = output.size[3];
            if (channels != 1) {
                std::cerr << "⚠️  WrinkleSegmenter: Unexpected channel count " << channels << " in model output." << std::endl;
                return cv::Mat();
            }
            raw = cv::Mat(height, width, CV_32F, output.ptr<float>()).clone();
        } else if (output.dims == 3) {
            raw = output.clone().reshape(1, output.size[1]);
        } else {
            std::cerr << "⚠️  WrinkleSegmenter: Unsupported output shape (dims=" << output.dims << ")." << std::endl;
            return cv::Mat();
        }

        cv::Mat prob = Sigmoid(raw);
        cv::Mat prob_resized;
        cv::resize(prob, prob_resized, face_region.size(), 0, 0, cv::INTER_LINEAR);

        cv::Mat mask_full(frame_bgr.size(), CV_32F, cv::Scalar(0.0f));
        prob_resized.copyTo(mask_full(face_roi));

        return PostProcessMask(mask_full, frame_bgr.size(), regions);
    } catch (const cv::Exception& e) {
        if (!warned_inference_failure_) {
            std::cerr << "❌ WrinkleSegmenter: Inference failed: " << e.what() << std::endl;
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

    cv::GaussianBlur(mask_float, mask_float, cv::Size(0, 0), 1.2);
    return mask_float;
}

} // namespace segmecam
