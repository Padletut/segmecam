#pragma once

#include "mediapipe/framework/port/opencv_core_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include <vector>
#include "mediapipe/framework/formats/landmark.pb.h"

// Include modular face effects headers
#include "face_regions.h"
#include "lip_effects.h"
#include "teeth_effects.h"
#include "skin_effects.h"
#include "advanced_skin_effects.h"

// Legacy function for backward compatibility - now delegates to modular functions
cv::Mat BuildWrinkleBoostMap(const mediapipe::NormalizedLandmarkList& lms,
                             const cv::Size& size,
                             float smile_boost,
                             float squint_boost);
