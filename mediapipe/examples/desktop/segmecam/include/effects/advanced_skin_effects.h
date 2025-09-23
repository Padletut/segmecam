#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "include/effects/face_regions.h"
#include "mediapipe/framework/formats/landmark.pb.h"

// Forward declaration for NormalizedLandmarkList - now included above
// namespace mediapipe {
// class NormalizedLandmarkList;
// }

// Build a high-quality skin weight map (0..1 float) using landmarks.
// - edge_feather_px: width of the falloff near face contour in pixels.
// - texture_thresh: reduce weight near strong gradients (0..1, higher keeps more texture).
cv::Mat BuildSkinWeightMap(const FaceRegions& fr,
                           const cv::Size& frame_size,
                           float edge_feather_px,
                           float texture_thresh,
                           const cv::Mat& hint_bgr = cv::Mat());

// Build a wrinkle mask emphasizing dark, narrow, linear structures on skin.
// Returns CV_32F in [0,1]. Only inside face region (excluding lips/eyes).
cv::Mat BuildWrinkleLineMask(const cv::Mat& frame_bgr,
                             const FaceRegions& fr,
                             float min_scale_px,
                             float max_scale_px,
                             bool suppress_lower_face = false,
                             float lower_face_ratio = 0.45f,
                             bool ignore_glasses = false,
                             float glasses_margin_px = 10.0f,
                             float keep_ratio = 0.12f,
                             bool use_skin_gate = true,
                             float mask_gain = 1.0f);

// Advanced LAB frequency separation smoothing guided by weight map from landmarks.
// - amount: attenuation of high-frequency detail (0..1).
// - radius_px: Gaussian radius for base layer (pixels).
// - texture_thresh: see BuildSkinWeightMap.
// - edge_feather_px: see BuildSkinWeightMap.
void ApplySkinSmoothingAdvBGR(cv::Mat& frame_bgr,
                              const FaceRegions& fr,
                              float amount,
                              float radius_px,
                              float texture_thresh,
                              float edge_feather_px,
                              const mediapipe::NormalizedLandmarkList* lms,
                              float smile_boost,
                              float squint_boost,
                              float forehead_boost,
                              float boost_gain,
                              bool suppress_lower_face = false,
                              float lower_face_ratio = 0.45f,
                              bool ignore_glasses = false,
                              float glasses_margin_px = 10.0f,
                              float keep_ratio = 0.12f,
                              float line_min_px = -1.0f,
                              float line_max_px = -1.0f,
                              float forehead_margin_px = 8.0f,
                              bool wrinkle_preview = false,
                              float baseline_boost = 0.25f,
                              bool use_skin_gate = true,
                              float mask_gain = 1.0f,
                              float neg_atten_cap = 0.8f);