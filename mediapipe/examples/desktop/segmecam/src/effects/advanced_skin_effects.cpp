#include "include/effects/advanced_skin_effects.h"
#include <cmath>

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

cv::Mat BuildWrinkleLineMask(const cv::Mat& frame_bgr,
                             const FaceRegions& fr,
                             float min_scale_px,
                             float max_scale_px,
                             bool suppress_lower_face,
                             float lower_face_ratio,
                             bool ignore_glasses,
                             float glasses_margin_px,
                             float keep_ratio,
                             bool use_skin_gate,
                             float mask_gain) {
  cv::Size sz = frame_bgr.size();
  min_scale_px = std::max(1.0f, min_scale_px);
  max_scale_px = std::max(min_scale_px, max_scale_px);

  // Base face region mask (uint8)
  cv::Mat base(sz, CV_8U, cv::Scalar(0));
  if (!fr.face_oval.empty()) cv::fillPoly(base, std::vector<std::vector<cv::Point>>{fr.face_oval}, cv::Scalar(255));
  if (!fr.lips_outer.empty()) cv::fillPoly(base, std::vector<std::vector<cv::Point>>{fr.lips_outer}, cv::Scalar(0));
  if (!fr.left_eye.empty())   cv::fillPoly(base, std::vector<std::vector<cv::Point>>{fr.left_eye},   cv::Scalar(0));
  if (!fr.right_eye.empty())  cv::fillPoly(base, std::vector<std::vector<cv::Point>>{fr.right_eye},  cv::Scalar(0));

  // Skin gating (suppress textiles, hair/stubble). Quick YCrCb thresholds.
  cv::Mat ycrcb; cv::cvtColor(frame_bgr, ycrcb, cv::COLOR_BGR2YCrCb);
  std::vector<cv::Mat> yc; cv::split(ycrcb, yc);
  cv::Mat skin;
  {
    // Typical ranges (8-bit): Cr in [135, 180], Cb in [85, 135]
    cv::Mat m1, m2; cv::inRange(yc[1], 135, 180, m1); cv::inRange(yc[2], 85, 135, m2);
    cv::bitwise_and(m1, m2, skin);
    // Smooth and clean small holes
    cv::GaussianBlur(skin, skin, cv::Size(0,0), 1.5);
    cv::Mat ker = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3,3));
    cv::morphologyEx(skin, skin, cv::MORPH_CLOSE, ker);
  }

  // Convert to LAB -> L channel (8U)
  cv::Mat lab; cv::cvtColor(frame_bgr, lab, cv::COLOR_BGR2Lab);
  std::vector<cv::Mat> ch; cv::split(lab, ch);
  cv::Mat L8 = ch[0];

  // Multi-scale black-hat to emphasize dark narrow lines
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
    cv::Mat bh; cv::morphologyEx(L8, bh, cv::MORPH_BLACKHAT, elem);
    cv::Mat bhf; bh.convertTo(bhf, CV_32F, 1.0/255.0);
    // Favor smaller scales slightly (narrower lines)
    float w = 1.0f - 0.25f * (s - min_scale_px) / std::max(1e-3f, (max_scale_px - min_scale_px));
    acc = cv::max(acc, bhf * w);
  }

  // Orientation coherence via structure tensor to prefer line-like over blotchy
  cv::Mat gray; cv::cvtColor(frame_bgr, gray, cv::COLOR_BGR2GRAY);
  cv::Mat gx, gy; cv::Sobel(gray, gx, CV_32F, 1, 0, 3); cv::Sobel(gray, gy, CV_32F, 0, 1, 3);
  cv::Mat Jxx, Jyy, Jxy; cv::GaussianBlur(gx.mul(gx), Jxx, cv::Size(0,0), 1.5);
  cv::GaussianBlur(gy.mul(gy), Jyy, cv::Size(0,0), 1.5);
  cv::GaussianBlur(gx.mul(gy), Jxy, cv::Size(0,0), 1.5);
  cv::Mat tmp; cv::Mat diff = Jxx - Jyy; cv::pow(diff, 2.0, tmp);
  cv::Mat D; cv::sqrt(tmp + 4.0*Jxy.mul(Jxy), D);
  cv::Mat trace = Jxx + Jyy;
  cv::Mat lam1 = 0.5*(trace + D);
  cv::Mat lam2 = 0.5*(trace - D);
  cv::Mat coh = (lam1 - lam2) / (lam1 + lam2 + 1e-6f);
  coh = cv::min(cv::max(coh, 0.0f), 1.0f);

  // Optional suppress lower-face stubble region using a horizontal cut
  cv::Mat extra_gate = cv::Mat::ones(sz, CV_32F);
  if (suppress_lower_face && !fr.face_oval.empty() && !fr.lips_outer.empty()) {
    int mouth_y = 0; for (const auto& p : fr.lips_outer) mouth_y += p.y; mouth_y = (int)std::round((double)mouth_y / std::max(1,(int)fr.lips_outer.size()));
    int chin_y = 0; for (const auto& p : fr.face_oval) chin_y = std::max(chin_y, p.y);
    int cut_y = mouth_y + (int)std::round(std::clamp(lower_face_ratio, 0.2f, 0.8f) * (chin_y - mouth_y));
    extra_gate = cv::Mat::zeros(sz, CV_32F);
    cv::rectangle(extra_gate, cv::Rect(0,0,sz.width, std::max(0, cut_y)), cv::Scalar(1.0f), cv::FILLED);
  }

  // Optional ignore glasses: suppress a band covering both eyes with margin
  if (ignore_glasses && (!fr.left_eye.empty() || !fr.right_eye.empty())) {
    cv::Rect er;
    auto rect_of = [](const std::vector<cv::Point>& poly){ return cv::boundingRect(poly); };
    if (!fr.left_eye.empty()) er = rect_of(fr.left_eye);
    if (!fr.right_eye.empty()) er = er.area() ? (er | rect_of(fr.right_eye)) : rect_of(fr.right_eye);
    int m = (int)std::round(std::max(0.0f, glasses_margin_px));
    er.x = std::max(0, er.x - m); er.y = std::max(0, er.y - m);
    er.width = std::min(sz.width - er.x, er.width + 2*m);
    er.height = std::min(sz.height - er.y, er.height + 2*m);
    // Zero inside the band
    cv::rectangle(extra_gate, er, cv::Scalar(0.0f), cv::FILLED);
    // Feather edges slightly for smooth transition
    cv::GaussianBlur(extra_gate, extra_gate, cv::Size(0,0), 2.0);
  }

  // Combine and restrict to face region
  cv::Mat base_f; base.convertTo(base_f, CV_32F, 1.0/255.0);
  cv::Mat skin_f; if (use_skin_gate) skin.convertTo(skin_f, CV_32F, 1.0/255.0); else skin_f = cv::Mat(base_f.size(), CV_32F, cv::Scalar(1.0f));
  cv::Mat wr = acc.mul(coh).mul(base_f).mul(skin_f).mul(extra_gate);
  cv::GaussianBlur(wr, wr, cv::Size(0,0), 1.0);
  wr = cv::min(cv::max(wr, 0.0f), 1.0f);

  // Keep only top percentile of responses inside face+skin region to make lines thin and localized.
  keep_ratio = std::clamp(keep_ratio, 0.02f, 0.5f); // 2%..50%
  cv::Mat region_mask_u8; // where to measure histogram
  {
    cv::Mat region_f = base_f.mul(skin_f).mul(extra_gate);
    region_f.convertTo(region_mask_u8, CV_8U, 255.0);
  }
  if (cv::countNonZero(region_mask_u8) > 0) {
    cv::Mat wr8; wr.convertTo(wr8, CV_8U, 255.0);
    int histSize = 256; float range[] = {0, 256}; const float* ranges = {range};
    cv::Mat hist;
    cv::calcHist(&wr8, 1, 0, region_mask_u8, hist, 1, &histSize, &ranges, true, false);
    double total = cv::sum(hist)[0];
    double target = total * keep_ratio;
    int thrBin = 255; double acc = 0.0;
    for (int b = 255; b >= 0; --b) { acc += hist.at<float>(b); if (acc >= target) { thrBin = b; break; } }
    double thrVal = (double)thrBin;
    cv::Mat strong; cv::threshold(wr8, strong, thrVal, 255, cv::THRESH_BINARY);
    // Slightly blur to make soft weights
    cv::GaussianBlur(strong, strong, cv::Size(0,0), 0.75);
    cv::Mat strong_f; strong.convertTo(strong_f, CV_32F, 1.0/255.0);
    wr = wr.mul(strong_f);
  }
  if (mask_gain > 1.0f) wr = cv::min(1.0f, wr * mask_gain);
  return wr; // CV_32F [0,1]
}

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
                              bool suppress_lower_face,
                              float lower_face_ratio,
                              bool ignore_glasses,
                              float glasses_margin_px,
                              float keep_ratio,
                              float line_min_px,
                              float line_max_px,
                              float forehead_margin_px,
                              bool wrinkle_preview,
                              float baseline_boost,
                              bool use_skin_gate,
                              float mask_gain,
                              float neg_atten_cap) {
  amount = std::clamp(amount, 0.0f, 1.0f);
  if (amount <= 0.0f || fr.face_oval.empty()) return;

  cv::Mat weight = BuildSkinWeightMap(fr, frame_bgr.size(), edge_feather_px, texture_thresh, frame_bgr);
  // Prepare LAB
  cv::Mat lab; cv::cvtColor(frame_bgr, lab, cv::COLOR_BGR2Lab);
  std::vector<cv::Mat> ch; cv::split(lab, ch);
  cv::Mat Lf; ch[0].convertTo(Lf, CV_32F, 1.0/255.0);

  // Base (low-pass) via Gaussian with radius ~ sigma (or identity in preview)
  int k = std::max(1, (int)std::round(radius_px) * 2 + 1);
  cv::Mat base; cv::GaussianBlur(Lf, base, cv::Size(k, k), radius_px);
  cv::Mat detail = Lf - base;

  // Attenuate detail by amount * weight, with optional wrinkle-aware boost
  cv::Mat atten = weight * amount; // CV_32F (base smoothing)
  // Build wrinkle-aware attenuation whenever landmarks are available (independent of smile/squint)
  if (lms) {
    const int W = frame_bgr.cols, H = frame_bgr.rows;
    auto pt = [&](int idx){
      idx = std::clamp(idx, 0, lms->landmark_size()-1);
      const auto& p = lms->landmark(idx);
      return cv::Point(std::clamp((int)std::round(p.x()*W), 0, W-1), std::clamp((int)std::round(p.y()*H), 0, H-1));
    };
    auto dist = [&](const cv::Point& a, const cv::Point& b){ return std::hypot((double)a.x-b.x, (double)a.y-b.y); };
    // Key points
    cv::Point mouthL = pt(61), mouthR = pt(291);
    cv::Point eyeLO = pt(33), eyeLI = pt(133);
    cv::Point eyeRO = pt(263), eyeRI = pt(362);
    cv::Point eyeLT = pt(159), eyeLB = pt(145);
    cv::Point eyeRT = pt(386), eyeRB = pt(374);
    double eyeSpan = dist(eyeLO, eyeRO);
    double mouthW = dist(mouthL, mouthR);
    double leftW = dist(eyeLO, eyeLI); double rightW = dist(eyeRO, eyeRI);
    double leftH = std::abs(eyeLT.y - eyeLB.y); double rightH = std::abs(eyeRT.y - eyeRB.y);
    double aperture = 0.5*(leftH/ std::max(1.0,leftW) + rightH/ std::max(1.0,rightW));
    // Smile factor: ratio mapped to 0..1 (neutral ~0.35, smile ~0.55)
    double smile_ratio = (eyeSpan > 1.0) ? (mouthW/eyeSpan) : 0.0;
    float smile_f = (float)std::clamp((smile_ratio - 0.35) / 0.20, 0.0, 1.0);
    // Squint factor: lower aperture => higher value; neutral ~0.22
    float squint_f = (float)std::clamp((0.22 - aperture) / 0.12, 0.0, 1.0);

    // Build boost maps for nasolabial (mouth corners) and crow's-feet (outer eye corners)
    cv::Mat boost(frame_bgr.size(), CV_32F, cv::Scalar(0));
    int r_naso = std::max(3, (int)std::round(0.08 * eyeSpan));
    // Slightly larger to make effect visible at eyes
    int r_crow = std::max(3, (int)std::round(0.08 * eyeSpan));
    if (smile_boost > 0.0f && smile_f > 0.0f) {
      cv::Mat m(frame_bgr.size(), CV_8U, cv::Scalar(0));
      cv::circle(m, mouthL, r_naso, cv::Scalar(255), cv::FILLED, cv::LINE_AA);
      cv::circle(m, mouthR, r_naso, cv::Scalar(255), cv::FILLED, cv::LINE_AA);
      cv::GaussianBlur(m, m, cv::Size(0,0), r_naso*0.5);
      cv::Mat mf; m.convertTo(mf, CV_32F, 1.0/255.0);
      boost += mf * (smile_boost * smile_f);
    }
    // Consider eye corner boost from squint AND a fraction of smile
    float eff_squint = std::max(squint_f, 0.5f * smile_f);
    if (squint_boost > 0.0f && eff_squint > 0.0f) {
      cv::Mat m(frame_bgr.size(), CV_8U, cv::Scalar(0));
      cv::circle(m, eyeLO, r_crow, cv::Scalar(255), cv::FILLED, cv::LINE_AA);
      cv::circle(m, eyeRO, r_crow, cv::Scalar(255), cv::FILLED, cv::LINE_AA);
      cv::GaussianBlur(m, m, cv::Size(0,0), r_crow*0.5);
      cv::Mat mf; m.convertTo(mf, CV_32F, 1.0/255.0);
      boost += mf * (squint_boost * eff_squint);
    }
    // Optional forehead boost focusing on horizontal wrinkles
    if (forehead_boost > 0.0f && !fr.face_oval.empty()) {
      int topY = frame_bgr.rows, minEyeY = frame_bgr.rows;
      for (const auto& p : fr.face_oval) topY = std::min(topY, p.y);
      if (!fr.left_eye.empty()) for (const auto& p : fr.left_eye) minEyeY = std::min(minEyeY, p.y);
      if (!fr.right_eye.empty()) for (const auto& p : fr.right_eye) minEyeY = std::min(minEyeY, p.y);
      int cut = std::max(0, std::min(frame_bgr.rows-1, minEyeY - (int)std::round(std::max(0.0f, forehead_margin_px))));
      cv::Mat band(frame_bgr.size(), CV_8U, cv::Scalar(0));
      std::vector<std::vector<cv::Point>> polys = {fr.face_oval};
      cv::fillPoly(band, polys, cv::Scalar(255));
      cv::rectangle(band, cv::Rect(0, cut, frame_bgr.cols, frame_bgr.rows-cut), cv::Scalar(0), cv::FILLED);
      // Prefer horizontal lines: use vertical gradient magnitude on grayscale
      cv::Mat gray_fh; cv::cvtColor(frame_bgr, gray_fh, cv::COLOR_BGR2GRAY);
      cv::Mat gy; cv::Sobel(gray_fh, gy, CV_32F, 0, 1, 3);
      cv::GaussianBlur(gy, gy, cv::Size(0,0), 1.0);
      cv::Mat gy_abs = cv::abs(gy);
      double meanGy = cv::mean(gy_abs, band)[0];
      float gy_scale = (float)std::max(8.0, meanGy * 3.0 + 1e-3);
      cv::Mat gy_n; gy_abs.convertTo(gy_n, CV_32F, 1.0f/gy_scale); gy_n = cv::min(gy_n, 1.0f);
      cv::Mat band_f; band.convertTo(band_f, CV_32F, 1.0/255.0);
      // Local dark gate from negative detail
      cv::Mat dark2 = cv::max(0.0f, -detail);
      cv::GaussianBlur(dark2, dark2, cv::Size(0,0), std::max(1.0f, radius_px*0.5f));
      cv::Mat dark2n; dark2.convertTo(dark2n, CV_32F, 1.0f/0.12f); dark2n = cv::min(dark2n, 1.0f);
      cv::Mat f_boost = cv::min(1.0f, gy_n.mul(dark2n).mul(band_f) * forehead_boost);
      boost = cv::min(1.0f, boost + f_boost);
    }
    boost = cv::min(boost, 1.0f);
    // Wrinkle awareness: emphasize dark, narrow, linear structures.
    // 1) Negative detail + gradient gate (local, fast)
    cv::Mat gray_hint; cv::cvtColor(frame_bgr, gray_hint, cv::COLOR_BGR2GRAY);
    cv::Mat gx2, gy2; cv::Sobel(gray_hint, gx2, CV_32F, 1, 0, 3); cv::Sobel(gray_hint, gy2, CV_32F, 0, 1, 3);
    cv::Mat grad_mag; cv::magnitude(gx2, gy2, grad_mag);
    cv::GaussianBlur(grad_mag, grad_mag, cv::Size(0,0), std::max(1.0f, radius_px*0.5f));
    cv::Mat dark = cv::max(0.0f, -detail);
    cv::GaussianBlur(dark, dark, cv::Size(0,0), std::max(1.0f, radius_px*0.5f));
    cv::Mat dark_n; dark.convertTo(dark_n, CV_32F, 1.0f/0.12f); dark_n = cv::min(dark_n, 1.0f);
    double gm_mean = cv::mean(grad_mag)[0];
    float gm_scale = (float)std::max(8.0, gm_mean * 3.0 + 1e-3);
    cv::Mat grad_n; grad_mag.convertTo(grad_n, CV_32F, 1.0f / gm_scale); grad_n = cv::min(grad_n, 1.0f);
    cv::Mat wrinkle_local = cv::min(dark_n, grad_n);
    // 2) Line-sensitive mask (multi-scale black-hat + coherence)
    float s_min = (line_min_px > 0.0f) ? line_min_px : std::max(1.5f, radius_px * 0.5f);
    float s_max = (line_max_px > 0.0f) ? line_max_px : std::max(3.0f, radius_px * 1.25f);
    if (s_max < s_min) std::swap(s_min, s_max);
    cv::Mat wrinkle_line = BuildWrinkleLineMask(frame_bgr, fr, s_min, s_max,
                                                /*suppress_lower_face=*/suppress_lower_face,
                                                /*lower_face_ratio=*/lower_face_ratio,
                                                /*ignore_glasses=*/ignore_glasses,
                                                /*glasses_margin_px=*/glasses_margin_px,
                                                /*keep_ratio=*/keep_ratio,
                                                /*use_skin_gate=*/use_skin_gate,
                                                /*mask_gain=*/mask_gain);
    // Combine local and line masks with sensitivity: higher keep_ratio favors line mask
    float s = std::clamp(keep_ratio, 0.02f, 0.80f);
    float s_norm = (s - 0.02f) / (0.78f); // 0..1
    float w_line = 0.4f + 0.9f * s_norm;   // 0.4 .. 1.3
    float w_local = 0.6f * (1.0f - s_norm); // 0.6 .. 0
    cv::Mat wrinkle_mask = cv::min(1.0f, wrinkle_line * w_line + wrinkle_local * w_local);
    // Ensure a baseline sensitivity to wrinkles even without expression triggers
    cv::Mat boost_any = cv::min(1.0f, boost + std::max(0.0f, baseline_boost));
    // Gate boost strictly to face region to avoid affecting background
    cv::Mat face_gate_u8; cv::compare(weight, 1e-6, face_gate_u8, cv::CMP_GT); // 0 or 255
    cv::Mat face_gate; face_gate_u8.convertTo(face_gate, CV_32F, 1.0/255.0);
    cv::Mat boost_final = boost_any.mul(wrinkle_mask).mul(face_gate);
    // Increase attenuation locally using a gain; clamp at 1
    if (wrinkle_preview) {
      // In preview: show only wrinkle attenuation, no base smoothing
      atten = cv::min(1.0f, boost_gain * boost_final);
      base = Lf; // no low-pass
    } else {
      atten = cv::min(1.0f, atten + boost_gain * boost_final);
    }
  }
  // Frequency separation handling that preserves highlights/pores:
  // Split detail into positive (highlights/pores) and negative (shadows/wrinkles).
  cv::Mat detail_pos = cv::max(detail, 0.0f);
  cv::Mat detail_neg = cv::min(detail, 0.0f);
  // Use lighter attenuation for positive detail to keep sheen/pores.
  cv::Mat pos_atten = wrinkle_preview ? cv::Mat::zeros(atten.size(), CV_32F) : (weight * (amount * 0.15f));
  // Stronger attenuation on negative detail (wrinkle shadows), clamped to user cap
  float cap = std::clamp(neg_atten_cap, 0.4f, 1.0f);
  cv::Mat neg_atten = cv::min(cap, atten);
  cv::Mat outL = base + detail_pos.mul(1.0f - pos_atten) + detail_neg.mul(1.0f - neg_atten);
  outL = cv::min(cv::max(outL, 0.0f), 1.0f);
  outL.convertTo(ch[0], CV_8U, 255.0);
  cv::merge(ch, lab);
  cv::cvtColor(lab, frame_bgr, cv::COLOR_Lab2BGR);
}