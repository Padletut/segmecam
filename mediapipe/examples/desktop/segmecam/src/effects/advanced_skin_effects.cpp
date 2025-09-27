
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
    cv::Mat outF = dstF + (resF - dstF).mul(a3) * gain;

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
  cv::Mat wr = acc.mul(coh).mul(base_f).mul(skin_f).mul(extra_gate);
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

  cv::Mat boost_any = cv::min(1.0f, expression_boost + wrinkle_boost + std::max(0.0f, baseline_boost));
  cv::Mat boost_final = boost_any.mul(face_gate);
  return boost_final;
}

void UpdateLabAndConvertBack(cv::Mat& Lf, std::vector<cv::Mat>& ch, cv::Mat& lab, cv::Mat& frame_bgr) {
  Lf.convertTo(ch[0], CV_8U, 255.0);
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
  cv::GaussianBlur(gx.mul(gx), Jxx, cv::Size(0,0), 1.5);
  cv::GaussianBlur(gy.mul(gy), Jyy, cv::Size(0,0), 1.5);
  cv::GaussianBlur(gx.mul(gy), Jxy, cv::Size(0,0), 1.5);

  cv::Mat tmp;
  cv::Mat diff = Jxx - Jyy;
  cv::pow(diff, 2.0, tmp);
  cv::Mat D;
  cv::sqrt(tmp + 4.0*Jxy.mul(Jxy), D);
  cv::Mat trace = Jxx + Jyy;
  cv::Mat lam1 = 0.5*(trace + D);
  cv::Mat lam2 = 0.5*(trace - D);
  cv::Mat coh = (lam1 - lam2) / (lam1 + lam2 + 1e-6f);
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
    cv::Mat region_f = base_f.mul(skin_f).mul(extra_gate);
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
    wr = wr.mul(strong_f);
  }
  if (mask_gain > 1.0f) wr = cv::min(1.0f, wr * mask_gain);
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
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f  // cheek_squint_left, cheek_squint_right, brow_inner_up, brow_outer_up_left, brow_outer_up_right
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

  // Squint factor: lower aperture => higher value; neutral ~0.22
  metrics.squint_factor = (float)std::clamp((0.22 - aperture) / 0.12, 0.0, 1.0);

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
  // Use mouthUpperUpLeft/Right as proxy for cheek activity (more responsive than cheekSquint)
  metrics.cheek_squint_left = get_blendshape("mouthUpperUpLeft");
  metrics.cheek_squint_right = get_blendshape("mouthUpperUpRight");
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
  // Build wrinkle-aware attenuation: emphasize dark, narrow, linear structures.
  // 1) Negative detail + gradient gate (local, fast)
  cv::Mat detail = Lf - base;
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

  // Cheek wrinkles: boost nasolabial folds when smiling or squinting cheeks
  float cheek_expression = std::max({metrics.cheek_squint_left, metrics.cheek_squint_right, metrics.smile_factor});
  // Inpaint nasolabial fold (cheek wrinkle) only when smiling strongly
  if (cheek_expression > 0.5f && !fr.face_oval.empty()) {
    float smile_gain = std::max(0.0f, config.smile_wrinkle_gain);
    float boost_val = cheek_expression * (2.0f + 10.0f * smile_gain); // 2.0 base, up to 12x

    // Use metrics for nose edge and mouth corners
    cv::Point nose_left = metrics.nose_left;
    cv::Point nose_right = metrics.nose_right;
    cv::Point mouth_left = metrics.mouth_left;
    cv::Point mouth_right = metrics.mouth_right;

    // Build mask for inpainting
    cv::Mat wrinkle_mask = cv::Mat::zeros(frame_bgr.size(), CV_8U);
    std::vector<cv::Point> naso_left, naso_right;
    for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
      int x = static_cast<int>(nose_left.x * (1-t) + mouth_left.x * t);
      int y = static_cast<int>(nose_left.y * (1-t) + mouth_left.y * t);
      naso_left.push_back(cv::Point(x, y));
      x = static_cast<int>(nose_right.x * (1-t) + mouth_right.x * t);
      y = static_cast<int>(nose_right.y * (1-t) + mouth_right.y * t);
      naso_right.push_back(cv::Point(x, y));
    }
    for (const auto& pt : naso_left) {
      cv::circle(wrinkle_mask, pt, 6, cv::Scalar(255), cv::FILLED);
    }
    for (const auto& pt : naso_right) {
      cv::circle(wrinkle_mask, pt, 6, cv::Scalar(255), cv::FILLED);
    }
    cv::GaussianBlur(wrinkle_mask, wrinkle_mask, cv::Size(0,0), 3.0f);

    // Inpaint only the wrinkle mask region
    cv::Mat inpainted;
    auto dump = [](const char* n, const cv::Mat& m){
      std::cout << "[DEBUG] " << n << " size=" << m.cols << "x" << m.rows
                << " ch=" << m.channels() << " type=" << m.type() << std::endl;
    };
    dump("inpaint input", frame_bgr);
    dump("inpaint mask", wrinkle_mask);
    cv::inpaint(frame_bgr, wrinkle_mask, inpainted, 7.0, cv::INPAINT_TELEA);
    dump("inpaint output", inpainted);

    // Ensure inpainted is valid
    if (inpainted.empty() || inpainted.size() != frame_bgr.size()) {
      std::cerr << "❌ Inpaint failed or returned invalid result" << std::endl;
      return cv::Mat::zeros(frame_bgr.size(), CV_32F); // Return zero boost map as fallback
    }

    // Ensure mask and inpainted are same size as frame_bgr (for process scaling)
    if (wrinkle_mask.size() != frame_bgr.size()) {
      cv::resize(wrinkle_mask, wrinkle_mask, frame_bgr.size(), 0, 0, cv::INTER_NEAREST);
    }
    if (inpainted.size() != frame_bgr.size()) {
      cv::resize(inpainted, inpainted, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
    }
    // Convert mask to float and expand to 3 channels
    cv::Mat mask_f;
    wrinkle_mask.convertTo(mask_f, CV_32F, 1.0/255.0);
    cv::Mat mask3;
    cv::Mat mask_channels[] = {mask_f, mask_f, mask_f};
    cv::merge(mask_channels, 3, mask3);

    // Ensure mask values are in valid range [0,1] using pixel-wise operations
    for (int y = 0; y < mask3.rows; ++y) {
      for (int x = 0; x < mask3.cols; ++x) {
        cv::Vec3f& pixel = mask3.at<cv::Vec3f>(y, x);
        for (int c = 0; c < 3; ++c) {
          pixel[c] = std::max(0.0f, std::min(1.0f, pixel[c]));
        }
      }
    }

    // Convert images to float and 3 channels
    cv::Mat orig, inp;
    if (frame_bgr.type() != CV_32FC3) frame_bgr.convertTo(orig, CV_32F, 1.0/255.0);
    else orig = frame_bgr;
    if (inpainted.type() != CV_32FC3) inpainted.convertTo(inp, CV_32F, 1.0/255.0);
    else inp = inpainted;

    // Sanity check: all must be same size and 3 channels
    if (orig.size() != inp.size() || orig.size() != mask3.size()) {
      std::cerr << "BuildWrinkleBoostMap: Size mismatch - orig:" << orig.size()
                << " inp:" << inp.size() << " mask3:" << mask3.size() << std::endl;
      return cv::Mat::zeros(frame_bgr.size(), CV_32F);
    }
    if (orig.channels() != 3 || inp.channels() != 3 || mask3.channels() != 3) {
      std::cerr << "BuildWrinkleBoostMap: Channel mismatch - orig:" << orig.channels()
                << " inp:" << inp.channels() << " mask3:" << mask3.channels() << std::endl;
      return cv::Mat::zeros(frame_bgr.size(), CV_32F);
    }
    if (orig.type() != CV_32FC3 || inp.type() != CV_32FC3 || mask3.type() != CV_32FC3) {
      std::cerr << "BuildWrinkleBoostMap: Type mismatch - orig:" << orig.type()
                << " inp:" << inp.type() << " mask3:" << mask3.type() << std::endl;
      return cv::Mat::zeros(frame_bgr.size(), CV_32F);
    }

    CV_Assert(orig.size() == inp.size() && orig.size() == mask3.size());
    CV_Assert(orig.channels() == 3 && inp.channels() == 3 && mask3.channels() == 3);

    // Additional validation before arithmetic operations
    cv::Mat ones = cv::Mat::ones(mask3.size(), CV_32FC3);
    std::cout << "[DEBUG] BuildWrinkleBoostMap: ones size=" << ones.size() << " type=" << ones.type()
              << " mask3 size=" << mask3.size() << " type=" << mask3.type() << std::endl;
    if (ones.size() != mask3.size() || ones.type() != mask3.type()) {
      std::cerr << "BuildWrinkleBoostMap: ones matrix incompatible - ones:" << ones.size() << " type:" << ones.type()
                << " mask3:" << mask3.size() << " type:" << mask3.type() << std::endl;
      return cv::Mat::zeros(frame_bgr.size(), CV_32F);
    }

    // Blend using proper alpha blending with pixel-wise operations
    cv::Mat blended;
    try {
      // Use the already converted matrices (orig and inp are already CV_32FC3)
      // mask3 is already CV_32FC3 from the merge operation

      // Pixel-wise alpha blending: result = (1 - mask) * orig + mask * inp
      blended = cv::Mat::zeros(orig.size(), CV_32FC3);
      for (int y = 0; y < orig.rows; ++y) {
        for (int x = 0; x < orig.cols; ++x) {
          cv::Vec3f orig_pixel = orig.at<cv::Vec3f>(y, x);
          cv::Vec3f inp_pixel = inp.at<cv::Vec3f>(y, x);
          cv::Vec3f mask_pixel = mask3.at<cv::Vec3f>(y, x);

          cv::Vec3f result_pixel;
          for (int c = 0; c < 3; ++c) {
            float alpha = mask_pixel[c];
            result_pixel[c] = (1.0f - alpha) * orig_pixel[c] + alpha * inp_pixel[c];
          }
          blended.at<cv::Vec3f>(y, x) = result_pixel;
        }
      }

      // Convert to single-channel boost map (already in [0,1] range from blending)
      cv::cvtColor(blended, blendshape_boost, cv::COLOR_BGR2GRAY);
    } catch (const cv::Exception& e) {
      std::cerr << "BuildWrinkleBoostMap: Pixel-wise blending failed: " << e.what() << std::endl;
      return cv::Mat::zeros(frame_bgr.size(), CV_32F);
    }
  }

  // Combine local and line masks with sensitivity: higher keep_ratio favors line mask
  float s = std::clamp(config.keep_ratio, 0.02f, 0.80f);
  float s_norm = (s - 0.02f) / (0.78f); // 0..1
  float w_line = 0.4f + 0.9f * s_norm;   // 0.4 .. 1.3
  float w_local = 0.6f * (1.0f - s_norm); // 0.6 .. 0
  float w_blendshape = 0.3f; // Weight for blendshape-guided enhancement

  // EXTREME: Increase blendshape boost weight for smile suppression
  float w_blendshape_extreme = 1.0f;
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
  cv::Mat wrinkle_mask = cv::min(1.0f, wrinkle_line_f * w_line + wrinkle_local_f * w_local + blendshape_boost_f * w_blendshape_extreme);
  
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
  cv::Mat base;
  cv::GaussianBlur(Lf, base, cv::Size(0, 0), 3.0); // 3px sigma for low-frequency base

  // In wrinkle preview mode, show only wrinkle attenuation, no base smoothing
  if (wrinkle_preview) {
    base = Lf; // no low-pass filtering in preview mode
  }

  // Frequency separation handling that preserves highlights/pores:
  // Split detail into positive (highlights/pores) and negative (shadows/wrinkles).
  cv::Mat detail = Lf - base;
  cv::Mat detail_pos = cv::max(detail, 0.0f);
  cv::Mat detail_neg = cv::min(detail, 0.0f);

  // Attenuate detail by amount * weight, with optional wrinkle-aware boost
  cv::Mat atten = weight * amount; // CV_32F (base smoothing)
  if (wrinkle_preview) {
    // In preview: show only wrinkle attenuation, no base smoothing
    atten = cv::min(1.0f, boost_gain * boost_final);
  } else {
    atten = cv::min(1.0f, atten + boost_gain * boost_final);
  }

  // Use lighter attenuation for positive detail to keep sheen/pores.
  cv::Mat pos_atten = wrinkle_preview ? cv::Mat::zeros(atten.size(), CV_32F) : (weight * (amount * 0.15f));
  // Stronger attenuation on negative detail (wrinkle shadows), clamped to user cap
  float cap = std::clamp(neg_atten_cap, 0.4f, 1.0f);
  cv::Mat neg_atten = cv::min(cap, atten);

  cv::Mat outL = base + detail_pos.mul(1.0f - pos_atten) + detail_neg.mul(1.0f - neg_atten);
  outL = cv::min(cv::max(outL, 0.0f), 1.0f);
  outL.copyTo(Lf);
}

