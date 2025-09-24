#include "effects/face/face_processor.h"
#include "render/segmecam_composite.h"
#include "mediapipe/tasks/cc/vision/face_landmarker/face_landmarks_connections.h"

namespace segmecam {

FaceProcessor::FaceProcessor() {
    // Constructor - no initialization needed
}

FaceProcessor::~FaceProcessor() {
    // Destructor - no cleanup needed
}

FaceRegions FaceProcessor::ExtractFaceRegionsFromLandmarks(const mediapipe::NormalizedLandmarkList& landmarks,
                                                          const cv::Size& frame_size) {
    FaceRegions regions;
    ExtractFaceRegions(landmarks, frame_size, &regions, false, false, false);
    return regions;
}

cv::Mat FaceProcessor::VisualizeMask(const cv::Mat& mask) {
    return VisualizeMaskRGB(mask);
}

cv::Scalar FaceProcessor::ConvertRGBColorToBGR(float r, float g, float b) {
    return cv::Scalar(
        std::clamp(b * 255.0f, 0.0f, 255.0f),
        std::clamp(g * 255.0f, 0.0f, 255.0f),
        std::clamp(r * 255.0f, 0.0f, 255.0f)
    );
}

void FaceProcessor::DrawLandmarks(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks) {
    int W = frame_bgr.cols;
    int H = frame_bgr.rows;
    const int n = landmarks.landmark_size();

    // Draw individual landmark points
    for (int i = 0; i < n; ++i) {
        const auto& p = landmarks.landmark(i);
        float nx = p.x();
        float ny = p.y();
        int x = std::max(0, std::min(W - 1, (int)std::round(nx * W)));
        int y = std::max(0, std::min(H - 1, (int)std::round(ny * H)));
        cv::circle(frame_bgr, cv::Point(x, y), 1, cv::Scalar(0, 255, 0), cv::FILLED, cv::LINE_AA);
    }

    // Draw connections for different face parts
    using Conn = mediapipe::tasks::vision::face_landmarker::FaceLandmarksConnections;
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksLips, cv::Scalar(0, 128, 255));
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksFaceOval, cv::Scalar(0, 200, 255));
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksLeftEye, cv::Scalar(255, 200, 80));
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksRightEye, cv::Scalar(255, 200, 80));
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksLeftEyeBrow, cv::Scalar(180, 180, 255));
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksRightEyeBrow, cv::Scalar(180, 180, 255));
}

template<size_t N>
void FaceProcessor::DrawConnections(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks,
                                   const std::array<std::array<int, 2>, N>& connections, const cv::Scalar& color) {
    int W = frame_bgr.cols;
    int H = frame_bgr.rows;
    const int n = landmarks.landmark_size();

    for (const auto& e : connections) {
        if (e[0] >= n || e[1] >= n) continue; // Safety check
        const auto& pa = landmarks.landmark(e[0]);
        const auto& pb = landmarks.landmark(e[1]);

        cv::Point pt_a((int)std::round(pa.x() * W), (int)std::round(pa.y() * H));
        cv::Point pt_b((int)std::round(pb.x() * W), (int)std::round(pb.y() * H));

        cv::line(frame_bgr, pt_a, pt_b, color, 1, cv::LINE_AA);
    }
}

} // namespace segmecam