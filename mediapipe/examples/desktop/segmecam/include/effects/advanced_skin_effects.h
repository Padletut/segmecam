#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "include/effects/face_regions.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/formats/classification.pb.h"

// Configuration structs to reduce parameter count
struct RegionGatesConfig {
  bool suppress_lower_face = false;
  float lower_face_ratio = 0.5f;
  bool ignore_glasses = false;
  float glasses_margin_px = 10.0f;
};

struct ExpressionBoostConfig {
  float smile_boost = 0.0f;
  float squint_boost = 0.0f;
  float forehead_boost = 0.0f;
  float forehead_margin_px = 8.0f;
};

struct WrinkleMaskConfig {
  float min_scale_px = 1.5f;
  float max_scale_px = 3.0f;
  bool suppress_lower_face = false;
  float lower_face_ratio = 0.5f;
  bool ignore_glasses = false;
  float glasses_margin_px = 10.0f;
  float keep_ratio = 0.1f;
  bool use_skin_gate = true;
  float mask_gain = 1.0f;
};

struct WrinkleBoostConfig {
  float line_min_px = 1.5f;
  float line_max_px = 3.0f;
  float keep_ratio = 0.1f;
  bool use_skin_gate = true;
  float mask_gain = 1.0f;
  float smile_wrinkle_gain = 1.0f;
  RegionGatesConfig region_gates;
};

struct FacialExpressionMetrics {
  float smile_factor;
  float squint_factor;
  cv::Point mouth_left, mouth_right;
  cv::Point nose_left, nose_right;
  cv::Point eye_left_outer, eye_left_inner;
  cv::Point eye_right_outer, eye_right_inner;
  cv::Point eye_left_top, eye_left_bottom;
  cv::Point eye_right_top, eye_right_bottom;

  // Blendshape-enhanced metrics
  float brow_furrow_factor;    // browDownLeft + browDownRight
  float eye_squint_left;       // eyeSquintLeft
  float eye_squint_right;      // eyeSquintRight
  float eye_blink_left;        // eyeBlinkLeft
  float eye_blink_right;       // eyeBlinkRight
  float cheek_squint_left;     // cheekSquintLeft
  float cheek_squint_right;    // cheekSquintRight
  float brow_inner_up;         // browInnerUp
  float brow_outer_up_left;    // browOuterUpLeft
  float brow_outer_up_right;   // browOuterUpRight
  float mouth_pucker;          // mouthPucker
  float mouth_left_expr;       // mouthLeft
  float mouth_right_expr;      // mouthRight
  float mouth_upper_up_left;   // mouthUpperUpLeft
  float mouth_upper_up_right;  // mouthUpperUpRight
};

struct SkinSmoothingConfig {
  float amount = 0.0f;
  float radius_px = 8.0f;
  float texture_thresh = 0.15f;
  float edge_feather_px = 8.0f;
  ExpressionBoostConfig expression;
  WrinkleBoostConfig wrinkle;
  float smile_wrinkle_gain = 1.0f;
  float boost_gain = 1.0f;
  bool wrinkle_enabled = false;
  bool wrinkle_preview = false;
  float baseline_boost = 0.25f;
  float neg_atten_cap = 0.8f;
};

// Helper function declarations
cv::Mat ApplyRegionGates(cv::Size sz, const FaceRegions& fr, const RegionGatesConfig& config);
void ApplyLowerFaceSuppression(cv::Mat& gate, const FaceRegions& fr, float lower_face_ratio);
void ApplyGlassesSuppression(cv::Mat& gate, const FaceRegions& fr, float glasses_margin_px);
cv::Rect GetEyesBoundingRect(const FaceRegions& fr);
cv::Mat BuildExpressionBoostMap(const FacialExpressionMetrics& metrics, cv::Size frame_size,
                               const ExpressionBoostConfig& config, const FaceRegions& fr,
                               const cv::Mat& frame_bgr);
void AddNasolabialBoost(cv::Mat& boost, const FacialExpressionMetrics& metrics, float smile_boost);
void AddSquintBoost(cv::Mat& boost, const FacialExpressionMetrics& metrics, float squint_boost, float eff_squint);
void AddForeheadBoost(cv::Mat& boost, const cv::Mat& frame_bgr, const FaceRegions& fr,
                     float forehead_boost, float forehead_margin_px);
cv::Mat BuildWrinkleBoostMap(const cv::Mat& frame_bgr, const FaceRegions& fr,
                            const FacialExpressionMetrics& metrics, const WrinkleBoostConfig& config,
                            const cv::Mat& Lf, const cv::Mat& base);
cv::Mat BuildWrinkleLineMask(const cv::Mat& frame_bgr, const FaceRegions& fr, const WrinkleMaskConfig& config);

// Enhanced facial expression extraction with blendshape support
FacialExpressionMetrics ExtractFacialExpressions(const mediapipe::NormalizedLandmarkList* lms, 
                                               int width, int height,
                                               const mediapipe::ClassificationList* blendshapes = nullptr);

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
                             const WrinkleMaskConfig& config);

// Advanced LAB frequency separation smoothing guided by weight map from landmarks.
// - amount: attenuation of high-frequency detail (0..1).
// - radius_px: Gaussian radius for base layer (pixels).
// - texture_thresh: see BuildSkinWeightMap.
// - edge_feather_px: see BuildSkinWeightMap.
FacialExpressionMetrics ApplySkinSmoothingAdvBGR(cv::Mat& frame_bgr,
                              const FaceRegions& fr,
                              const SkinSmoothingConfig& config,
                              const mediapipe::NormalizedLandmarkList* lms,
                              const mediapipe::ClassificationList* blendshapes = nullptr);