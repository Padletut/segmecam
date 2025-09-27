#include "include/effects/advanced_skin_effects.h"
#include <cmath>
#include "include/effects/advanced_skin_effects.h"
#include <cmath>
#include <opencv2/photo.hpp>


// ROI-based inpainting blending function
static inline void blendInpaintedROI(
    cv::Mat& baseBGR, cv::Rect roi,
    const cv::Mat& inpaintSmallBGR,
    const cv::Mat& maskSmall8U,
    float gain)
{
    if (baseBGR.empty() || inpaintSmallBGR.empty() || maskSmall8U.empty()) return;

    // 1) Clamp ROI and get view in base
    roi &= cv::Rect(0,0,baseBGR.cols, baseBGR.rows);
    if (roi.width<=0 || roi.height<=0) return;
    cv::Mat dstROI = baseBGR(roi);

    // 2) Scale up result + mask to ROI size
    cv::Mat resUp, maskUp;
    cv::resize(inpaintSmallBGR, resUp, dstROI.size(), 0, 0, cv::INTER_LINEAR);
    cv::resize(maskSmall8U,    maskUp, dstROI.size(), 0, 0, cv::INTER_LINEAR);

    // 3) Convert to same dtype/channels before blending
    cv::Mat dstF, resF, aF, a3;
    dstROI.convertTo(dstF, CV_32F, 1.0/255.0);
    resUp.convertTo(resF, CV_32F, 1.0/255.0);
    if (maskUp.type()!=CV_8U) {
        cv::Mat tmp; maskUp.convertTo(tmp, CV_8U); maskUp = tmp;
    }
    maskUp.convertTo(aF, CV_32F, 1.0/255.0);          // 1c [0..1]
    cv::merge(std::vector<cv::Mat>{aF,aF,aF}, a3);    // 3c [0..1] (avoid 1c*3c mismatch)

    // 4) Blend: base + gain * a * (res - base)
    cv::Mat diff, weighted_diff, outF;
    cv::subtract(resF, dstF, diff);
    cv::multiply(diff, a3, weighted_diff);
    cv::multiply(weighted_diff, gain, weighted_diff);
    cv::add(dstF, weighted_diff, outF);

    // 5) Convert back in place
    outF.convertTo(dstROI, CV_8U, 255.0);
}

// Helper functions - defined first to avoid forward declaration issues
cv::Mat ApplyLowerFaceSuppression(cv::Size sz, const FaceRegions& fr, float lower_face_ratio) {
  if (fr.face_oval.empty() || fr.lips_outer.empty()) {
    return cv::Mat::ones(sz, CV_32F);
  }

  int mouth_y = 0;
  for (const auto& p : fr.lips_outer) mouth_y += p.y;
  mouth_y = (int)std::round((double)mouth_y / std::max(1,(int)fr.lips_outer.size()));

  int chin_y = 0;
  for (const auto& p : fr.face_oval) chin_y = std::max(chin_y, p.y);

  int cut_y = mouth_y + (int)std::round(std::clamp(lower_face_ratio, 0.2f, 0.8f) * (chin_y - mouth_y));

  cv::Mat gate = cv::Mat::zeros(sz, CV_32F);
  cv::rectangle(gate, cv::Rect(0, 0, sz.width, std::max(0, cut_y)), cv::Scalar(1.0f), cv::FILLED);
  return gate;
}

void ApplyGlassesSuppression(cv::Mat& extra_gate, const FaceRegions& fr, float glasses_margin_px) {
  if (fr.left_eye.empty() && fr.right_eye.empty()) return;

  cv::Rect er = GetEyesBoundingRect(fr);
  int m = (int)std::round(std::max(0.0f, glasses_margin_px));

  er.x = std::max(0, er.x - m);
  er.y = std::max(0, er.y - m);
  er.width = std::min(extra_gate.cols - er.x, er.width + 2*m);
  er.height = std::min(extra_gate.rows - er.y, er.height + 2*m);

  // Zero inside the band
  cv::rectangle(extra_gate, er, cv::Scalar(0.0f), cv::FILLED);
  // Feather edges slightly for smooth transition
  cv::GaussianBlur(extra_gate, extra_gate, cv::Size(0,0), 2.0);
}

cv::Rect GetEyesBoundingRect(const FaceRegions& fr) {
  auto rect_of = [](const std::vector<cv::Point>& poly){ return cv::boundingRect(poly); };

  cv::Rect er;
  if (!fr.left_eye.empty()) er = rect_of(fr.left_eye);
  if (!fr.right_eye.empty()) {
    cv::Rect right_rect = rect_of(fr.right_eye);
    er = er.area() ? (er | right_rect) : right_rect;
  }
  return er;
}

void AddNasolabialBoost(cv::Mat& boost, const FacialExpressionMetrics& metrics, float smile_boost) {
  if (smile_boost <= 0.0f) return;

  cv::Mat mask = cv::Mat::zeros(boost.size(), CV_32F);
  // Use mouth corners as nasolabial fold approximation
  cv::circle(mask, metrics.mouth_left, 12, cv::Scalar(smile_boost), cv::FILLED);
  cv::circle(mask, metrics.mouth_right, 12, cv::Scalar(smile_boost), cv::FILLED);
  cv::GaussianBlur(mask, mask, cv::Size(0,0), 8.0);
  boost = cv::max(boost, mask);
}

void AddSquintBoost(cv::Mat& boost, const FacialExpressionMetrics& metrics, float squint_boost, float eff_squint) {
  if (squint_boost <= 0.0f || eff_squint <= 0.0f) return;

  cv::Mat mask = cv::Mat::zeros(boost.size(), CV_32F);
  // Boost outer eye corners when squinting
  cv::circle(mask, metrics.eye_left_outer, 8, cv::Scalar(squint_boost * eff_squint), cv::FILLED);
  cv::circle(mask, metrics.eye_right_outer, 8, cv::Scalar(squint_boost * eff_squint), cv::FILLED);
  cv::GaussianBlur(mask, mask, cv::Size(0,0), 6.0);
  boost = cv::max(boost, mask);
}

void AddForeheadBoost(cv::Mat& boost, const cv::Mat& frame_bgr, const FaceRegions& fr,
                     const FacialExpressionMetrics& metrics, float forehead_boost, float forehead_margin_px) {
  if (forehead_boost <= 0.0f || fr.face_oval.empty()) return;

  // Calculate forehead region from face oval and eye positions
  cv::Point forehead_top = cv::Point(
    (metrics.eye_left_outer.x + metrics.eye_right_outer.x) / 2,
    fr.face_oval[0].y - 30  // Top of face oval minus offset
  );

  cv::Rect forehead_rect;
  forehead_rect.x = std::max(0, (int)(forehead_top.x - 40));
  forehead_rect.y = std::max(0, (int)(forehead_top.y - 20));
  forehead_rect.width = std::min(frame_bgr.cols - forehead_rect.x, 80);
  forehead_rect.height = std::min(frame_bgr.rows - forehead_rect.y, 40);

  if (forehead_rect.width <= 0 || forehead_rect.height <= 0) return;

  // Analyze vertical gradients in forehead region
  cv::Mat forehead_roi = frame_bgr(forehead_rect);
  cv::Mat gray; cv::cvtColor(forehead_roi, gray, cv::COLOR_BGR2GRAY);
  cv::Mat gx, gy; cv::Sobel(gray, gx, CV_32F, 1, 0, 3); cv::Sobel(gray, gy, CV_32F, 0, 1, 3);
  cv::Mat grad_y = cv::abs(gy); // Vertical gradients indicate horizontal lines

  // Normalize and threshold
  double max_grad; cv::minMaxLoc(grad_y, nullptr, &max_grad);
  if (max_grad > 1e-6) grad_y.convertTo(grad_y, CV_32F, 1.0/max_grad);
  cv::threshold(grad_y, grad_y, 0.3, 1.0, cv::THRESH_BINARY);

  // Scale by boost factor and place in boost map
  cv::Mat scaled_grad; grad_y.convertTo(scaled_grad, CV_32F, forehead_boost);
  scaled_grad.copyTo(boost(forehead_rect));
}

// Helper functions for BuildWrinkleLineMask
cv::Mat PrepareWrinkleDetectionData(const cv::Mat& frame_bgr, cv::Size sz) {
  // Convert to LAB -> L channel (8U)
  cv::Mat lab;
  cv::cvtColor(frame_bgr, lab, cv::COLOR_BGR2Lab);
  std::vector<cv::Mat> ch;
  cv::split(lab, ch);
  return ch[0]; // L channel
}

cv::Mat CombineWrinkleComponents(const cv::Mat& acc, const cv::Mat& coh,
                                const cv::Mat& base_f, const cv::Mat& skin_f,
                                const cv::Mat& extra_gate) {
  cv::Mat wr;
  cv::multiply(acc, coh, wr);
  cv::multiply(wr, base_f, wr);
  cv::multiply(wr, skin_f, wr);
  cv::multiply(wr, extra_gate, wr);
  cv::GaussianBlur(wr, wr, cv::Size(0,0), 1.0);
  wr = cv::min(cv::max(wr, 0.0f), 1.0f);
  return wr;
}

// Helper functions for ApplySkinSmoothingAdvBGR
cv::Mat PrepareLabLuminance(const cv::Mat& frame_bgr) {
  cv::Mat lab;
  cv::cvtColor(frame_bgr, lab, cv::COLOR_BGR2Lab);
  std::vector<cv::Mat> ch;
  cv::split(lab, ch);
  cv::Mat Lf;
  ch[0].convertTo(Lf, CV_32F, 1.0/255.0);
  return Lf;
}

cv::Mat CreateGaussianBase(const cv::Mat& Lf, float radius_px) {
  int k = std::max(1, (int)std::round(radius_px) * 2 + 1);
  cv::Mat base;
  cv::GaussianBlur(Lf, base, cv::Size(k, k), radius_px);
  return base;
}

