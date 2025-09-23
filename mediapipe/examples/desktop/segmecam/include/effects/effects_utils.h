#ifndef MEDIAPIPE_EXAMPLES_DESKTOP_SEGMECAM_EFFECTS_UTILS_H_
#define MEDIAPIPE_EXAMPLES_DESKTOP_SEGMECAM_EFFECTS_UTILS_H_

#include <opencv2/opencv.hpp>

namespace effects_utils {

// Applies Gaussian blur feathering to a mask
void featherMask(cv::Mat& mask, int ksize);

} // namespace effects_utils

#endif // MEDIAPIPE_EXAMPLES_DESKTOP_SEGMECAM_EFFECTS_UTILS_H_