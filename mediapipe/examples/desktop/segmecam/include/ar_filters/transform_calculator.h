// Copyright 2025 SegmeCam Contributors
// Licensed under the Apache License, Version 2.0

#ifndef MEDIAPIPE_EXAMPLES_DESKTOP_SEGMECAM_INCLUDE_AR_FILTERS_TRANSFORM_CALCULATOR_H_
#define MEDIAPIPE_EXAMPLES_DESKTOP_SEGMECAM_INCLUDE_AR_FILTERS_TRANSFORM_CALCULATOR_H_

#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <opencv2/core.hpp>

// Forward declaration
namespace segmecam {
struct FaceMesh;
}

namespace segmecam {

// 3D head pose with multiple representations for flexibility
struct HeadPose {
    cv::Vec3f position;          // Head center in 3D world space (x, y, z in pixels)
    cv::Vec4f rotation_quat;     // Quaternion (w, x, y, z) for smooth interpolation
    cv::Vec3f euler_angles;      // Yaw, pitch, roll in degrees (for debugging/display)
    float scale;                 // Head scale factor (1.0 = average size)
    cv::Mat rotation_matrix;     // 3x3 rotation matrix (OpenCV format)
    cv::Mat transform_matrix;    // 4x4 transformation matrix (OpenGL-compatible)
    double timestamp;            // Frame timestamp in milliseconds

    HeadPose()
        : position(0, 0, 0),
          rotation_quat(1, 0, 0, 0),  // Identity quaternion (w=1, x=y=z=0)
          euler_angles(0, 0, 0),
          scale(1.0f),
          timestamp(0.0) {
        rotation_matrix = cv::Mat::eye(3, 3, CV_32F);
        transform_matrix = cv::Mat::eye(4, 4, CV_32F);
    }
};

// Anchor point for attaching AR objects to face
struct AnchorPoint {
    std::string name;            // Identifier: "nose_bridge", "forehead", "left_ear", etc.
    cv::Vec3f position_world;    // 3D position in world space
    cv::Vec3f position_local;    // 3D position in face-local space
    cv::Point2f position_2d;     // 2D projected screen position (Phase 2 Step 4)
    cv::Vec4f orientation_quat;  // Local orientation as quaternion
    cv::Mat transform_matrix;    // 4x4 local-to-world transform matrix
    float scale;                 // Local scale factor (for asymmetric objects)
    bool is_visible;             // Visibility flag (false if occluded/out of frame)
    
    // Phase 2 Step 4: Stability tracking
    float stability;             // Stability score: 0.0 (unstable) to 1.0 (very stable)
    float variance;              // Position variance over recent frames (lower = more stable)

    AnchorPoint()
        : name("unknown"),
          position_world(0, 0, 0),
          position_local(0, 0, 0),
          position_2d(0, 0),
          orientation_quat(1, 0, 0, 0),  // Identity quaternion
          scale(1.0f),
          is_visible(false),
          stability(0.0f),
          variance(0.0f) {
        transform_matrix = cv::Mat::eye(4, 4, CV_32F);
    }
};

// Transform state for temporal filtering
struct TransformState {
    HeadPose current_pose;       // Current frame pose (unsmoothed)
    HeadPose smoothed_pose;      // Temporally smoothed pose
    std::vector<AnchorPoint> anchors;  // All anchor points
    double last_update_time;     // Timestamp of last update
    int frame_count;             // Frame counter
    
    // Phase 2 Step 4: Anchor position history for stability tracking
    std::vector<std::deque<cv::Vec3f>> anchor_position_history;  // Per-anchor position history (max 30 frames)
    static constexpr size_t kHistorySize = 30;  // Track last 30 frames (~1 second at 30 FPS)

    TransformState() : last_update_time(0.0), frame_count(0) {}
};

// Main transform calculator for AR object placement
class TransformCalculator {
public:
    TransformCalculator();
    ~TransformCalculator() = default;

    // Initialize with camera intrinsics
    // camera_matrix: 3x3 intrinsic matrix (fx, fy, cx, cy)
    // frame_width: Width of input frames in pixels
    // frame_height: Height of input frames in pixels
    void Initialize(const cv::Mat& camera_matrix, int frame_width, int frame_height);

