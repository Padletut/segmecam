#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "include/effects/face_regions.h"
#include "mediapipe/framework/formats/landmark.pb.h"

// Forward declaration for NormalizedLandmarkList - now included above
// namespace mediapipe {
// class NormalizedLandmarkList;
// }

// Apply basic skin smoothing inside face oval, excluding lips and eyes.
// strength controls bilateral filter sigma (typical 0..1 -> 25..75).
void ApplySkinSmoothingBGR(cv::Mat& frame_bgr,
                           const FaceRegions& fr,
                           float strength,
                           bool use_ocl = false);