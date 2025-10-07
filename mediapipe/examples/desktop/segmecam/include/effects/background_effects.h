#ifndef BACKGROUND_EFFECTS_H
#define BACKGROUND_EFFECTS_H

#include <opencv2/core.hpp>
#include <string>

namespace segmecam {

struct BeautyState; // Forward declaration

class BackgroundEffects {
public:
    BackgroundEffects();
    ~BackgroundEffects();

    // Background effect application
    cv::Mat ApplyBackgroundEffect(const cv::Mat& frame_bgr, const cv::Mat& segmentation_mask,
                                const BeautyState& beauty_state);

    // Background image management
    bool LoadBackgroundImage(const std::string& path);
    void ClearBackgroundImage();

private:
    // Background effect implementations
    cv::Mat ApplyBlurBackground(const cv::Mat& frame_bgr, const cv::Mat& mask);
    cv::Mat ApplyImageBackground(const cv::Mat& frame_bgr, const cv::Mat& mask);
    cv::Mat ApplySolidBackground(const cv::Mat& frame_bgr, const cv::Mat& mask, const cv::Scalar& color);

    // Helper methods
    cv::Mat ResizeMaskIfNeeded(const cv::Mat& mask, const cv::Size& target_size);

    // Background image storage
    cv::Mat background_image_;
};

} // namespace segmecam

#endif // BACKGROUND_EFFECTS_H