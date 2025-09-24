#pragma once

#include <opencv2/opencv.hpp>
#include "mediapipe/framework/formats/landmark.pb.h"
#include "effects/segmecam_face_effects.h"

namespace segmecam {

/**
 * Face Processing Utilities Module
 *
 * Handles face region extraction, landmark processing, and face-related utilities.
 */
class FaceProcessor {
public:
    FaceProcessor();
    ~FaceProcessor();

    // Face region extraction
    FaceRegions ExtractFaceRegionsFromLandmarks(const mediapipe::NormalizedLandmarkList& landmarks,
                                               const cv::Size& frame_size);

    // Landmark visualization
    void DrawLandmarks(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks);
    cv::Mat VisualizeMask(const cv::Mat& mask);

    // Utility methods
    cv::Scalar ConvertRGBColorToBGR(float r, float g, float b);

private:
    // Helper methods for landmark drawing
    template<size_t N>
    void DrawConnections(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks,
                        const std::array<std::array<int, 2>, N>& connections, const cv::Scalar& color);
};

} // namespace segmecam