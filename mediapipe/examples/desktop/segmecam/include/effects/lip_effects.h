#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "include/effects/face_regions.h"
#include "mediapipe/framework/formats/landmark.pb.h"

// Forward declaration for NormalizedLandmarkList - now included above
// namespace mediapipe {
// class NormalizedLandmarkList;
// }

// Parameters for lip refinement effects
struct LipRefinerParams {
    cv::Scalar color_bgr;      // target tint (0..255 per channel)
    float strength;            // 0..1 amount of color shift (LAB a/b blend)
    float feather_px;          // soft edge in pixels
    float lightness;           // additive L* adjustment (-1..1 typical)
    float band_grow_px;        // band growth in pixels
    const mediapipe::NormalizedLandmarkList& lms;  // landmark list reference
    const cv::Size& frame_size; // frame size reference
};

// Apply lipstick/lip-refiner using landmark lips (outer minus inner).
void ApplyLipRefinerBGR(cv::Mat& frame_bgr,
                        const FaceRegions& fr,
                        const LipRefinerParams& params);