cv::Mat CombineBoostMaps(const cv::Mat& expression_boost, const cv::Mat& wrinkle_boost,
                        float baseline_boost, const cv::Mat& face_gate) {
  // Validate input matrices
  if (expression_boost.empty() || wrinkle_boost.empty() || face_gate.empty()) {
    std::cerr << "CombineBoostMaps: Empty input matrices" << std::endl;
    return cv::Mat();
  }
  if (expression_boost.size() != wrinkle_boost.size() || expression_boost.size() != face_gate.size()) {
    std::cerr << "CombineBoostMaps: Matrix size mismatch - expression:" << expression_boost.size()
              << " wrinkle:" << wrinkle_boost.size() << " face_gate:" << face_gate.size() << std::endl;
    return cv::Mat();
  }
  if (expression_boost.type() != CV_32F || wrinkle_boost.type() != CV_32F || face_gate.type() != CV_32F) {
    std::cerr << "CombineBoostMaps: Type mismatch - expression:" << expression_boost.type()
              << " wrinkle:" << wrinkle_boost.type() << " face_gate:" << face_gate.type() << std::endl;
    return cv::Mat();
  }

  cv::Mat expression_plus_wrinkle;
  cv::add(expression_boost, wrinkle_boost, expression_plus_wrinkle);
  cv::Mat baseline_mat;
  cv::multiply(cv::Mat::ones(expression_boost.size(), CV_32F), cv::Scalar(std::max(0.0f, baseline_boost)), baseline_mat);
  cv::Mat combined;
  cv::add(expression_plus_wrinkle, baseline_mat, combined);
  cv::Mat boost_any;
  cv::min(combined, 1.0f, boost_any);
  cv::Mat boost_final;
  cv::multiply(boost_any, face_gate, boost_final);
  return boost_final;
}

void UpdateLabAndConvertBack(cv::Mat& Lf, std::vector<cv::Mat>& ch, cv::Mat& lab, cv::Mat& frame_bgr) {
  // Ensure all channels are CV_8U before merging
  Lf.convertTo(ch[0], CV_8U, 255.0);
  if (ch[1].type() != CV_8U) {
    ch[1].convertTo(ch[1], CV_8U, 255.0);
  }
  if (ch[2].type() != CV_8U) {
    ch[2].convertTo(ch[2], CV_8U, 255.0);
  }

  // Ensure all channels have the same size before merging
  cv::Size target_size = ch[0].size();
  if (ch[1].size() != target_size) {
    cv::resize(ch[1], ch[1], target_size, 0, 0, cv::INTER_LINEAR);
  }
  if (ch[2].size() != target_size) {
    cv::resize(ch[2], ch[2], target_size, 0, 0, cv::INTER_LINEAR);
  }

  cv::merge(ch, lab);
  cv::cvtColor(lab, frame_bgr, cv::COLOR_Lab2BGR);
}
cv::Mat CreateBaseFaceMask(const FaceRegions& fr, cv::Size sz) {
  cv::Mat base(sz, CV_8U, cv::Scalar(0));
  if (!fr.face_oval.empty()) {
    cv::fillPoly(base, std::vector<std::vector<cv::Point>>{fr.face_oval}, cv::Scalar(255));
  }
  if (!fr.lips_outer.empty()) {
    cv::fillPoly(base, std::vector<std::vector<cv::Point>>{fr.lips_outer}, cv::Scalar(0));
  }
  if (!fr.left_eye.empty()) {
    cv::fillPoly(base, std::vector<std::vector<cv::Point>>{fr.left_eye}, cv::Scalar(0));
  }
  if (!fr.right_eye.empty()) {
    cv::fillPoly(base, std::vector<std::vector<cv::Point>>{fr.right_eye}, cv::Scalar(0));
  }
  return base;
}

cv::Mat CreateSkinGateMask(const cv::Mat& frame_bgr) {
  cv::Mat ycrcb;
  cv::cvtColor(frame_bgr, ycrcb, cv::COLOR_BGR2YCrCb);
  std::vector<cv::Mat> yc;
  cv::split(ycrcb, yc);

  // Typical ranges (8-bit): Cr in [135, 180], Cb in [85, 135]
  cv::Mat m1, m2;
  cv::inRange(yc[1], 135, 180, m1);
  cv::inRange(yc[2], 85, 135, m2);
  cv::Mat skin;
  cv::bitwise_and(m1, m2, skin);

  // Smooth and clean small holes
  cv::GaussianBlur(skin, skin, cv::Size(0,0), 1.5);
  cv::Mat ker = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3,3));
  cv::morphologyEx(skin, skin, cv::MORPH_CLOSE, ker);
  return skin;
}

cv::Mat ComputeMultiScaleBlackHat(const cv::Mat& L8, cv::Size sz, float min_scale_px, float max_scale_px) {
  std::vector<float> scales;
  const int steps = 3;
  for (int i = 0; i < steps; ++i) {
    float s = min_scale_px + (max_scale_px - min_scale_px) * (float)i / std::max(1, steps - 1);
    scales.push_back(s);
  }

  cv::Mat acc = cv::Mat::zeros(sz, CV_32F);
  for (float s : scales) {
    int k = std::max(3, (int)std::round(s * 2) | 1);
    cv::Mat elem = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
    cv::Mat bh;
    cv::morphologyEx(L8, bh, cv::MORPH_BLACKHAT, elem);
    cv::Mat bhf;
    bh.convertTo(bhf, CV_32F, 1.0/255.0);
    // Favor smaller scales slightly (narrower lines)
    float w = 1.0f - 0.25f * (s - min_scale_px) / std::max(1e-3f, (max_scale_px - min_scale_px));
    acc = cv::max(acc, bhf * w);
  }
  return acc;
}

cv::Mat ComputeOrientationCoherence(const cv::Mat& frame_bgr, cv::Size sz) {
  cv::Mat gray;
  cv::cvtColor(frame_bgr, gray, cv::COLOR_BGR2GRAY);
  cv::Mat gx, gy;
  cv::Sobel(gray, gx, CV_32F, 1, 0, 3);
  cv::Sobel(gray, gy, CV_32F, 0, 1, 3);

  cv::Mat Jxx, Jyy, Jxy;
  cv::Mat gx_sq, gy_sq, gx_gy;
  cv::multiply(gx, gx, gx_sq);
  cv::multiply(gy, gy, gy_sq);
  cv::multiply(gx, gy, gx_gy);
  cv::GaussianBlur(gx_sq, Jxx, cv::Size(0,0), 1.5);
  cv::GaussianBlur(gy_sq, Jyy, cv::Size(0,0), 1.5);
  cv::GaussianBlur(gx_gy, Jxy, cv::Size(0,0), 1.5);

  cv::Mat tmp;
  cv::Mat diff = Jxx - Jyy;
  cv::pow(diff, 2.0, tmp);
  cv::Mat D;
  cv::Mat Jxy_sq;
  cv::multiply(Jxy, Jxy, Jxy_sq);
  cv::Mat four_Jxy_sq;
  cv::multiply(Jxy_sq, cv::Scalar(4.0), four_Jxy_sq);
  cv::sqrt(tmp + four_Jxy_sq, D);
  cv::Mat trace = Jxx + Jyy;
  cv::Mat lam1, lam2;
  cv::add(trace, D, lam1);
  cv::multiply(lam1, cv::Scalar(0.5), lam1);
  cv::subtract(trace, D, lam2);
  cv::multiply(lam2, cv::Scalar(0.5), lam2);
  cv::Mat coh_num, coh_denom;
  cv::subtract(lam1, lam2, coh_num);
  cv::add(lam1, lam2, coh_denom);
  cv::add(coh_denom, 1e-6f, coh_denom);
  cv::Mat coh;
  cv::divide(coh_num, coh_denom, coh);
  coh = cv::min(cv::max(coh, 0.0f), 1.0f);
  return coh;
}

cv::Mat ApplyRegionGates(cv::Size sz, const FaceRegions& fr, const RegionGatesConfig& config) {
  cv::Mat extra_gate = cv::Mat::ones(sz, CV_32F);

  // Optional suppress lower-face stubble region using a horizontal cut
  if (config.suppress_lower_face) {
    extra_gate = ApplyLowerFaceSuppression(sz, fr, config.lower_face_ratio);
  }

  // Optional ignore glasses: suppress a band covering both eyes with margin
  if (config.ignore_glasses) {
    ApplyGlassesSuppression(extra_gate, fr, config.glasses_margin_px);
  }

  return extra_gate;
}

cv::Mat ApplyPercentileThreshold(cv::Mat wr, const cv::Mat& base_f, const cv::Mat& skin_f,
                                const cv::Mat& extra_gate, float keep_ratio, float mask_gain) {
  // Keep only top percentile of responses inside face+skin region to make lines thin and localized.
  keep_ratio = std::clamp(keep_ratio, 0.02f, 0.5f); // 2%..50%
  cv::Mat region_mask_u8; // where to measure histogram
  {
    cv::Mat base_skin;
    cv::multiply(base_f, skin_f, base_skin);
    cv::Mat region_f;
    cv::multiply(base_skin, extra_gate, region_f);
    region_f.convertTo(region_mask_u8, CV_8U, 255.0);
  }
  if (cv::countNonZero(region_mask_u8) > 0) {
    cv::Mat wr8;
    wr.convertTo(wr8, CV_8U, 255.0);
    int histSize = 256;
    float range[] = {0, 256};
    const float* ranges = {range};
    cv::Mat hist;
    cv::calcHist(&wr8, 1, 0, region_mask_u8, hist, 1, &histSize, &ranges, true, false);
    double total = cv::sum(hist)[0];
    double target = total * keep_ratio;
    int thrBin = 255;
    double acc = 0.0;
    for (int b = 255; b >= 0; --b) {
      acc += hist.at<float>(b);
      if (acc >= target) {
        thrBin = b;
        break;
      }
    }
    double thrVal = (double)thrBin;
    cv::Mat strong;
    cv::threshold(wr8, strong, thrVal, 255, cv::THRESH_BINARY);
    // Slightly blur to make soft weights
    cv::GaussianBlur(strong, strong, cv::Size(0,0), 0.75);
    cv::Mat strong_f;
    strong.convertTo(strong_f, CV_32F, 1.0/255.0);
    cv::multiply(wr, strong_f, wr);
  }
  if (mask_gain > 1.0f) {
    cv::Mat gain_mat;
    cv::multiply(wr, cv::Scalar(mask_gain), gain_mat);
    wr = cv::min(1.0f, gain_mat);
  }
  return wr; // CV_32F [0,1]
}

