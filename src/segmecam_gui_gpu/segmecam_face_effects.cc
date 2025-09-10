#include "segmecam_face_effects.h"
#include <cmath>

namespace {
// MediaPipe Face Mesh landmark indices (subset)
// Commonly used sets sourced from public examples.
const int FACE_OVAL_IDX[] = {10,338,297,332,284,251,389,356,454,323,361,288,397,365,379,378,400,377,152,148,176,149,150,136,172,58,132,93,234,127,162,21,54,103,67,109};
const int LIPS_OUTER_IDX[] = {61,146,91,181,84,17,314,405,321,375,291,61};
const int LIPS_INNER_IDX[] = {78,95,88,178,87,14,317,402,318,324,308,78};
const int LEFT_EYE_IDX[]   = {33,7,163,144,145,153,154,155,133,173,157,158,159,160,161,246};
const int RIGHT_EYE_IDX[]  = {263,249,390,373,374,380,381,382,362,398,384,385,386,387,388,466};

template <size_t N>
static std::vector<cv::Point> polyFromIdx(const mediapipe::NormalizedLandmarkList& lms,
                                          const cv::Size& sz,
                                          const int (&idx)[N],
                                          bool flip_x,
                                          bool flip_y,
                                          bool swap_xy) {
  std::vector<cv::Point> poly; poly.reserve(N);
  const int W = sz.width, H = sz.height;
  const int count = static_cast<int>(lms.landmark_size());
  for (size_t i = 0; i < N; ++i) {
    int k = idx[i];
    if (k < 0 || k >= count) continue;
    const auto& p = lms.landmark(k);
    float nx = p.x(); float ny = p.y();
    if (swap_xy) { std::swap(nx, ny); }
    if (flip_x) nx = 1.0f - nx;
    if (flip_y) ny = 1.0f - ny;
    int x = std::clamp(static_cast<int>(std::round(nx * W)), 0, W - 1);
    int y = std::clamp(static_cast<int>(std::round(ny * H)), 0, H - 1);
    poly.emplace_back(x, y);
  }
  return poly;
}

static void orderAsConvexHull(std::vector<cv::Point>& pts) {
  if (pts.size() < 4) return;
  std::vector<int> hull_idx;
  cv::convexHull(pts, hull_idx, /*clockwise=*/false, /*returnPoints=*/false);
  std::vector<cv::Point> hull; hull.reserve(hull_idx.size());
  for (int i : hull_idx) hull.push_back(pts[i]);
  pts.swap(hull);
}

static void featherMask(cv::Mat& mask, int ksize) {
  if (ksize <= 1) return;
  int k = (ksize | 1);
  cv::GaussianBlur(mask, mask, cv::Size(k, k), 0);
}
} // namespace

bool ExtractFaceRegions(const mediapipe::NormalizedLandmarkList& lms,
                        const cv::Size& frame_size,
                        FaceRegions* out,
                        bool flip_x,
                        bool flip_y,
                        bool swap_xy) {
  if (!out) return false;
  if (lms.landmark_size() < 200) return false;
  out->face_oval = polyFromIdx(lms, frame_size, FACE_OVAL_IDX, flip_x, flip_y, swap_xy);
  out->lips_outer = polyFromIdx(lms, frame_size, LIPS_OUTER_IDX, flip_x, flip_y, swap_xy);
  out->lips_inner = polyFromIdx(lms, frame_size, LIPS_INNER_IDX, flip_x, flip_y, swap_xy);
  // Only enforce hull on face oval; keep lips in ring order for fidelity
  orderAsConvexHull(out->face_oval);
  out->left_eye = polyFromIdx(lms, frame_size, LEFT_EYE_IDX, flip_x, flip_y, swap_xy);
  out->right_eye = polyFromIdx(lms, frame_size, RIGHT_EYE_IDX, flip_x, flip_y, swap_xy);
  return !(out->face_oval.empty() || out->lips_outer.empty());
}

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

