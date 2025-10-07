#include "effects/background/background_effects.h"
#include "render/segmecam_composite.h"
#include "application/app_state.h"
#include <iostream>

namespace segmecam {

BackgroundEffects::BackgroundEffects() {
    // Constructor - no initialization needed
}

BackgroundEffects::~BackgroundEffects() {
    // Destructor - no cleanup needed
}

cv::Mat BackgroundEffects::ApplyBackgroundEffect(const cv::Mat& frame_bgr, const cv::Mat& mask,
                                               const BeautyState& beauty_state, bool use_ocl) {
    cv::Mat resized_mask = ResizeMaskIfNeeded(mask, frame_bgr.size());

    // Check if user wants to show mask visualization (overrides all other background effects)
    if (beauty_state.show_mask && !resized_mask.empty()) {
        return ApplyMaskVisualization(resized_mask);
    }

    // Apply background effect based on mode
    switch (beauty_state.bg_mode) {
        case 0: // NONE
            return frame_bgr.clone(); // Return original frame with no background effect
        case 1: // BLUR
            return ApplyBlurBackground(frame_bgr, resized_mask, beauty_state.blur_strength,
                                     beauty_state.feather_px, use_ocl, beauty_state.fx_adv_scale);
        case 2: // IMAGE
            if (!background_image_.empty()) {
                return ApplyImageBackground(frame_bgr, resized_mask, background_image_, use_ocl, beauty_state.fx_adv_scale);
            }
            // Fall through to solid if no image loaded
        case 3: // SOLID
        default:
            {
                cv::Scalar bg_color = ConvertRGBColorToBGR(beauty_state.solid_color[0],
                                                         beauty_state.solid_color[1],
                                                         beauty_state.solid_color[2]);
                return ApplySolidBackground(frame_bgr, resized_mask, bg_color, use_ocl, beauty_state.fx_adv_scale);
            }
    }
}

cv::Mat BackgroundEffects::ApplyMaskVisualization(const cv::Mat& mask) {
    // Create a more visible mask visualization with color mapping
    cv::Mat visualization;

    // Apply colormap to make the mask more visible (red for foreground, blue for background)
    cv::applyColorMap(mask, visualization, cv::COLORMAP_JET);

    return visualization;
}

cv::Mat BackgroundEffects::ApplyBlurBackground(const cv::Mat& frame_bgr, const cv::Mat& mask,
                                             int blur_strength, float feather_px, bool use_ocl, float scale) {
    cv::Mat result = CompositeBlurBackgroundBGR_Accel(frame_bgr, mask, blur_strength, feather_px,
                                           use_ocl, scale);
    return result;
}

cv::Mat BackgroundEffects::ApplyImageBackground(const cv::Mat& frame_bgr, const cv::Mat& mask,
                                              const cv::Mat& bg_image, bool use_ocl, float scale) {
    // Removed debug output for image background result size
    return CompositeImageBackgroundBGR_Accel(frame_bgr, mask, bg_image, use_ocl, scale);
}

cv::Mat BackgroundEffects::ApplySolidBackground(const cv::Mat& frame_bgr, const cv::Mat& mask,
                                              const cv::Scalar& color, bool use_ocl, float scale) {
    cv::Mat result = CompositeSolidBackgroundBGR_Accel(frame_bgr, mask, color,
                                           use_ocl, scale);
    return result;
}

cv::Mat BackgroundEffects::ApplyDefaultBackgroundEffect(const cv::Mat& frame_bgr) {
    // Convert BGR to RGB for display
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

cv::Mat BackgroundEffects::ResizeMaskIfNeeded(const cv::Mat& mask, const cv::Size& target_size) {
    if (mask.empty()) return mask;

    if (mask.cols != target_size.width || mask.rows != target_size.height) {
        cv::Mat resized;
        cv::resize(mask, resized, target_size, 0, 0, cv::INTER_LINEAR);
        return resized;
    }

    return mask;
}

cv::Scalar BackgroundEffects::ConvertRGBColorToBGR(float r, float g, float b) {
    return cv::Scalar(
        std::clamp(b * 255.0f, 0.0f, 255.0f),
        std::clamp(g * 255.0f, 0.0f, 255.0f),
        std::clamp(r * 255.0f, 0.0f, 255.0f)
    );
}

} // namespace segmecam