// Helper functions for ApplySkinSmoothingAdvBGR
FacialExpressionMetrics ExtractFacialExpressions(const mediapipe::NormalizedLandmarkList* lms, 
                                               int width, int height,
                                               const mediapipe::ClassificationList* blendshapes) {
  FacialExpressionMetrics metrics = {
    0.0f, 0.0f,  // smile_factor, squint_factor
    {0,0}, {0,0}, // mouth_left, mouth_right
    {0,0}, {0,0}, // nose_left, nose_right
    {0,0}, {0,0}, // eye_left_outer, eye_left_inner
    {0,0}, {0,0}, // eye_right_outer, eye_right_inner
    {0,0}, {0,0}, // eye_left_top, eye_left_bottom
    {0,0}, {0,0}, // eye_right_top, eye_right_bottom
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // brow_furrow_factor, eye_squint_left, eye_squint_right, eye_blink_left, eye_blink_right
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // cheek_squint_left, cheek_squint_right, brow_inner_up, brow_outer_up_left, brow_outer_up_right
    0.0f, 0.0f, 0.0f  // mouth_pucker, mouth_left_expr, mouth_right_expr
  };

  if (!lms) return metrics;

  auto pt = [&](int idx) -> cv::Point {
    idx = std::clamp(idx, 0, lms->landmark_size()-1);
    const auto& p = lms->landmark(idx);
    return cv::Point(std::clamp((int)std::round(p.x()*width), 0, width-1),
                     std::clamp((int)std::round(p.y()*height), 0, height-1));
  };

  auto dist = [&](const cv::Point& a, const cv::Point& b) {
    return std::hypot((double)a.x-b.x, (double)a.y-b.y);
  };

  // Key points
  metrics.mouth_left = pt(61);
  metrics.mouth_right = pt(291);
  metrics.nose_left = pt(98);
  metrics.nose_right = pt(327);
  metrics.eye_left_outer = pt(33);
  metrics.eye_left_inner = pt(133);
  metrics.eye_right_outer = pt(263);
  metrics.eye_right_inner = pt(362);
  metrics.eye_left_top = pt(159);
  metrics.eye_left_bottom = pt(145);
  metrics.eye_right_top = pt(386);
  metrics.eye_right_bottom = pt(374);

  double eyeSpan = dist(metrics.eye_left_outer, metrics.eye_right_outer);
  double mouthW = dist(metrics.mouth_left, metrics.mouth_right);
  double leftW = dist(metrics.eye_left_outer, metrics.eye_left_inner);
  double rightW = dist(metrics.eye_right_outer, metrics.eye_right_inner);
  double leftH = std::abs(metrics.eye_left_top.y - metrics.eye_left_bottom.y);
  double rightH = std::abs(metrics.eye_right_top.y - metrics.eye_right_bottom.y);
  double aperture = 0.5*(leftH/ std::max(1.0,leftW) + rightH/ std::max(1.0,rightW));

  // Smile factor: ratio mapped to 0..1 (neutral ~0.35, smile ~0.55)
  double smile_ratio = (eyeSpan > 1.0) ? (mouthW/eyeSpan) : 0.0;
  metrics.smile_factor = (float)std::clamp((smile_ratio - 0.35) / 0.20, 0.0, 1.0);

  // Extract blendshape values if available
  if (blendshapes) {
    auto get_blendshape = [&](const std::string& name) -> float {
      for (int i = 0; i < blendshapes->classification_size(); ++i) {
        const auto& c = blendshapes->classification(i);
        if (c.label() == name) {
          return c.score();
        }
      }
      return 0.0f;
    };

  // Brow expressions for forehead wrinkles
  metrics.brow_furrow_factor = get_blendshape("browDownLeft") + get_blendshape("browDownRight");
  metrics.brow_inner_up = get_blendshape("browInnerUp");
  metrics.brow_outer_up_left = get_blendshape("browOuterUpLeft");
  metrics.brow_outer_up_right = get_blendshape("browOuterUpRight");

  // Eye expressions for eye wrinkles
  metrics.eye_squint_left = get_blendshape("eyeSquintLeft");
  metrics.eye_squint_right = get_blendshape("eyeSquintRight");
  metrics.eye_blink_left = get_blendshape("eyeBlinkLeft");
  metrics.eye_blink_right = get_blendshape("eyeBlinkRight");

  // Cheek expressions for nasolabial folds
  metrics.cheek_squint_left = get_blendshape("cheekSquintLeft");
  metrics.cheek_squint_right = get_blendshape("cheekSquintRight");

  // Mouth expressions for additional cheek wrinkle detection
  metrics.mouth_pucker = get_blendshape("mouthPucker");
  metrics.mouth_left_expr = get_blendshape("mouthLeft");
  metrics.mouth_right_expr = get_blendshape("mouthRight");
  metrics.mouth_upper_up_left = get_blendshape("mouthUpperUpLeft");
  metrics.mouth_upper_up_right = get_blendshape("mouthUpperUpRight");

  // Use blendshape-based smile detection (more accurate than geometric)
  // mouthUpperUpLeft and mouthUpperUpRight are strong indicators of smiling
  float mouth_upper_avg = (metrics.mouth_upper_up_left + metrics.mouth_upper_up_right) * 0.5f;
  metrics.smile_factor = std::clamp(mouth_upper_avg, 0.0f, 1.0f);
  }

  return metrics;
}

cv::Mat BuildExpressionBoostMap(const FacialExpressionMetrics& metrics, cv::Size frame_size,
                               const ExpressionBoostConfig& config, const FaceRegions& fr,
                               const cv::Mat& frame_bgr) {
  cv::Mat boost(frame_size, CV_32F, cv::Scalar(0));

  // Nasolabial boost for smile
  if (config.smile_boost > 0.0f && metrics.smile_factor > 0.0f) {
    AddNasolabialBoost(boost, metrics, config.smile_boost);
  }

  // Eye corner boost from squint
  float eff_squint = std::max(metrics.squint_factor, 0.5f * metrics.smile_factor);
  if (config.squint_boost > 0.0f && eff_squint > 0.0f) {
    AddSquintBoost(boost, metrics, config.squint_boost, eff_squint);
  }

  // Forehead boost from negative detail
  if (config.forehead_boost > 0.0f) {
    AddForeheadBoost(boost, frame_bgr, fr, metrics, config.forehead_boost, config.forehead_margin_px);
  }

  boost = cv::min(boost, 1.0f);
  return boost;
}

