#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "include/effects/face_regions.h"
#include "mediapipe/framework/formats/landmark.pb.h"

// Forward declaration for NormalizedLandmarkList - now included above
// namespace mediapipe {
// class NormalizedLandmarkList;
// }

// Apply lipstick/lip-refiner using landmark lips (outer minus inner).
// color_bgr: target tint (0..255 per channel)
// strength:  0..1 amount of color shift (LAB a/b blend)
// feather_px: soft edge in pixels
// lightness: additive L* adjustment (-1..1 typical)
void ApplyLipRefinerBGR(cv::Mat& frame_bgr,
                        const FaceRegions& fr,
                        const cv::Scalar& color_bgr,
                        float strength,
                        float feather_px,
                        float lightness,
                        float band_grow_px,
                        const mediapipe::NormalizedLandmarkList& lms,
                        const cv::Size& frame_size);