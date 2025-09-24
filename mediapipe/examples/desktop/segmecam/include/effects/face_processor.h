#ifndef FACE_PROCESSOR_H
#define FACE_PROCESSOR_H

#include "include/effects/face_regions.h"
#include "presets.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include <opencv2/core.hpp>

namespace segmecam {

class FaceProcessor {
public:
    FaceProcessor();
    ~FaceProcessor();

    // Face region extraction
    FaceRegions ExtractFaceRegionsFromLandmarks(const mediapipe::NormalizedLandmarkList& landmarks,
                                               const cv::Size& frame_size);

    // Landmark drawing
    void DrawLandmarks(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks);
    void DrawMesh(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks, bool dense = false);

    // Advanced skin smoothing with processing scale
    void ApplySkinSmoothingWithProcessingScale(cv::Mat& frame_bgr, const FaceRegions& regions,
                                             const mediapipe::NormalizedLandmarkList& landmarks,
                                             const BeautyState& beauty_state);

private:
    // Helper methods for processing scale optimization
    cv::Rect CalculateProcessingROI(const FaceRegions& regions, const cv::Size& frame_size);
    void ApplyFullResolutionSkinSmoothing(cv::Mat& frame_bgr, const FaceRegions& regions,
                                        const mediapipe::NormalizedLandmarkList& landmarks,
                                        const BeautyState& beauty_state);
    FaceRegions TransformFaceRegionsToScaledROI(const FaceRegions& regions, const cv::Rect& roi);
    FaceRegions ShiftFaceRegionsToROI(const FaceRegions& regions, const cv::Rect& roi);
    FaceRegions ScaleFaceRegions(const FaceRegions& regions, float scale);
    std::vector<cv::Point> ShiftPolygon(const std::vector<cv::Point>& poly, int dx, int dy);
    std::vector<cv::Point> ScalePolygon(const std::vector<cv::Point>& poly, float scale);
    mediapipe::NormalizedLandmarkList TransformLandmarksToROI(const mediapipe::NormalizedLandmarkList& landmarks,
                                                            const cv::Rect& roi, const cv::Size& frame_size);
    void ProcessAndUpsampleROI(cv::Mat& frame_bgr, const cv::Rect& roi, const FaceRegions& fr_small,
                             const mediapipe::NormalizedLandmarkList& lms_roi, const BeautyState& beauty_state, float scale);
    cv::Mat DownscaleROI(const cv::Mat& roi_bgr, float scale);
    void ApplySkinSmoothingToScaledImage(cv::Mat& small, const FaceRegions& fr_small,
                                       const mediapipe::NormalizedLandmarkList& lms_roi,
                                       const BeautyState& beauty_state, float scale);
    cv::Mat UpsampleProcessedImage(const cv::Mat& small, const cv::Size& target_size);
    void ApplyDetailPreservationIfNeeded(cv::Mat& up, const cv::Mat& roi_bgr, const FaceRegions& fr_roi,
                                       const BeautyState& beauty_state);
    void ApplyDetailPreservation(cv::Mat& up, const cv::Mat& roi_bgr, const FaceRegions& fr_roi, float dp);
    cv::Mat CreateFaceMask(const FaceRegions& fr_roi, const cv::Size& size);

    // Template helper for drawing connections
    template<size_t N>
    void DrawConnections(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks,
                        const std::array<std::array<int, 2>, N>& connections, const cv::Scalar& color);
};

} // namespace segmecam

#endif // FACE_PROCESSOR_H