cv::Mat BuildWrinkleBoostMap(const cv::Mat& frame_bgr, const FaceRegions& fr,
                            const FacialExpressionMetrics& metrics, const WrinkleBoostConfig& config,
                            const cv::Mat& Lf, const cv::Mat& base) {
  // Ensure all input matrices have consistent type and size
  cv::Size target_size = frame_bgr.size();
  int target_type = CV_32F;

  // Convert and validate input matrices
  cv::Mat Lf_norm, base_norm;
  if (Lf.type() != target_type) Lf.convertTo(Lf_norm, target_type);
  else Lf_norm = Lf;
  if (base.type() != target_type) base.convertTo(base_norm, target_type);
  else base_norm = base;

  // Resize if needed
  if (Lf_norm.size() != target_size) cv::resize(Lf_norm, Lf_norm, target_size, 0, 0, cv::INTER_LINEAR);
  if (base_norm.size() != target_size) cv::resize(base_norm, base_norm, target_size, 0, 0, cv::INTER_LINEAR);

  // Build wrinkle-aware attenuation: emphasize dark, narrow, linear structures.
  // 1) Negative detail + gradient gate (local, fast)
  cv::Mat detail;
  cv::subtract(Lf_norm, base_norm, detail);
  cv::Mat gray_hint; cv::cvtColor(frame_bgr, gray_hint, cv::COLOR_BGR2GRAY);
  cv::Mat gx2, gy2; cv::Sobel(gray_hint, gx2, CV_32F, 1, 0, 3); cv::Sobel(gray_hint, gy2, CV_32F, 0, 1, 3);
  cv::Mat grad_mag; cv::magnitude(gx2, gy2, grad_mag);
  cv::GaussianBlur(grad_mag, grad_mag, cv::Size(0,0), 1.0);
  cv::Mat dark = cv::max(0.0f, -detail);
  cv::GaussianBlur(dark, dark, cv::Size(0,0), 1.0);
  cv::Mat dark_n; dark.convertTo(dark_n, CV_32F, 1.0f/0.12f); dark_n = cv::min(dark_n, 1.0f);
  double gm_mean = cv::mean(grad_mag)[0];
  float gm_scale = (float)std::max(8.0, gm_mean * 3.0 + 1e-3);
  cv::Mat grad_n; grad_mag.convertTo(grad_n, CV_32F, 1.0f / gm_scale); grad_n = cv::min(grad_n, 1.0f);
  cv::Mat wrinkle_local = cv::min(dark_n, grad_n);

  // 2) Line-sensitive mask (multi-scale black-hat + coherence)
  WrinkleMaskConfig wrinkle_config{config.line_min_px, config.line_max_px, config.region_gates.suppress_lower_face,
                                  config.region_gates.lower_face_ratio, config.region_gates.ignore_glasses,
                                  config.region_gates.glasses_margin_px, config.keep_ratio, config.use_skin_gate, config.mask_gain};
  cv::Mat wrinkle_line = BuildWrinkleLineMask(frame_bgr, fr, wrinkle_config);

  // 3) Blendshape-guided wrinkle enhancement
  cv::Mat blendshape_boost = cv::Mat::zeros(frame_bgr.size(), CV_32F);

  // Eye wrinkles: boost around eyes when squinting or blinking
  float eye_expression = std::max({metrics.eye_squint_left, metrics.eye_squint_right,
                                  metrics.eye_blink_left, metrics.eye_blink_right});
  if (eye_expression > 0.1f && !fr.left_eye.empty() && !fr.right_eye.empty()) {
    // Create circular boosts around eye regions
    cv::Rect left_eye_rect = cv::boundingRect(fr.left_eye);
    cv::Rect right_eye_rect = cv::boundingRect(fr.right_eye);

    cv::Point left_center(left_eye_rect.x + left_eye_rect.width/2, left_eye_rect.y + left_eye_rect.height/2);
    cv::Point right_center(right_eye_rect.x + right_eye_rect.width/2, right_eye_rect.y + right_eye_rect.height/2);

    int radius = std::max(left_eye_rect.width, left_eye_rect.height) * 2;
    cv::circle(blendshape_boost, left_center, radius, cv::Scalar(eye_expression * 0.8f), cv::FILLED);
    cv::circle(blendshape_boost, right_center, radius, cv::Scalar(eye_expression * 0.8f), cv::FILLED);
    cv::GaussianBlur(blendshape_boost, blendshape_boost, cv::Size(0,0), radius * 0.3f);
  }

  // Brow wrinkles: boost forehead when frowning or raising brows
  float brow_expression = std::max({metrics.brow_furrow_factor, metrics.brow_inner_up,
                                   metrics.brow_outer_up_left, metrics.brow_outer_up_right});
  if (brow_expression > 0.1f && !fr.face_oval.empty()) {
    // Create forehead region boost
    cv::Rect face_rect = cv::boundingRect(fr.face_oval);
    cv::Rect forehead_rect(face_rect.x + face_rect.width/4, face_rect.y,
                          face_rect.width/2, face_rect.height/3);
    cv::rectangle(blendshape_boost, forehead_rect, cv::Scalar(brow_expression * 0.6f), cv::FILLED);
    cv::GaussianBlur(blendshape_boost, blendshape_boost, cv::Size(0,0), forehead_rect.height * 0.2f);
  }

  // Cheek wrinkles: boost cheek area when smiling using mouth corner movements
  // Use mouthUpperUp expressions which give good values when smiling
  float cheek_expression = std::max(metrics.mouth_upper_up_left, metrics.mouth_upper_up_right);

  // Trigger cheek wrinkle inpainting when mouth corners move up (smiling)
  if (cheek_expression > 0.02f) {  // Lower threshold for mouthUpperUp expressions
    float mouth_boost = std::max({
      metrics.smile_factor,
      metrics.mouth_pucker * 0.5f,
      std::max(metrics.mouth_right_expr * 0.3f, metrics.mouth_left_expr * 0.3f)
    });
    cheek_expression = std::min(1.0f, cheek_expression + mouth_boost * 0.3f); // Reduced mouth influence
  }

  // ENHANCED CHEEK WRINKLE DETECTION: More aggressive detection for static cheek wrinkles
  // Always apply some base cheek wrinkle detection, even without strong expressions
  float base_cheek_boost = 0.8f; // Increased from 0.5f for maximum base smoothing

  // Increase sensitivity for cheek expressions and add nasolabial fold detection
  float enhanced_cheek_expression = std::max({
    cheek_expression,  // Original mouth-based detection
    metrics.cheek_squint_left * 0.8f,  // Cheek squinting
    metrics.cheek_squint_right * 0.8f, // Cheek squinting
    base_cheek_boost  // Always-on base smoothing
  });

  // Debug output for enhanced cheek detection
  static int cheek_debug_counter = 0;
  if (++cheek_debug_counter % 60 == 0) { // Log every 60 frames (~2 seconds at 30fps)
    std::cout << "[DEBUG] Enhanced cheek wrinkle detection: base=" << base_cheek_boost
              << " expression=" << cheek_expression
              << " cheek_squint_L=" << metrics.cheek_squint_left
              << " cheek_squint_R=" << metrics.cheek_squint_right
              << " enhanced=" << enhanced_cheek_expression << std::endl;
  }

  // Combine local and line masks with sensitivity: higher keep_ratio favors line mask
  float s = std::clamp(config.keep_ratio, 0.02f, 0.80f);
  float s_norm = (s - 0.02f) / (0.78f); // 0..1
  float w_line = 0.4f + 0.9f * s_norm;   // 0.4 .. 1.3
  float w_local = 0.6f * (1.0f - s_norm); // 0.6 .. 0
  float w_blendshape = 0.8f; // INCREASED weight for blendshape-guided enhancement (was 0.3f)

  // EXTREME: Increase blendshape boost weight for maximum smile suppression
  float w_blendshape_extreme = 3.0f; // INCREASED for maximum cheek wrinkle detection (was 2.0f)
  // Ensure all masks are CV_32F and same size
  cv::Mat wrinkle_line_f, wrinkle_local_f, blendshape_boost_f;
  if (wrinkle_line.type() != CV_32F) wrinkle_line.convertTo(wrinkle_line_f, CV_32F);
  else wrinkle_line_f = wrinkle_line;
  if (wrinkle_local.type() != CV_32F) wrinkle_local.convertTo(wrinkle_local_f, CV_32F);
  else wrinkle_local_f = wrinkle_local;
  if (blendshape_boost.type() != CV_32F) blendshape_boost.convertTo(blendshape_boost_f, CV_32F);
  else blendshape_boost_f = blendshape_boost;
  // Resize if needed
  if (wrinkle_line_f.size() != frame_bgr.size()) cv::resize(wrinkle_line_f, wrinkle_line_f, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
  if (wrinkle_local_f.size() != frame_bgr.size()) cv::resize(wrinkle_local_f, wrinkle_local_f, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
  if (blendshape_boost_f.size() != frame_bgr.size()) cv::resize(blendshape_boost_f, blendshape_boost_f, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);

  // Type-safe combination of wrinkle masks
  cv::Mat wrinkle_line_weighted, wrinkle_local_weighted, blendshape_weighted;
  cv::multiply(wrinkle_line_f, w_line, wrinkle_line_weighted);
  cv::multiply(wrinkle_local_f, w_local, wrinkle_local_weighted);
  cv::multiply(blendshape_boost_f, w_blendshape_extreme, blendshape_weighted);

  cv::Mat combined;
  cv::add(wrinkle_line_weighted, wrinkle_local_weighted, combined);
  cv::add(combined, blendshape_weighted, combined);

  cv::Mat wrinkle_mask;
  cv::min(combined, 1.0f, wrinkle_mask);

  return wrinkle_mask;
  
}



void ApplyFrequencySeparation(cv::Mat& Lf, const cv::Mat& weight, float amount, float boost_gain,
                             const cv::Mat& boost_final, bool wrinkle_preview, float neg_atten_cap) {
  // Validate input matrices
  if (Lf.empty() || weight.empty() || boost_final.empty()) {
    std::cerr << "ApplyFrequencySeparation: Empty input matrices" << std::endl;
    return;
  }
  if (Lf.size() != weight.size() || Lf.size() != boost_final.size()) {
    std::cerr << "ApplyFrequencySeparation: Matrix size mismatch - Lf:" << Lf.size()
              << " weight:" << weight.size() << " boost_final:" << boost_final.size() << std::endl;
    return;
  }
  if (Lf.type() != CV_32F || weight.type() != CV_32F || boost_final.type() != CV_32F) {
    std::cerr << "ApplyFrequencySeparation: Type mismatch - Lf:" << Lf.type()
              << " weight:" << weight.type() << " boost_final:" << boost_final.type() << std::endl;
    return;
  }

  // Create low-frequency base via Gaussian blur
  cv::Mat base_blur;
  cv::GaussianBlur(Lf, base_blur, cv::Size(0, 0), 3.0); // 3px sigma for low-frequency base

  // In wrinkle preview mode, show only wrinkle attenuation, no base smoothing
  if (wrinkle_preview) {
    base_blur = Lf; // no low-pass filtering in preview mode
  }

  // Frequency separation handling that preserves highlights/pores:
  // Split detail into positive (highlights/pores) and negative (shadows/wrinkles).
  cv::Mat detail;
  cv::subtract(Lf, base_blur, detail);
  cv::Mat detail_pos = cv::max(detail, 0.0f);
  cv::Mat detail_neg = cv::min(detail, 0.0f);

  // Attenuate detail by amount * weight, with optional wrinkle-aware boost
  cv::Mat atten;
  cv::multiply(weight, cv::Scalar(amount), atten); // CV_32F (base smoothing)
  if (wrinkle_preview) {
    // In preview: show only wrinkle attenuation, no base smoothing
    cv::multiply(boost_final, cv::Scalar(boost_gain), atten);
    cv::min(atten, 1.0f, atten);
  } else {
    cv::Mat boost_contrib;
    cv::multiply(boost_final, cv::Scalar(boost_gain), boost_contrib);
    cv::add(atten, boost_contrib, atten);
    cv::min(atten, 1.0f, atten);
  }

  // Use lighter attenuation for positive detail to keep sheen/pores.
  cv::Mat pos_atten;
  if (wrinkle_preview) {
    pos_atten = cv::Mat::zeros(atten.size(), CV_32F);
  } else {
    cv::multiply(weight, cv::Scalar(amount * 0.15f), pos_atten);
  }
  // Stronger attenuation on negative detail (wrinkle shadows), clamped to user cap
  float cap = std::clamp(neg_atten_cap, 0.4f, 1.0f);
  cv::Mat neg_atten;
  cv::min(atten, cap, neg_atten);

  // Type-safe combination: base + attenuated positive detail + attenuated negative detail
  cv::Mat pos_contrib, neg_contrib;
  cv::Mat pos_atten_inv, neg_atten_inv;
  cv::subtract(1.0f, pos_atten, pos_atten_inv);
  cv::subtract(1.0f, neg_atten, neg_atten_inv);
  cv::multiply(detail_pos, pos_atten_inv, pos_contrib);
  cv::multiply(detail_neg, neg_atten_inv, neg_contrib);
  
  cv::Mat outL;
  cv::add(base_blur, pos_contrib, outL);
  cv::add(outL, neg_contrib, outL);
  cv::min(outL, 1.0f, outL);
  cv::max(outL, 0.0f, outL);
  outL.copyTo(Lf);
}

cv::Mat BuildSkinWeightMap(const FaceRegions& fr,
                           const cv::Size& frame_size,
                           float edge_feather_px,
                           float texture_thresh,
                           const cv::Mat& hint_bgr,
                           const FacialExpressionMetrics* expr_metrics) {
  cv::Mat base(frame_size, CV_8UC1, cv::Scalar(0));
  
  // Start with the original face oval
  std::vector<cv::Point> expanded_face_oval = fr.face_oval;
  
  // Expand face oval for cheek coverage when smiling
  if (expr_metrics && !fr.face_oval.empty()) {
    float smile_intensity = std::max({
      expr_metrics->smile_factor,
      expr_metrics->mouth_upper_up_left,
      expr_metrics->mouth_upper_up_right,
      expr_metrics->mouth_pucker * 0.5f
    });
    
    if (smile_intensity > 0.05f) {  // Lower threshold for smile detection
      // Calculate face center and dimensions
      cv::Rect face_bounds = cv::boundingRect(fr.face_oval);
      cv::Point center(face_bounds.x + face_bounds.width/2, face_bounds.y + face_bounds.height/2);
      
      // Very aggressive expansion for comprehensive cheek coverage (50-90%)
      float cheek_expansion = 0.50f + smile_intensity * 0.40f;  // 50-90% expansion
      int expand_x = static_cast<int>(face_bounds.width * cheek_expansion);
      int expand_y = static_cast<int>(face_bounds.height * 0.20f);  // Expand vertically by 20%
      
      // Create expanded face oval by offsetting cheek landmarks outward
      expanded_face_oval = fr.face_oval;
      
      // Find cheek landmarks - be extremely inclusive (threshold from 0.25f to 0.05f)
      // This will expand almost all points including central areas for comprehensive coverage
      for (auto& pt : expanded_face_oval) {
        float rel_x = (pt.x - center.x) / static_cast<float>(face_bounds.width);
        float rel_y = (pt.y - center.y) / static_cast<float>(face_bounds.height);
        if (std::abs(rel_x) > 0.05f) {  // Extremely inclusive side detection
          pt.x += static_cast<int>(rel_x > 0 ? expand_x : -expand_x);
        }
        // Expand vertically for lower face and cheek areas
        if (rel_y > 0.0f) {  // Points at or below face center
          pt.y += expand_y;
        }
      }
      
      // Recompute convex hull after expansion
      std::vector<int> hull_idx;
      cv::convexHull(expanded_face_oval, hull_idx, false, false);
      std::vector<cv::Point> hull; hull.reserve(hull_idx.size());
      for (int i : hull_idx) hull.push_back(expanded_face_oval[i]);
      expanded_face_oval = std::move(hull);
    }
  }
  
  if (!expanded_face_oval.empty()) {
    cv::fillPoly(base, std::vector<std::vector<cv::Point>>{expanded_face_oval}, cv::Scalar(255));
  }
  if (!fr.lips_outer.empty()) cv::fillPoly(base, std::vector<std::vector<cv::Point>>{fr.lips_outer}, cv::Scalar(0));
  if (!fr.left_eye.empty())   cv::fillPoly(base, std::vector<std::vector<cv::Point>>{fr.left_eye},   cv::Scalar(0));
  if (!fr.right_eye.empty())  cv::fillPoly(base, std::vector<std::vector<cv::Point>>{fr.right_eye},  cv::Scalar(0));

  // Edge feather via distance transform: inside distances, normalized by edge_feather_px
  cv::Mat dist;
  cv::distanceTransform(base, dist, cv::DIST_L2, 3);
  float ef = std::max(1.0f, edge_feather_px);
  cv::Mat weight_edge; dist.convertTo(weight_edge, CV_32FC1, 1.0f / ef);
  cv::threshold(weight_edge, weight_edge, 1.0, 1.0, cv::THRESH_TRUNC);
  // Zero out weights where outside face
  cv::Mat base_f; base.convertTo(base_f, CV_32FC1, 1.0/255.0);
  cv::multiply(weight_edge, base_f, weight_edge);

  // Texture-aware suppression: compute gradient magnitude on hint or base image
  cv::Mat gray;
  if (!hint_bgr.empty()) {
    cv::cvtColor(hint_bgr, gray, cv::COLOR_BGR2GRAY);
  } else {
    gray = cv::Mat(frame_size, CV_8UC1, cv::Scalar(0));
  }
  cv::Mat gx, gy; if (!gray.empty()) {
    cv::Sobel(gray, gx, CV_32F, 1, 0, 3);
    cv::Sobel(gray, gy, CV_32F, 0, 1, 3);
  } else {
    gx = cv::Mat::zeros(frame_size, CV_32F);
    gy = cv::Mat::zeros(frame_size, CV_32F);
  }
  cv::Mat mag; cv::magnitude(gx, gy, mag);
  // Smooth gradients to avoid salt-and-pepper
  cv::GaussianBlur(mag, mag, cv::Size(0,0), 1.0);
  // Robust normalization based on mean magnitude; if gradients are tiny,
  // fall back to a gentle scale so weights don't collapse.
  cv::Scalar meanMag = cv::mean(mag, base_f > 0.0f);
  float scale = (float)std::max(8.0, meanMag[0] * 3.0 + 1e-3); // pixels-intensity heuristic
  cv::Mat mag_n; mag.convertTo(mag_n, CV_32F, 1.0f / scale);
  // Texture keep in 0..1: higher keeps more texture. Map to attenuation factor.
  float t = std::clamp(texture_thresh, 0.01f, 1.0f);
  // wtex decays smoothly with gradient; avoid zeroing out completely.
  cv::Mat mag_n_div_t, denominator, wtex;
  cv::divide(mag_n, t, mag_n_div_t);
  cv::add(1.0f, mag_n_div_t, denominator);
  cv::divide(1.0f, denominator, wtex);
  wtex = cv::min(wtex, 1.0f);

  cv::Mat weight;
  cv::multiply(weight_edge, wtex, weight);
  // Higher baseline weight for cheek areas (increased from 0.6f to 1.0f for maximum smoothing)
  cv::Mat baseline_weight;
  cv::multiply(cv::Scalar(1.0f), base_f, baseline_weight);
  weight = cv::max(weight, baseline_weight);
  // If still extremely low on average, drop texture suppression entirely
  if (cv::mean(weight)[0] < 0.02) weight = weight_edge;
  return weight; // CV_32F in [0,1]
}

cv::Mat BuildWrinkleLineMask(const cv::Mat& frame_bgr, const FaceRegions& fr, const WrinkleMaskConfig& config) {
  cv::Size sz = frame_bgr.size();
  float min_scale_px = std::max(1.0f, config.min_scale_px);
  float max_scale_px = std::max(min_scale_px, config.max_scale_px);

  // Create base components
  cv::Mat base = CreateBaseFaceMask(fr, sz);
  cv::Mat skin = CreateSkinGateMask(frame_bgr);
  cv::Mat L8 = PrepareWrinkleDetectionData(frame_bgr, sz);

  // Multi-scale black-hat and coherence
  cv::Mat acc = ComputeMultiScaleBlackHat(L8, sz, min_scale_px, max_scale_px);
  cv::Mat coh = ComputeOrientationCoherence(frame_bgr, sz);
  RegionGatesConfig region_config{config.suppress_lower_face, config.lower_face_ratio,
                                 config.ignore_glasses, config.glasses_margin_px};
  cv::Mat extra_gate = ApplyRegionGates(sz, fr, region_config);

  // Combine components
  cv::Mat base_f, skin_f;
  base.convertTo(base_f, CV_32F, 1.0/255.0);
  if (config.use_skin_gate) {
    skin.convertTo(skin_f, CV_32F, 1.0/255.0);
  } else {
    skin_f = cv::Mat(base_f.size(), CV_32F, cv::Scalar(1.0f));
  }

  cv::Mat wr = CombineWrinkleComponents(acc, coh, base_f, skin_f, extra_gate);

  // Apply percentile threshold
  return ApplyPercentileThreshold(wr, base_f, skin_f, extra_gate, config.keep_ratio, config.mask_gain);
}


// SNAPCHAT-STYLE MULTI-LAYER SKIN SMOOTHING FUNCTIONS

// Create a more refined face mask that better excludes eyes, eyebrows, and mouth
cv::Mat CreateRefinedFaceMask(const FaceRegions& fr, cv::Size sz, const cv::Mat& base_weight) {
  cv::Mat mask = cv::Mat::zeros(sz, CV_32F);

  // Start with base face oval
  if (!fr.face_oval.empty()) {
    std::vector<std::vector<cv::Point>> contours = {fr.face_oval};
    cv::fillPoly(mask, contours, cv::Scalar(1.0f));
  }

  // Exclude eyes with larger margins (eyebrows are typically above eyes)
  if (!fr.left_eye.empty()) {
    cv::Rect eye_rect = cv::boundingRect(fr.left_eye);
    eye_rect.x -= eye_rect.width * 0.5f;
    eye_rect.y -= eye_rect.height * 1.0f;  // Extend upward for eyebrows
    eye_rect.width *= 2.0f;
    eye_rect.height *= 2.5f;  // Extend height for eyebrows
    cv::rectangle(mask, eye_rect, cv::Scalar(0.0f), cv::FILLED);
  }

  if (!fr.right_eye.empty()) {
    cv::Rect eye_rect = cv::boundingRect(fr.right_eye);
    eye_rect.x -= eye_rect.width * 0.5f;
    eye_rect.y -= eye_rect.height * 1.0f;  // Extend upward for eyebrows
    eye_rect.width *= 2.0f;
    eye_rect.height *= 2.5f;  // Extend height for eyebrows
    cv::rectangle(mask, eye_rect, cv::Scalar(0.0f), cv::FILLED);
  }

  // Exclude mouth/lips with margin
  if (!fr.lips_outer.empty()) {
    cv::Rect mouth_rect = cv::boundingRect(fr.lips_outer);
    mouth_rect.x -= mouth_rect.width * 0.3f;
    mouth_rect.y -= mouth_rect.height * 0.3f;
    mouth_rect.width *= 1.6f;
    mouth_rect.height *= 1.6f;
    cv::rectangle(mask, mouth_rect, cv::Scalar(0.0f), cv::FILLED);
  }

  // Feather edges for smooth blending
  cv::GaussianBlur(mask, mask, cv::Size(0,0), 3.0);

  // Combine with base weight for better skin detection
  if (!base_weight.empty()) {
    cv::Mat weight_f;
    if (base_weight.type() == CV_8U) {
      base_weight.convertTo(weight_f, CV_32F, 1.0f/255.0f);
    } else if (base_weight.type() != CV_32F) {
      base_weight.convertTo(weight_f, CV_32F);
    } else {
      weight_f = base_weight;
    }

    // Ensure both matrices have the same size
    if (weight_f.size() != mask.size()) {
      cv::resize(weight_f, weight_f, mask.size(), 0, 0, cv::INTER_LINEAR);
    }

    cv::Mat result;
    cv::min(mask, weight_f, result);
    mask = result;
  }

  return mask;
}

// Apply targeted bilateral filtering only to skin areas (not wrinkles)
void ApplyTargetedBilateralFilter(cv::Mat& frame_bgr, const cv::Mat& skin_mask, float amount) {
  if (amount <= 0.0f) return;

  // Bilateral filter works better with smaller radius and higher sigma
  float bilateral_radius = 3.0f + amount * 4.0f;  // 3-7 pixels
  float sigma_color = 20.0f + amount * 30.0f;     // 20-50
  float sigma_space = bilateral_radius;

  cv::Mat filtered;
  cv::bilateralFilter(frame_bgr, filtered, (int)bilateral_radius, sigma_color, sigma_space);

  // Blend based on skin mask - only apply smoothing where there's skin (not wrinkles)
  // Ensure skin_mask is CV_32F and same size as frame_bgr
  cv::Mat skin_mask_f;
  if (skin_mask.type() != CV_32F) {
    skin_mask.convertTo(skin_mask_f, CV_32F, 1.0/255.0);
  } else {
    skin_mask_f = skin_mask;
  }

  // Ensure skin_mask_f has the same size as frame_bgr
  if (skin_mask_f.size() != frame_bgr.size()) {
    cv::resize(skin_mask_f, skin_mask_f, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
  }

  // Create 3-channel masks for proper blending with BGR image
  std::vector<cv::Mat> mask_channels = {skin_mask_f, skin_mask_f, skin_mask_f};
  cv::Mat skin_mask_3ch;
  cv::merge(mask_channels, skin_mask_3ch);

  // Ensure both matrices are CV_32FC3 before subtraction
  cv::Mat ones_3ch = cv::Mat::ones(skin_mask_3ch.size(), CV_32FC3);
  cv::Mat inv_mask_3ch;
  cv::subtract(ones_3ch, skin_mask_3ch, inv_mask_3ch);

  // Convert to float for proper arithmetic
  cv::Mat frame_float, filtered_float;
  frame_bgr.convertTo(frame_float, CV_32F, 1.0/255.0);
  filtered.convertTo(filtered_float, CV_32F, 1.0/255.0);

  // Blend: frame * (1 - mask) + filtered * mask
  cv::Mat frame_weighted, filtered_weighted, blended_float;
  cv::multiply(frame_float, inv_mask_3ch, frame_weighted);
  cv::multiply(filtered_float, skin_mask_3ch, filtered_weighted);
  cv::add(frame_weighted, filtered_weighted, blended_float);

  // Convert back to uint8
  blended_float.convertTo(frame_bgr, CV_8U, 255.0);
}

// Apply subtle skin tone enhancement for more natural look
void ApplySkinToneEnhancement(cv::Mat& frame_bgr, const cv::Mat& skin_mask, float amount) {
  if (amount <= 0.0f) return;

  cv::Mat lab;
  cv::cvtColor(frame_bgr, lab, cv::COLOR_BGR2Lab);
  std::vector<cv::Mat> channels;
  cv::split(lab, channels);

  // Enhance luminance (L channel) slightly for brighter, more even skin
  cv::Mat l_diff, l_boost;
  cv::subtract(channels[0], 128.0, l_diff);
  cv::multiply(l_diff, cv::Scalar(amount * 0.1f), l_boost);
  cv::Mat l_enhanced;
  cv::add(channels[0], l_boost, l_enhanced);
  cv::min(l_enhanced, 255.0, l_enhanced);
  cv::max(l_enhanced, 0.0, l_enhanced);

  // Subtle color enhancement (A and B channels)
  float color_boost = amount * 0.05f;
  cv::Mat a_enhanced, b_enhanced;
  cv::multiply(channels[1], cv::Scalar(1.0f + color_boost), a_enhanced);
  cv::multiply(channels[2], cv::Scalar(1.0f + color_boost), b_enhanced);

  // Apply only to skin areas - ensure proper type handling
  cv::Mat skin_mask_f;
  if (skin_mask.type() != CV_32F) {
    skin_mask.convertTo(skin_mask_f, CV_32F, 1.0/255.0);
  } else {
    skin_mask_f = skin_mask;
  }

  // Ensure skin_mask_f has the same size as the frame
  if (skin_mask_f.size() != frame_bgr.size()) {
    cv::resize(skin_mask_f, skin_mask_f, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
  }

  cv::Mat ones = cv::Mat::ones(skin_mask_f.size(), CV_32F);
  cv::Mat inv_mask;
  cv::subtract(ones, skin_mask_f, inv_mask);

  // Ensure all channels are CV_8U before operations
  cv::Mat l_orig, a_orig, b_orig;
  channels[0].convertTo(l_orig, CV_8U);
  channels[1].convertTo(a_orig, CV_8U);
  channels[2].convertTo(b_orig, CV_8U);

  cv::Mat l_result, a_result, b_result;
  cv::Mat l_orig_f, a_orig_f, b_orig_f, l_enhanced_f, a_enhanced_f, b_enhanced_f;
  
  // Convert to float for consistent operations
  l_orig.convertTo(l_orig_f, CV_32F, 1.0/255.0);
  a_orig.convertTo(a_orig_f, CV_32F, 1.0/255.0);
  b_orig.convertTo(b_orig_f, CV_32F, 1.0/255.0);
  l_enhanced.convertTo(l_enhanced_f, CV_32F, 1.0/255.0);
  a_enhanced.convertTo(a_enhanced_f, CV_32F, 1.0/255.0);
  b_enhanced.convertTo(b_enhanced_f, CV_32F, 1.0/255.0);

  cv::Mat l_orig_mul_inv, l_enhanced_mul_skin;
  cv::multiply(l_orig_f, inv_mask, l_orig_mul_inv);
  cv::multiply(l_enhanced_f, skin_mask_f, l_enhanced_mul_skin);
  cv::add(l_orig_mul_inv, l_enhanced_mul_skin, l_result);

  cv::Mat a_orig_mul_inv, a_enhanced_mul_skin;
  cv::multiply(a_orig_f, inv_mask, a_orig_mul_inv);
  cv::multiply(a_enhanced_f, skin_mask_f, a_enhanced_mul_skin);
  cv::add(a_orig_mul_inv, a_enhanced_mul_skin, a_result);

  cv::Mat b_orig_mul_inv, b_enhanced_mul_skin;
  cv::multiply(b_orig_f, inv_mask, b_orig_mul_inv);
  cv::multiply(b_enhanced_f, skin_mask_f, b_enhanced_mul_skin);
  cv::add(b_orig_mul_inv, b_enhanced_mul_skin, b_result);

  cv::multiply(l_result, cv::Scalar(255.0), channels[0]);
  cv::multiply(a_result, cv::Scalar(255.0), channels[1]);
  cv::multiply(b_result, cv::Scalar(255.0), channels[2]);

  // Ensure all channels have the same size before merging
  cv::Size target_size = lab.size();
  for (size_t i = 0; i < channels.size(); ++i) {
    if (channels[i].size() != target_size) {
      cv::resize(channels[i], channels[i], target_size, 0, 0, cv::INTER_LINEAR);
    }
  }

  cv::merge(channels, lab);
  cv::cvtColor(lab, frame_bgr, cv::COLOR_Lab2BGR);
}

// Apply multi-frequency texture preservation
void ApplyMultiFrequencyTexturePreservation(cv::Mat& frame_bgr, const cv::Mat& skin_mask,
                                           const cv::Mat& wrinkle_mask, float amount) {
  if (amount <= 0.0f) return;

  // Convert to LAB for better color handling
  cv::Mat lab;
  cv::cvtColor(frame_bgr, lab, cv::COLOR_BGR2Lab);
  std::vector<cv::Mat> channels;
  cv::split(lab, channels);

  // Work on L channel for texture preservation
  cv::Mat L = channels[0];

  // Create multiple frequency bands
  cv::Mat low_freq1, low_freq2;

  // Large scale smoothing (removes major blemishes)
  cv::GaussianBlur(L, low_freq1, cv::Size(15, 15), 0);
  // Medium scale smoothing (removes medium wrinkles)
  cv::GaussianBlur(L, low_freq2, cv::Size(7, 7), 0);

  // Extract high frequency details
  cv::Mat high_freq1, high_freq2;
  cv::subtract(L, low_freq1, high_freq1);
  cv::subtract(L, low_freq2, high_freq2);

  // Preserve texture in wrinkle areas, smooth in non-wrinkle areas
  cv::Mat wrinkle_mask_f, skin_mask_f;
  if (wrinkle_mask.type() != CV_32F) {
    wrinkle_mask.convertTo(wrinkle_mask_f, CV_32F, 1.0/255.0);
  } else {
    wrinkle_mask_f = wrinkle_mask;
  }
  if (skin_mask.type() != CV_32F) {
    skin_mask.convertTo(skin_mask_f, CV_32F, 1.0/255.0);
  } else {
    skin_mask_f = skin_mask;
  }

  cv::Mat ones = cv::Mat::ones(wrinkle_mask_f.size(), CV_32F);
  cv::Mat inv_wrinkle_mask;
  cv::subtract(ones, wrinkle_mask_f, inv_wrinkle_mask);

  cv::Mat preserved_texture;
  cv::Mat high_freq1_mul_wrinkle, high_freq2_mul_inv;
  cv::multiply(high_freq1, wrinkle_mask_f, high_freq1_mul_wrinkle);
  cv::multiply(high_freq2, inv_wrinkle_mask, high_freq2_mul_inv);
  cv::add(high_freq1_mul_wrinkle, high_freq2_mul_inv, preserved_texture);

  // Reconstruct with controlled smoothing
  cv::Mat texture_weight;
  cv::multiply(preserved_texture, cv::Scalar(0.3f + amount * 0.4f), texture_weight);
  cv::Mat smoothed;
  cv::add(low_freq2, texture_weight, smoothed);

  // Apply only to skin areas
  cv::Mat ones_skin = cv::Mat::ones(skin_mask_f.size(), CV_32F);
  cv::Mat inv_mask;
  cv::subtract(ones_skin, skin_mask_f, inv_mask);

  cv::Mat skin_contrib, non_skin_contrib, result;
  cv::multiply(channels[0], inv_mask, non_skin_contrib);
  cv::multiply(smoothed, skin_mask_f, skin_contrib);
  cv::add(non_skin_contrib, skin_contrib, result);
  channels[0] = result;

  // Ensure all channels have the same size before merging
  cv::Size target_size = lab.size();
  for (size_t i = 0; i < channels.size(); ++i) {
    if (channels[i].size() != target_size) {
      cv::resize(channels[i], channels[i], target_size, 0, 0, cv::INTER_LINEAR);
    }
  }

  cv::merge(channels, lab);
  cv::cvtColor(lab, frame_bgr, cv::COLOR_Lab2BGR);
}

// Luminance-based versions for Snapchat-style processing
void ApplyTargetedBilateralFilterToLuminance(cv::Mat& Lf, const cv::Mat& skin_mask, float amount) {
  if (amount <= 0.0f) return;

  // Bilateral filter works better with smaller radius and higher sigma
  float bilateral_radius = 3.0f + amount * 4.0f;  // 3-7 pixels
  float sigma_color = 20.0f + amount * 30.0f;     // 20-50
  float sigma_space = bilateral_radius;

  cv::Mat filtered;
  cv::bilateralFilter(Lf, filtered, (int)bilateral_radius, sigma_color, sigma_space);

  // Blend based on skin mask - only apply smoothing where there's skin (not wrinkles)
  cv::Mat skin_mask_f;
  if (skin_mask.type() != CV_32F) {
    skin_mask.convertTo(skin_mask_f, CV_32F, 1.0/255.0);
  } else {
    skin_mask_f = skin_mask;
  }

  // Ensure skin_mask_f has the same size as Lf
  if (skin_mask_f.size() != Lf.size()) {
    cv::resize(skin_mask_f, skin_mask_f, Lf.size(), 0, 0, cv::INTER_LINEAR);
  }

  cv::Mat ones = cv::Mat::ones(skin_mask_f.size(), CV_32F);
  cv::Mat inv_mask;
  cv::subtract(ones, skin_mask_f, inv_mask);

  // Blend: original * (1 - mask) + filtered * mask
  cv::Mat orig_weighted, filtered_weighted;
  cv::multiply(Lf, inv_mask, orig_weighted);
  cv::multiply(filtered, skin_mask_f, filtered_weighted);
  cv::add(orig_weighted, filtered_weighted, Lf);
}

void ApplySkinToneEnhancementToLuminance(cv::Mat& Lf, const cv::Mat& skin_mask, float amount) {
  if (amount <= 0.0f) return;

  // Enhance luminance slightly for brighter, more even skin
  cv::Mat l_diff, l_boost;
  cv::subtract(Lf, 128.0, l_diff);
  cv::multiply(l_diff, cv::Scalar(amount * 0.1f), l_boost);
  cv::Mat l_enhanced;
  cv::add(Lf, l_boost, l_enhanced);
  cv::min(l_enhanced, 255.0, l_enhanced);
  cv::max(l_enhanced, 0.0, l_enhanced);

  // Apply only to skin areas
  cv::Mat skin_mask_f;
  if (skin_mask.type() != CV_32F) {
    skin_mask.convertTo(skin_mask_f, CV_32F, 1.0/255.0);
  } else {
    skin_mask_f = skin_mask;
  }

  // Ensure skin_mask_f has the same size as Lf
  if (skin_mask_f.size() != Lf.size()) {
    cv::resize(skin_mask_f, skin_mask_f, Lf.size(), 0, 0, cv::INTER_LINEAR);
  }

  cv::Mat ones = cv::Mat::ones(skin_mask_f.size(), CV_32F);
  cv::Mat inv_mask;
  cv::subtract(ones, skin_mask_f, inv_mask);

  cv::Mat orig_weighted, enhanced_weighted;
  cv::multiply(Lf, inv_mask, orig_weighted);
  cv::multiply(l_enhanced, skin_mask_f, enhanced_weighted);
  cv::add(orig_weighted, enhanced_weighted, Lf);
}

void ApplyMultiFrequencyTexturePreservationToLuminance(cv::Mat& Lf, const cv::Mat& skin_mask,
                                                       const cv::Mat& wrinkle_mask, float amount) {
  if (amount <= 0.0f) return;

  // Create frequency decomposition
  cv::Mat low_freq1, high_freq1;
  cv::GaussianBlur(Lf, low_freq1, cv::Size(5, 5), 0);
  cv::subtract(Lf, low_freq1, high_freq1);

  cv::Mat low_freq2, high_freq2;
  cv::GaussianBlur(low_freq1, low_freq2, cv::Size(15, 15), 0);
  cv::subtract(low_freq1, low_freq2, high_freq2);

  // Prepare masks
  cv::Mat wrinkle_mask_f, skin_mask_f;
  if (wrinkle_mask.type() != CV_32F) {
    wrinkle_mask.convertTo(wrinkle_mask_f, CV_32F, 1.0/255.0);
  } else {
    wrinkle_mask_f = wrinkle_mask;
  }
  if (skin_mask.type() != CV_32F) {
    skin_mask.convertTo(skin_mask_f, CV_32F, 1.0/255.0);
  } else {
    skin_mask_f = skin_mask;
  }

  // Ensure masks have the same size as Lf
  if (wrinkle_mask_f.size() != Lf.size()) {
    cv::resize(wrinkle_mask_f, wrinkle_mask_f, Lf.size(), 0, 0, cv::INTER_LINEAR);
  }
  if (skin_mask_f.size() != Lf.size()) {
    cv::resize(skin_mask_f, skin_mask_f, Lf.size(), 0, 0, cv::INTER_LINEAR);
  }

  cv::Mat ones = cv::Mat::ones(wrinkle_mask_f.size(), CV_32F);
  cv::Mat inv_wrinkle_mask;
  cv::subtract(ones, wrinkle_mask_f, inv_wrinkle_mask);

  cv::Mat preserved_texture;
  cv::Mat high_freq1_mul_wrinkle, high_freq2_mul_inv;
  cv::multiply(high_freq1, wrinkle_mask_f, high_freq1_mul_wrinkle);
  cv::multiply(high_freq2, inv_wrinkle_mask, high_freq2_mul_inv);
  cv::add(high_freq1_mul_wrinkle, high_freq2_mul_inv, preserved_texture);

  // Reconstruct with controlled smoothing
  cv::Mat texture_weight;
  cv::multiply(preserved_texture, cv::Scalar(0.3f + amount * 0.4f), texture_weight);
  cv::Mat smoothed;
  cv::add(low_freq2, texture_weight, smoothed);

  // Apply only to skin areas
  cv::Mat ones_skin = cv::Mat::ones(skin_mask_f.size(), CV_32F);
  cv::Mat inv_mask;
  cv::subtract(ones_skin, skin_mask_f, inv_mask);

  cv::Mat skin_contrib, non_skin_contrib;
  cv::multiply(Lf, inv_mask, non_skin_contrib);
  cv::multiply(smoothed, skin_mask_f, skin_contrib);
  cv::add(non_skin_contrib, skin_contrib, Lf);
}

// Main Snapchat-style multi-layer smoothing function
void ApplySnapchatStyleSmoothing(cv::Mat& frame_bgr, const cv::Mat& refined_face_mask,
                                const FaceRegions& fr, const SkinSmoothingConfig& config,
                                const FacialExpressionMetrics& metrics, cv::Mat& Lf, cv::Mat& base,
                                const cv::Mat& weight) {

  float amount = std::clamp(config.amount, 0.0f, 1.0f);

  // Validate input matrices
  if (refined_face_mask.empty() || Lf.empty() || base.empty() || weight.empty()) {
    std::cerr << "ApplySnapchatStyleSmoothing: Empty input matrices" << std::endl;
    return;
  }
  if (refined_face_mask.size() != frame_bgr.size() || Lf.size() != frame_bgr.size() || 
      base.size() != frame_bgr.size() || weight.size() != frame_bgr.size()) {
    std::cerr << "ApplySnapchatStyleSmoothing: Size mismatch - frame:" << frame_bgr.size()
              << " mask:" << refined_face_mask.size() << " Lf:" << Lf.size() 
              << " base:" << base.size() << " weight:" << weight.size() << std::endl;
    return;
  }
  if (refined_face_mask.type() != CV_32F || Lf.type() != CV_32F || 
      base.type() != CV_32F || weight.type() != CV_32F) {
    std::cerr << "ApplySnapchatStyleSmoothing: Type mismatch - mask:" << refined_face_mask.type()
              << " Lf:" << Lf.type() << " base:" << base.type() << " weight:" << weight.type() << std::endl;
    return;
  }

  // Debug: Check if frame is being modified
  cv::Mat frame_before = frame_bgr.clone();
  uint64_t checksum_before = 0;
  for (int i = 0; i < frame_before.total() * frame_before.channels(); ++i) {
    checksum_before += ((uint8_t*)frame_before.data)[i];
  }

  // Layer 1: Targeted bilateral filtering for general skin smoothing
  // This provides the "polished" look Snapchat is known for
  ApplyTargetedBilateralFilterToLuminance(Lf, refined_face_mask, amount * 0.6f);

  // Layer 2: Skin tone enhancement for more natural, even complexion
  ApplySkinToneEnhancementToLuminance(Lf, refined_face_mask, amount * 0.3f);

  // Layer 3: Multi-frequency texture preservation
  // Build wrinkle mask for texture preservation
  cv::Mat wrinkle_mask = cv::Mat::zeros(frame_bgr.size(), CV_32F);
  if (config.wrinkle_enabled) {
    WrinkleMaskConfig wrinkle_config{config.wrinkle.line_min_px * 0.4f, config.wrinkle.line_max_px * 2.0f,  // Very inclusive range for cheek wrinkles
                                    false,  // Don't suppress lower face for cheek wrinkles
                                    config.wrinkle.region_gates.lower_face_ratio,
                                    config.wrinkle.region_gates.ignore_glasses,
                                    config.wrinkle.region_gates.glasses_margin_px,
                                    config.wrinkle.keep_ratio * 0.1f, config.wrinkle.use_skin_gate, config.wrinkle.mask_gain * 0.4f};  // Maximum suppression reduction for cheek areas
    wrinkle_mask = BuildWrinkleLineMask(frame_bgr, fr, wrinkle_config);
    if (wrinkle_mask.empty() || wrinkle_mask.size() != frame_bgr.size() || wrinkle_mask.type() != CV_32F) {
      std::cerr << "ApplySnapchatStyleSmoothing: Invalid wrinkle_mask - empty:" << wrinkle_mask.empty()
                << " size:" << wrinkle_mask.size() << " type:" << wrinkle_mask.type() << std::endl;
      wrinkle_mask = cv::Mat::zeros(frame_bgr.size(), CV_32F);
    }

  // Debug: Check wrinkle mask coverage
    cv::Scalar wrinkle_sum = cv::sum(wrinkle_mask);
    std::cout << "[DEBUG] Wrinkle mask total coverage: " << wrinkle_sum[0]
              << " (should be > 0 for wrinkle detection)" << std::endl;
  }

  ApplyMultiFrequencyTexturePreservationToLuminance(Lf, refined_face_mask, wrinkle_mask, amount * 0.4f);

  // Layer 4: Advanced frequency separation with expression-based boosting
  // This handles the heavy wrinkle reduction while preserving overall skin quality
  cv::Mat expression_boost = BuildExpressionBoostMap(metrics, frame_bgr.size(), config.expression, fr, frame_bgr);
  if (expression_boost.empty() || expression_boost.size() != frame_bgr.size() || expression_boost.type() != CV_32F) {
    std::cerr << "ApplySnapchatStyleSmoothing: Invalid expression_boost - empty:" << expression_boost.empty()
              << " size:" << expression_boost.size() << " type:" << expression_boost.type() << std::endl;
    expression_boost = cv::Mat::zeros(frame_bgr.size(), CV_32F);
  }

  cv::Mat wrinkle_boost = config.wrinkle_enabled ?
    BuildWrinkleBoostMap(frame_bgr, fr, metrics, config.wrinkle, Lf, base) :
    cv::Mat::zeros(frame_bgr.size(), CV_32F);
  if (wrinkle_boost.empty() || wrinkle_boost.size() != frame_bgr.size() || wrinkle_boost.type() != CV_32F) {
    std::cerr << "ApplySnapchatStyleSmoothing: Invalid wrinkle_boost - empty:" << wrinkle_boost.empty()
              << " size:" << wrinkle_boost.size() << " type:" << wrinkle_boost.type() << std::endl;
    wrinkle_boost = cv::Mat::zeros(frame_bgr.size(), CV_32F);
  }

  // Enhanced boosting for Snapchat-like aggressive wrinkle reduction
  cv::Mat face_gate_u8;
  cv::compare(weight, 1e-6, face_gate_u8, cv::CMP_GT);
  cv::Mat face_gate;
  face_gate_u8.convertTo(face_gate, CV_32F, 1.0/255.0);
  if (face_gate.empty() || face_gate.size() != frame_bgr.size() || face_gate.type() != CV_32F) {
    std::cerr << "ApplySnapchatStyleSmoothing: Invalid face_gate - empty:" << face_gate.empty()
              << " size:" << face_gate.size() << " type:" << face_gate.type() << std::endl;
    face_gate = cv::Mat::ones(frame_bgr.size(), CV_32F);
  }

  float effective_baseline = config.baseline_boost > 0.0f ? config.baseline_boost : 0.8f; // Increased minimum baseline for maximum Snapchat smoothing

  // Snapchat-style contrast reduction around wrinkles (subtle, not aggressive)
  if (config.wrinkle_enabled) {
    float contrast_reduction = config.smile_wrinkle_gain * 0.05f; // Much more subtle
    effective_baseline += contrast_reduction;
  }

  cv::Mat boost_final = CombineBoostMaps(expression_boost, wrinkle_boost, effective_baseline, face_gate);
  if (boost_final.empty() || boost_final.size() != frame_bgr.size() || boost_final.type() != CV_32F) {
    std::cerr << "ApplySnapchatStyleSmoothing: Invalid boost_final - empty:" << boost_final.empty()
              << " size:" << boost_final.size() << " type:" << boost_final.type() << std::endl;
    boost_final = cv::Mat::zeros(frame_bgr.size(), CV_32F);
  }

  // Basic wrinkle reduction logic (Snapchat-style) - keep this for visible smoothing
  float effective_boost_gain = config.boost_gain;

  if (config.wrinkle_enabled) {
    // Base wrinkle reduction for Snapchat-style smoothing (no smile-aware enhancement)
    float base_wrinkle_enhancement = 1.0f + (config.smile_wrinkle_gain * 0.3f); // Conservative base enhancement
    cv::Mat base_wrinkle_boost;
    cv::multiply(wrinkle_boost, cv::Scalar(base_wrinkle_enhancement - 1.0f), base_wrinkle_boost);

    cv::Mat base_boost_masked;
    cv::multiply(base_wrinkle_boost, face_gate, base_boost_masked);
    cv::add(boost_final, base_boost_masked, boost_final);

    std::cout << "[DEBUG] Snapchat-style base wrinkle reduction: " << base_wrinkle_enhancement << "x" << std::endl;
  }

  // Apply the Snapchat-style frequency separation with MAXIMUM increased amount for complete wrinkle smoothing
  float extreme_amount = std::min(amount * 5.0f, 1.0f);  // Quintuple the amount for maximum wrinkle coverage
  ApplyFrequencySeparation(Lf, weight, extreme_amount, effective_boost_gain, boost_final,
                          config.wrinkle_enabled && config.wrinkle_preview, config.neg_atten_cap);
}


FacialExpressionMetrics ApplySkinSmoothingAdvBGR(cv::Mat& frame_bgr, const FaceRegions& fr,
                              const SkinSmoothingConfig& config, const mediapipe::NormalizedLandmarkList* lms,
                              const mediapipe::ClassificationList* blendshapes) {

  // Debug print for config values
  std::cout << "[DEBUG] Skin smoothing config: amount=" << config.amount
            << " boost_gain=" << config.boost_gain
            << " baseline_boost=" << config.baseline_boost
            << " wrinkle_enabled=" << config.wrinkle_enabled
            << " smile_wrinkle_gain=" << config.smile_wrinkle_gain << std::endl;

  float amount = std::clamp(config.amount, 0.0f, 1.0f);
  // Allow processing if either main skin smoothing OR smile wrinkle suppression is enabled
  bool should_process = (amount > 0.0f) && !fr.face_oval.empty();
  if (!should_process) {
    std::cout << "[DEBUG] Skin smoothing SKIPPED: amount=" << amount << std::endl;
    return FacialExpressionMetrics{}; // Return default metrics when no processing needed
  }

  // Phase 2: Extract expressions (needed for both frequency separation and smile wrinkles)
  FacialExpressionMetrics metrics = ExtractFacialExpressions(lms, frame_bgr.cols, frame_bgr.rows, blendshapes);

  // Phase 1: Prepare data (only needed for frequency separation)
  cv::Mat weight, Lf, base;
  cv::Mat orig_a, orig_b; // Save original color channels for consistency
  if (amount > 0.0f) {
    weight = BuildSkinWeightMap(fr, frame_bgr.size(), config.edge_feather_px,
                                config.texture_thresh, frame_bgr, &metrics);
    
    // Debug: Check weight map coverage
    cv::Scalar weight_sum = cv::sum(weight);
    std::cout << "[DEBUG] Weight map total coverage: " << weight_sum[0] 
              << " face_oval size: " << fr.face_oval.size() << std::endl;
    
    cv::Mat orig_lab;
    cv::cvtColor(frame_bgr, orig_lab, cv::COLOR_BGR2Lab);
    std::vector<cv::Mat> orig_ch;
    cv::split(orig_lab, orig_ch);
    orig_a = orig_ch[1].clone(); // Save original A channel
    orig_b = orig_ch[2].clone(); // Save original B channel
    Lf = PrepareLabLuminance(frame_bgr);
    base = CreateGaussianBase(Lf, config.radius_px);
  }

  // SNAPCHAT-STYLE MULTI-LAYER SKIN SMOOTHING
  cv::Mat frame_before_snapchat = frame_bgr.clone(); // Save state before Snapchat processing
  if (amount > 0.0f) {
    // Create refined face mask for better skin detection
    cv::Mat refined_face_mask = CreateRefinedFaceMask(fr, frame_bgr.size(), weight);

    // Apply multi-layer smoothing: combine bilateral + frequency separation
    ApplySnapchatStyleSmoothing(frame_bgr, refined_face_mask, fr, config, metrics, Lf, base, weight);
  }

  // Phase 5: Convert back to BGR (only if we did frequency separation)
  if (amount > 0.0f) {
    cv::Mat lab;
    cv::cvtColor(frame_bgr, lab, cv::COLOR_BGR2Lab);
    std::vector<cv::Mat> ch;
    cv::split(lab, ch);
    // Use original color channels for consistency, only update luminance
    ch[1] = orig_a;
    ch[2] = orig_b;
    UpdateLabAndConvertBack(Lf, ch, lab, frame_bgr);
  }

  // Smile wrinkle suppression is now handled by the multi-layer smoothing above
  return metrics;
}