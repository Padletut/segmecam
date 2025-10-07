#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include "mediapipe/framework/formats/landmark.pb.h"

// Forward declaration for NormalizedLandmarkList - now included above
// namespace mediapipe {
// class NormalizedLandmarkList;
// }

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