    // Update pose and anchors from face mesh
    // face_mesh: Face mesh data from FaceMeshProcessor (Phase 1)
    void Update(const FaceMesh& face_mesh);

    // Get current head pose (smoothed if enabled)
    const HeadPose& GetHeadPose() const;

    // Get specific anchor point by name
    // Returns null anchor if not found
    const AnchorPoint& GetAnchor(const std::string& name) const;

    // Get all anchor points
    const std::vector<AnchorPoint>& GetAnchors() const;

    // Check if transform data is available
    bool IsAvailable() const { return data_available_; }

    // Smoothing controls
    void SetSmoothingEnabled(bool enabled) { smoothing_enabled_ = enabled; }
    bool IsSmoothingEnabled() const { return smoothing_enabled_; }
    void SetSmoothingAlpha(float alpha);  // Range: 0.0 (no smoothing) to 1.0 (max smoothing)
    float GetSmoothingAlpha() const { return smoothing_alpha_; }

    // Reset state (e.g., when face tracking lost)
    void Reset();

private:
    // Core transformation methods
    HeadPose CalculatePose(const FaceMesh& face_mesh);
    std::vector<AnchorPoint> CalculateAnchors(const HeadPose& pose, const FaceMesh& face_mesh);

    // Coordinate transformations
    cv::Vec4f EulerToQuaternion(const cv::Vec3f& euler_degrees);
    cv::Vec3f QuaternionToEuler(const cv::Vec4f& quat);
    cv::Mat QuaternionToRotationMatrix(const cv::Vec4f& quat);
    cv::Mat BuildTransformMatrix(const cv::Vec3f& position, const cv::Vec4f& rotation, float scale);

    // Quaternion operations
    cv::Vec4f NormalizeQuaternion(const cv::Vec4f& quat);
    cv::Vec4f SlerpQuaternions(const cv::Vec4f& q1, const cv::Vec4f& q2, float t);
    float QuaternionDot(const cv::Vec4f& q1, const cv::Vec4f& q2);

    // Smoothing
    HeadPose SmoothPose(const HeadPose& current, const HeadPose& target);
    cv::Vec3f SmoothVector(const cv::Vec3f& current, const cv::Vec3f& target, float alpha);

    // Anchor-specific calculations
    AnchorPoint CalculateNoseBridge(const HeadPose& pose, const FaceMesh& face_mesh);
    AnchorPoint CalculateForehead(const HeadPose& pose, const FaceMesh& face_mesh);
    AnchorPoint CalculateLeftEar(const HeadPose& pose, const FaceMesh& face_mesh);
    AnchorPoint CalculateRightEar(const HeadPose& pose, const FaceMesh& face_mesh);
    AnchorPoint CalculateChin(const HeadPose& pose, const FaceMesh& face_mesh);
    AnchorPoint CalculateLeftTemple(const HeadPose& pose, const FaceMesh& face_mesh);
    AnchorPoint CalculateRightTemple(const HeadPose& pose, const FaceMesh& face_mesh);
    
    // Phase 2 Step 4: Stability tracking
    void UpdateStabilityTracking(std::vector<AnchorPoint>& anchors);
    float CalculateVariance(const std::deque<cv::Vec3f>& position_history);
    float VarianceToStability(float variance);
    void ProjectAnchorTo2D(AnchorPoint& anchor, const FaceMesh& face_mesh);

    // Utility methods
    cv::Vec3f CalculateHeadCenter(const FaceMesh& face_mesh);
    cv::Vec3f CalculateFaceNormal(const FaceMesh& face_mesh);
    cv::Vec4f DirectionToQuaternion(const cv::Vec3f& forward_dir);
    bool IsWithinFrame(const cv::Vec3f& position) const;
    double GetCurrentTimeMs() const;

    // State
    TransformState state_;
    cv::Mat camera_matrix_;
    int frame_width_;
    int frame_height_;
    bool smoothing_enabled_;
    float smoothing_alpha_;
    bool data_available_;
    mutable std::mutex state_mutex_;

    // Null anchor for error cases
    AnchorPoint null_anchor_;
};

}  // namespace segmecam

#endif  // MEDIAPIPE_EXAMPLES_DESKTOP_SEGMECAM_INCLUDE_AR_FILTERS_TRANSFORM_CALCULATOR_H_
