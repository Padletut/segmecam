#include "include/effects/face_regions.h"
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