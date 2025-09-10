#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include "mediapipe/framework/formats/landmark.pb.h"

struct FaceRegions {
  std::vector<cv::Point> face_oval;           // outer face polygon
  std::vector<cv::Point> lips_outer;          // outer lips polygon
  std::vector<cv::Point> lips_inner;          // inner lips polygon
  std::vector<cv::Point> left_eye;            // left eye polygon
  std::vector<cv::Point> right_eye;           // right eye polygon
};

// Extract face region polygons (pixel coords) from a NormalizedLandmarkList.
// Returns true if polygons were successfully extracted.
bool ExtractFaceRegions(const mediapipe::NormalizedLandmarkList& lms,
                        const cv::Size& frame_size,
                        FaceRegions* out,
                        bool flip_x = false,
                        bool flip_y = false,
                        bool swap_xy = false);

// Build wrinkle boost heatmap (0..1) based on smile/squint around
// mouth corners and outer eye corners. Size should be full-frame.
cv::Mat BuildWrinkleBoostMap(const mediapipe::NormalizedLandmarkList& lms,
                             const cv::Size& size,
                             float smile_boost,
                             float squint_boost);

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

// Apply simple teeth whitening inside inner lips polygon.
// strength in 0..1.
// Teeth whitening inside the inner-lips polygon, with shrink to avoid lip bleed.
// strength: 0..1 amount, shrink_px: erode margin from inner contour before whitening.
void ApplyTeethWhitenBGR(cv::Mat& frame_bgr,
                         const FaceRegions& fr,
                         float strength,
                         float shrink_px);

// Apply basic skin smoothing inside face oval, excluding lips and eyes.
// strength controls bilateral filter sigma (typical 0..1 -> 25..75).
void ApplySkinSmoothingBGR(cv::Mat& frame_bgr,
                           const FaceRegions& fr,
                           float strength,
                           bool use_ocl=false);

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
