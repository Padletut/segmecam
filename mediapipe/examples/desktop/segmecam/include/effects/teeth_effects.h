#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "include/effects/face_regions.h"
#include "mediapipe/framework/formats/landmark.pb.h"

// Forward declaration for NormalizedLandmarkList - now included above
// namespace mediapipe {
// class NormalizedLandmarkList;
// }

// Apply simple teeth whitening inside inner lips polygon.
// strength in 0..1.
// Teeth whitening inside the inner-lips polygon, with shrink to avoid lip bleed.
// strength: 0..1 amount, shrink_px: erode margin from inner contour before whitening.
void ApplyTeethWhitenBGR(cv::Mat& frame_bgr,
                         const FaceRegions& fr,
                         float strength,
                         float shrink_px);