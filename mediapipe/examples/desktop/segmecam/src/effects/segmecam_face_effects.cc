#include "include/effects/segmecam_face_effects.h"
#include <cmath>

// Legacy function for backward compatibility - delegates to advanced skin effects
cv::Mat BuildWrinkleBoostMap(const mediapipe::NormalizedLandmarkList& lms,
                             const cv::Size& size,
                             float smile_boost,
                             float squint_boost) {
  // This is a legacy function that was used for basic wrinkle boost mapping
  // In the new architecture, this functionality is integrated into ApplySkinSmoothingAdvBGR
  // Return an empty matrix to indicate this function is deprecated
  return cv::Mat(size, CV_32F, cv::Scalar(0.0f));
}