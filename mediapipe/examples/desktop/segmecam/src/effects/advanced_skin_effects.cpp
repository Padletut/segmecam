#include "include/effects/advanced_skin_effects.h"
#include <cmath>

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

// Helper struct for facial expression analysis
// Helper functions for ApplySkinSmoothingAdvBGR
FacialExpressionMetrics ExtractFacialExpressions(const mediapipe::NormalizedLandmarkList* lms, int width, int height) {
  FacialExpressionMetrics metrics = {0.0f, 0.0f};

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

  // Combine local and line masks with sensitivity: higher keep_ratio favors line mask
  float s = std::clamp(config.keep_ratio, 0.02f, 0.80f);
  float s_norm = (s - 0.02f) / (0.78f); // 0..1
  float w_line = 0.4f + 0.9f * s_norm;   // 0.4 .. 1.3
  float w_local = 0.6f * (1.0f - s_norm); // 0.6 .. 0
  cv::Mat wrinkle_mask = cv::min(1.0f, wrinkle_line * w_line + wrinkle_local * w_local);

  return wrinkle_mask;
}

void ApplyFrequencySeparation(cv::Mat& Lf, const cv::Mat& weight, float amount, float boost_gain,
                             const cv::Mat& boost_final, bool wrinkle_preview, float neg_atten_cap) {
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
                              const SkinSmoothingConfig& config, const mediapipe::NormalizedLandmarkList* lms) {


  float amount = std::clamp(config.amount, 0.0f, 1.0f);
  if (amount <= 0.0f || fr.face_oval.empty()) return;

  // Phase 1: Prepare data
  cv::Mat weight = BuildSkinWeightMap(fr, frame_bgr.size(), config.edge_feather_px,
                                    config.texture_thresh, frame_bgr);
  cv::Mat Lf = PrepareLabLuminance(frame_bgr);
  cv::Mat base = CreateGaussianBase(Lf, config.radius_px);

  // Phase 2: Extract expressions and build boosts
  FacialExpressionMetrics metrics = ExtractFacialExpressions(lms, frame_bgr.cols, frame_bgr.rows);
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

  // Phase 4: Apply frequency separation
  ApplyFrequencySeparation(Lf, weight, amount, config.boost_gain, boost_final,
                          config.wrinkle_enabled && config.wrinkle_preview, config.neg_atten_cap);

  // Phase 5: Convert back to BGR
  cv::Mat lab;
  cv::cvtColor(frame_bgr, lab, cv::COLOR_BGR2Lab);
  std::vector<cv::Mat> ch;
  cv::split(lab, ch);
  UpdateLabAndConvertBack(Lf, ch, lab, frame_bgr);
}