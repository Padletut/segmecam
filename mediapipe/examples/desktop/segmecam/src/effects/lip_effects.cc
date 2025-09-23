#include "include/effects/lip_effects.h"
#include <cmath>

namespace {
static void featherMask(cv::Mat& mask, int ksize) {
  if (ksize <= 1) return;
  int k = (ksize | 1);
  cv::GaussianBlur(mask, mask, cv::Size(k, k), 0);
}
} // namespace

void ApplyLipRefinerBGR(cv::Mat& frame_bgr,
                        const FaceRegions& fr,
                        const cv::Scalar& color_bgr,
                        float strength,
                        float feather_px,
                        float lightness,
                        float band_grow_px,
                        const mediapipe::NormalizedLandmarkList& lms,
                        const cv::Size& frame_size) {
  strength = std::clamp(strength, 0.0f, 1.0f);
  if (strength <= 0.0f || fr.lips_outer.empty()) return;

  // Build separate upper/lower lip polygons from landmark arc indices
  auto make_poly = [&](const std::vector<int>& arc_outer_idx,
                       const std::vector<int>& arc_inner_idx) {
    auto idx_to_pt = [&](int idx){
      const auto& p = lms.landmark(idx);
      int x = std::clamp((int)std::round(p.x()*frame_size.width), 0, frame_size.width-1);
      int y = std::clamp((int)std::round(p.y()*frame_size.height), 0, frame_size.height-1);
      return cv::Point(x,y);
    };
    std::vector<cv::Point> arc_outer; arc_outer.reserve(arc_outer_idx.size());
    for (int i : arc_outer_idx) arc_outer.push_back(idx_to_pt(i));
    std::vector<cv::Point> arc_inner; arc_inner.reserve(arc_inner_idx.size());
    for (int i : arc_inner_idx) arc_inner.push_back(idx_to_pt(i));
    std::vector<cv::Point> poly = arc_outer;
    for (int i = (int)arc_inner.size()-1; i >= 0; --i) poly.push_back(arc_inner[i]);
    return poly;
  };

  // Canonical MediaPipe lip arcs (outer/inner, upper/lower)
  static const int OUTER_UP[] = {61,146,91,181,84,17,314,405,321,375,291};
  static const int OUTER_LO[] = {61,185,40,39,37,0,267,269,270,409,291};
  static const int INNER_UP[] = {78,95,88,178,87,14,317,402,318,324,308};
  static const int INNER_LO[] = {78,191,80,81,82,13,312,311,310,415,308};
  std::vector<int> ou(OUTER_UP, OUTER_UP+11), ol(OUTER_LO, OUTER_LO+11), iu(INNER_UP, INNER_UP+11), il(INNER_LO, INNER_LO+11);

  cv::Mat mask(frame_bgr.size(), CV_8UC1, cv::Scalar(0));
  if (!ou.empty() && !iu.empty()) {
    auto poly_top = make_poly(ou, iu);
    cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{poly_top}, cv::Scalar(255));
  }
  if (!ol.empty() && !il.empty()) {
    auto poly_bot = make_poly(ol, il);
    cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{poly_bot}, cv::Scalar(255));
  }
  // Slight dilate to unify seam between halves
  if (band_grow_px > 0.5f) {
    int k = std::max(1, (int)std::round(band_grow_px));
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
    cv::dilate(mask, mask, kernel);
  }
  if (feather_px > 0.5f) featherMask(mask, (int)std::round(feather_px));
  if (feather_px > 0.5f) featherMask(mask, (int)std::round(feather_px));

  // Convert to LAB for perceptual color shift
  cv::Mat lab; cv::cvtColor(frame_bgr, lab, cv::COLOR_BGR2Lab);
  std::vector<cv::Mat> ch; cv::split(lab, ch); // L(0..255), a(0..255), b(0..255)
  cv::Mat mask_f; mask.convertTo(mask_f, CV_32FC1, (float)strength / 255.0f); // scaled by strength

  // Convert target color to Lab
  cv::Mat patch(1,1,CV_8UC3, color_bgr);
  cv::Mat patch_lab; cv::cvtColor(patch, patch_lab, cv::COLOR_BGR2Lab);
  cv::Vec3b labv = patch_lab.at<cv::Vec3b>(0,0);
  float La = (float)labv[0];
  float Aa = (float)labv[1];
  float Ba = (float)labv[2];

  // Prepare float channels
  cv::Mat Lf, Af, Bf; ch[0].convertTo(Lf, CV_32F); ch[1].convertTo(Af, CV_32F); ch[2].convertTo(Bf, CV_32F);

  // Blend a/b toward target using mask_f
  Af = Af.mul(1.0f - mask_f) + (Aa * mask_f);
  Bf = Bf.mul(1.0f - mask_f) + (Ba * mask_f);

  // Optional lightness adjustment on L (in Lab units)
  float dL = std::clamp(lightness, -1.0f, 1.0f) * 25.0f; // typical gentle range
  if (std::abs(dL) > 1e-3f) Lf = Lf + dL * mask_f;

  // Merge back
  cv::Mat L8, A8, B8; Lf.convertTo(L8, CV_8U); Af.convertTo(A8, CV_8U); Bf.convertTo(B8, CV_8U);
  std::vector<cv::Mat> merged = {L8, A8, B8};
  cv::merge(merged, lab);
  cv::cvtColor(lab, frame_bgr, cv::COLOR_Lab2BGR);
}