void ApplyTeethWhitenBGR(cv::Mat& frame_bgr,
                         const FaceRegions& fr,
                         float strength,
                         float shrink_px) {
  strength = std::clamp(strength, 0.0f, 1.0f);
  if (strength <= 0.0f || fr.lips_inner.empty()) return;
  cv::Mat mask(frame_bgr.size(), CV_8UC1, cv::Scalar(0));
  cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{fr.lips_inner}, cv::Scalar(255));
  // Erode to avoid lips bleed into whitening
  if (shrink_px > 0.5f) {
    int k = std::max(1, (int)std::round(shrink_px));
    cv::Mat ker = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
    cv::erode(mask, mask, ker);
  }
  featherMask(mask, 5);
  // Convert to LAB and nudge b* toward blue (reduce yellow), slight L* increase.
  cv::Mat lab; cv::cvtColor(frame_bgr, lab, cv::COLOR_BGR2Lab);
  std::vector<cv::Mat> ch; cv::split(lab, ch);
  // ch[0]=L 0..255, ch[1]=a 0..255, ch[2]=b 0..255 (128 is neutral)
  // Move b toward 128 by factor, and slightly increase L.
  const float k = 0.35f * strength;
  const float kL = 0.15f * strength;
  ch[2].forEach<uchar>([&](uchar &pix, const int pos[]) {
    int y = pos[0], x = pos[1];
    if (mask.at<uchar>(y, x) == 0) return;
    float b = pix;
    b = 128.0f + (b - 128.0f) * (1.0f - k);
    pix = (uchar)std::clamp(b, 0.0f, 255.0f);
  });
  ch[0].forEach<uchar>([&](uchar &pix, const int pos[]) {
    int y = pos[0], x = pos[1];
    if (mask.at<uchar>(y, x) == 0) return;
    float L = pix * (1.0f + kL);
    pix = (uchar)std::clamp(L, 0.0f, 255.0f);
  });
  cv::merge(ch, lab);
  cv::cvtColor(lab, frame_bgr, cv::COLOR_Lab2BGR);
}

void ApplySkinSmoothingBGR(cv::Mat& frame_bgr,
                           const FaceRegions& fr,
                           float strength,
                           bool use_ocl) {
  strength = std::clamp(strength, 0.0f, 1.0f);
  if (strength <= 0.0f || fr.face_oval.empty()) return;
  cv::Mat mask(frame_bgr.size(), CV_8UC1, cv::Scalar(0));
  std::vector<std::vector<cv::Point>> face = {fr.face_oval};
  cv::fillPoly(mask, face, cv::Scalar(220));
  if (!fr.lips_outer.empty()) {
    std::vector<std::vector<cv::Point>> lips = {fr.lips_outer};
    cv::fillPoly(mask, lips, cv::Scalar(0));
  }
  if (!fr.left_eye.empty()) {
    std::vector<std::vector<cv::Point>> e = {fr.left_eye};
    cv::fillPoly(mask, e, cv::Scalar(0));
  }
  if (!fr.right_eye.empty()) {
    std::vector<std::vector<cv::Point>> e = {fr.right_eye};
    cv::fillPoly(mask, e, cv::Scalar(0));
  }
  featherMask(mask, 15);
  // Bilateral filter strength mapping
  int d = 9;
  double sigmaColor = 25.0 + 75.0 * strength;
  double sigmaSpace = 9.0 + 21.0 * strength;
  if (!use_ocl) {
    cv::Mat smooth; cv::bilateralFilter(frame_bgr, smooth, d, sigmaColor, sigmaSpace);
    // Blend only where mask applies (do math in float)
    cv::Mat mask_f; mask.convertTo(mask_f, CV_32FC1, 1.0/255.0);
    std::vector<cv::Mat> fch(3), sch(3), of(3);
    cv::split(frame_bgr, fch); cv::split(smooth, sch);
    for (int i = 0; i < 3; ++i) {
      cv::Mat f32, s32;
      fch[i].convertTo(f32, CV_32FC1, 1.0/255.0);
      sch[i].convertTo(s32, CV_32FC1, 1.0/255.0);
      of[i] = f32.mul(1.0f - mask_f) + s32.mul(mask_f);
    }
    cv::Mat comp_f; cv::merge(of, comp_f);
    comp_f.convertTo(frame_bgr, CV_8UC3, 255.0);
  } else {
    // OpenCL path using Transparent API (UMat).
    cv::UMat src; frame_bgr.copyTo(src);
    cv::UMat smooth_u; cv::bilateralFilter(src, smooth_u, d, sigmaColor, sigmaSpace);
    cv::UMat mask_u; mask.copyTo(mask_u);
    cv::UMat mask_f; mask_u.convertTo(mask_f, CV_32FC1, 1.0/255.0);
    std::vector<cv::UMat> fch(3), sch(3), of(3);
    cv::split(src, fch); cv::split(smooth_u, sch);
    for (int i = 0; i < 3; ++i) {
      cv::UMat f32, s32;
      fch[i].convertTo(f32, CV_32FC1, 1.0/255.0);
      sch[i].convertTo(s32, CV_32FC1, 1.0/255.0);
      // out = f*(1-mask) + s*mask
      of[i] = f32.mul(1.0f - mask_f) + s32.mul(mask_f);
    }
    cv::UMat comp_f; cv::merge(of, comp_f);
    cv::UMat comp_u8; comp_f.convertTo(comp_u8, CV_8UC3, 255.0);
    comp_u8.copyTo(frame_bgr);
  }
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