cv::Mat BuildSkinWeightMap(const FaceRegions& fr,
                           const cv::Size& frame_size,
                           float edge_feather_px,
                           float texture_thresh,
                           const cv::Mat& hint_bgr) {
  cv::Mat base(frame_size, CV_8UC1, cv::Scalar(0));
  if (!fr.face_oval.empty()) {
    cv::fillPoly(base, std::vector<std::vector<cv::Point>>{fr.face_oval}, cv::Scalar(255));
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
  weight_edge = weight_edge.mul(base_f);

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
  cv::Mat wtex = 1.0f / (1.0f + (mag_n / t));
  wtex = cv::min(wtex, 1.0f);

  cv::Mat weight = weight_edge.mul(wtex);
  // Ensure a baseline weight inside the face so effect is visible
  weight = cv::max(weight, 0.15f * base_f);
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


void ApplySkinSmoothingAdvBGR(cv::Mat& frame_bgr, const FaceRegions& fr,
                              const SkinSmoothingConfig& config, const mediapipe::NormalizedLandmarkList* lms,
                              const mediapipe::ClassificationList* blendshapes) {


  // Debug print for config values
  std::cout << "[DEBUG] boost_gain: " << config.boost_gain
            << " smile_wrinkle_gain: " << config.smile_wrinkle_gain << std::endl;

  float amount = std::clamp(config.amount, 0.0f, 1.0f);
  if (amount <= 0.0f || fr.face_oval.empty()) return;

  // Phase 1: Prepare data
  cv::Mat weight = BuildSkinWeightMap(fr, frame_bgr.size(), config.edge_feather_px,
                                    config.texture_thresh, frame_bgr);
  cv::Mat Lf = PrepareLabLuminance(frame_bgr);
  cv::Mat base = CreateGaussianBase(Lf, config.radius_px);

  // Phase 2: Extract expressions and build boosts
  FacialExpressionMetrics metrics = ExtractFacialExpressions(lms, frame_bgr.cols, frame_bgr.rows, blendshapes);
  cv::Mat expression_boost = BuildExpressionBoostMap(metrics, frame_bgr.size(), config.expression, fr, frame_bgr);
  cv::Mat wrinkle_boost = config.wrinkle_enabled ? 
    BuildWrinkleBoostMap(frame_bgr, fr, metrics, config.wrinkle, Lf, base) : 
    cv::Mat::zeros(frame_bgr.size(), CV_32F);

  // Phase 3: Combine and apply boosts
  cv::Mat face_gate_u8;
  cv::compare(weight, 1e-6, face_gate_u8, cv::CMP_GT);
  cv::Mat face_gate;
  face_gate_u8.convertTo(face_gate, CV_32F, 1.0/255.0);
  // Only apply baseline boost when wrinkle-aware is enabled
  float effective_baseline = config.wrinkle_enabled ? config.baseline_boost : 0.0f;
  cv::Mat boost_final = CombineBoostMaps(expression_boost, wrinkle_boost, effective_baseline, face_gate);


  // Phase 4: Apply frequency separation as usual
  ApplyFrequencySeparation(Lf, weight, amount, config.boost_gain, boost_final,
                          config.wrinkle_enabled && config.wrinkle_preview, config.neg_atten_cap);

  // --- Smile wrinkle inpainting (Snapchat-style cheek wrinkle removal) ---
  if (config.smile_wrinkle_gain > 0.01f && config.wrinkle_enabled && lms) {
    FacialExpressionMetrics metrics = ExtractFacialExpressions(lms, frame_bgr.cols, frame_bgr.rows, blendshapes);
    float cheek_expression = std::max({metrics.cheek_squint_left, metrics.cheek_squint_right, metrics.smile_factor});
    if (cheek_expression > 0.2f && !fr.face_oval.empty()) {
      std::cout << "[DEBUG] Inpainting triggered: cheek_expression=" << cheek_expression << std::endl;
      // Build nasolabial fold mask (thick for debug)
      cv::Point nose_left = metrics.nose_left;
      cv::Point nose_right = metrics.nose_right;
      cv::Point mouth_left = metrics.mouth_left;
      cv::Point mouth_right = metrics.mouth_right;
      cv::Mat wrinkle_mask = cv::Mat::zeros(frame_bgr.size(), CV_8U);
      std::vector<cv::Point> naso_left, naso_right;
      for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
        int x = static_cast<int>(nose_left.x * (1-t) + mouth_left.x * t);
        int y = static_cast<int>(nose_left.y * (1-t) + mouth_left.y * t);
        naso_left.push_back(cv::Point(x, y));
        x = static_cast<int>(nose_right.x * (1-t) + mouth_right.x * t);
        y = static_cast<int>(nose_right.y * (1-t) + mouth_right.y * t);
        naso_right.push_back(cv::Point(x, y));
      }
      for (const auto& pt : naso_left) {
        cv::circle(wrinkle_mask, pt, 18, cv::Scalar(255), cv::FILLED); // much thicker for debug
      }
      for (const auto& pt : naso_right) {
        cv::circle(wrinkle_mask, pt, 18, cv::Scalar(255), cv::FILLED);
      }
      cv::GaussianBlur(wrinkle_mask, wrinkle_mask, cv::Size(0,0), 6.0f);

      // Robust mask conversion: never convert in-place, always use new variables
  cv::Mat mask_resized, mask_u8;
      if (wrinkle_mask.size() != frame_bgr.size()) {
        cv::resize(wrinkle_mask, mask_resized, frame_bgr.size(), 0, 0, cv::INTER_NEAREST);
      } else {
        mask_resized = wrinkle_mask.clone();
      }
      if (mask_resized.type() != CV_8UC1) {
        mask_resized.convertTo(mask_u8, CV_8U);
      } else {
        mask_u8 = mask_resized;
      }

      if (mask_u8.size() != frame_bgr.size() || mask_u8.type() != CV_8UC1) {
        return;
      }

      // Ensure mask is single-channel 8U and same size as frame
      // Robust mask conversion: never convert in-place, always use new variables
      // Inpaint only the wrinkle mask region
      cv::Mat inpainted;
      auto dump = [](const char* n, const cv::Mat& m){
        std::cout << "[DEBUG] " << n << " size=" << m.cols << "x" << m.rows
                  << " ch=" << m.channels() << " type=" << m.type() << std::endl;
      };
      dump("inpaint input", frame_bgr);
      dump("inpaint mask", mask_u8);
      cv::inpaint(frame_bgr, mask_u8, inpainted, 7.0, cv::INPAINT_TELEA);
      dump("inpaint output", inpainted);

      // Blend inpainted region into output frame using pixel-wise blending with validation
      cv::Mat mask_f;
      mask_u8.convertTo(mask_f, CV_32F, 1.0/255.0);

      // Validate matrices before blending
      if (frame_bgr.size() != inpainted.size() || frame_bgr.size() != mask_f.size()) {
        std::cout << "[ERROR] Matrix size mismatch in inpainting blend: frame=" << frame_bgr.size()
                  << " inpainted=" << inpainted.size() << " mask=" << mask_f.size() << std::endl;
        return;
      }

      if (frame_bgr.type() != CV_8UC3 || inpainted.type() != CV_8UC3 || mask_f.type() != CV_32FC1) {
        std::cout << "[ERROR] Matrix type mismatch in inpainting blend: frame=" << frame_bgr.type()
                  << " inpainted=" << inpainted.type() << " mask=" << mask_f.type() << std::endl;
        return;
      }

      // Apply gain to mask for blending strength
      mask_f *= config.smile_wrinkle_gain;

      // Pixel-wise blending with bounds checking
      for (int y = 0; y < frame_bgr.rows; ++y) {
        for (int x = 0; x < frame_bgr.cols; ++x) {
          float w = mask_f.at<float>(y, x);
          if (w > 0.01f) {
            // Clamp weight to prevent over-blending
            w = std::min(w, 1.0f);
            for (int c = 0; c < 3; ++c) {
              float orig = frame_bgr.at<cv::Vec3b>(y, x)[c];
              float inpt = inpainted.at<cv::Vec3b>(y, x)[c];
              float blended = orig * (1.0f - w) + inpt * w;
              frame_bgr.at<cv::Vec3b>(y, x)[c] = static_cast<uchar>(std::clamp(blended, 0.0f, 255.0f));
            }
          }
        }
      }
    }
  }

  // Phase 5: Convert back to BGR
  cv::Mat lab;
  cv::cvtColor(frame_bgr, lab, cv::COLOR_BGR2Lab);
  std::vector<cv::Mat> ch;
  cv::split(lab, ch);
  UpdateLabAndConvertBack(Lf, ch, lab, frame_bgr);
}