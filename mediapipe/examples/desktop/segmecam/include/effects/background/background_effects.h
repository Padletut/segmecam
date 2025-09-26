#pragma once

#include <opencv2/opencv.hpp>
#include "effects/segmecam_face_effects.h"
#include "effects/presets.h"

namespace segmecam {

/**
 * Background Effects Module
 *
 * Handles all background replacement effects including:
 * - Blur backgrounds
 * - Image backgrounds
 * - Solid color backgrounds
 * - Mask visualization
 */
class BackgroundEffects {
public:
    BackgroundEffects();
    ~BackgroundEffects();

    // Main background effect application
    cv::Mat ApplyBackgroundEffect(const cv::Mat& frame_bgr, const cv::Mat& mask,
                                 const BeautyState& beauty_state, bool use_ocl);

    // Individual background effect types
    cv::Mat ApplyBlurBackground(const cv::Mat& frame_bgr, const cv::Mat& mask,
                               int blur_strength, float feather_px, bool use_ocl, float scale);
    cv::Mat ApplyImageBackground(const cv::Mat& frame_bgr, const cv::Mat& mask,
                                const cv::Mat& bg_image, bool use_ocl, float scale);
    cv::Mat ApplySolidBackground(const cv::Mat& frame_bgr, const cv::Mat& mask,
                                const cv::Scalar& color, bool use_ocl, float scale);

    // Mask visualization
    cv::Mat ApplyMaskVisualization(const cv::Mat& resized_mask);

    // Background image management
    void SetBackgroundImage(const cv::Mat& image) { background_image_ = image.clone(); }

    // Utility methods
    cv::Mat ResizeMaskIfNeeded(const cv::Mat& mask, const cv::Size& target_size);
    cv::Scalar ConvertRGBColorToBGR(float r, float g, float b);

private:
    // Helper methods
    cv::Mat ApplyBackgroundModeEffect(const cv::Mat& frame_bgr, const cv::Mat& resized_mask,
                                     int mode, int blur_strength, float feather_px,
                                     const cv::Mat& bg_image, const cv::Scalar& solid_color);
    cv::Mat ApplyDefaultBackgroundEffect(const cv::Mat& frame_bgr);
    
    // Member variables
    cv::Mat background_image_;
};

} // namespace segmecam