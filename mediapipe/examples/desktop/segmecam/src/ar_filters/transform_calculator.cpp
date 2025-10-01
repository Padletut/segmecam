// Copyright 2025 SegmeCam Contributors
// Licensed under the Apache License, Version 2.0

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/transform_calculator.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/face_mesh_processor.h"

#include <chrono>
#include <cmath>
#include <algorithm>
#include <opencv2/calib3d.hpp>
#include <opencv2/core/types.hpp>

namespace segmecam {

// Constants
constexpr float kDefaultSmoothingAlpha = 0.3f;  // 0.3 = moderate smoothing
constexpr float kMinSmoothingAlpha = 0.0f;
constexpr float kMaxSmoothingAlpha = 0.95f;
constexpr double kPI = 3.14159265358979323846;

// Constructor
TransformCalculator::TransformCalculator()
    : frame_width_(0),
      frame_height_(0),
      smoothing_enabled_(true),
      smoothing_alpha_(kDefaultSmoothingAlpha),
      data_available_(false) {
    camera_matrix_ = cv::Mat::eye(3, 3, CV_64F);
}

// Initialize with camera intrinsics
void TransformCalculator::Initialize(const cv::Mat& camera_matrix, int frame_width, int frame_height) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    camera_matrix_ = camera_matrix.clone();
    frame_width_ = frame_width;
    frame_height_ = frame_height;
    
    Reset();
    
    std::cout << "🎯 TransformCalculator initialized: " << frame_width << "x" << frame_height << std::endl;
}

// Update pose and anchors from face mesh
void TransformCalculator::Update(const FaceMesh& face_mesh) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    // Calculate current pose
    HeadPose current_pose = CalculatePose(face_mesh);
    
    // Apply smoothing if enabled and we have previous data
    if (smoothing_enabled_ && state_.frame_count > 0) {
        state_.smoothed_pose = SmoothPose(state_.smoothed_pose, current_pose);
    } else {
        state_.smoothed_pose = current_pose;
    }
    
    state_.current_pose = current_pose;
    
    // Calculate anchor points using smoothed pose
    state_.anchors = CalculateAnchors(state_.smoothed_pose, face_mesh);
    
    // Update metadata
    state_.last_update_time = GetCurrentTimeMs();
    state_.frame_count++;
    data_available_ = true;
}

// Get current head pose (smoothed if enabled)
const HeadPose& TransformCalculator::GetHeadPose() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_.smoothed_pose;
}

// Get specific anchor point by name
const AnchorPoint& TransformCalculator::GetAnchor(const std::string& name) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    for (const auto& anchor : state_.anchors) {
        if (anchor.name == name) {
            return anchor;
        }
    }
    
    return null_anchor_;  // Not found
}

// Get all anchor points
const std::vector<AnchorPoint>& TransformCalculator::GetAnchors() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_.anchors;
}

// Set smoothing alpha with range clamping
void TransformCalculator::SetSmoothingAlpha(float alpha) {
    smoothing_alpha_ = std::clamp(alpha, kMinSmoothingAlpha, kMaxSmoothingAlpha);
}

// Reset state
void TransformCalculator::Reset() {
    state_ = TransformState();
    data_available_ = false;
}

// =============================================================================
// Core Transformation Methods
// =============================================================================

// Calculate head pose from face mesh
HeadPose TransformCalculator::CalculatePose(const FaceMesh& face_mesh) {
    HeadPose pose;
    
    // 1. Use Phase 1's Euler angles as starting point
    pose.euler_angles = cv::Vec3f(
        face_mesh.pose.yaw,
        face_mesh.pose.pitch,
        face_mesh.pose.roll
    );
    
    // 2. Convert to quaternion for smooth interpolation
    pose.rotation_quat = EulerToQuaternion(pose.euler_angles);
    
    // 3. Build rotation matrix from quaternion
    pose.rotation_matrix = QuaternionToRotationMatrix(pose.rotation_quat);
    
    // 4. Calculate head center position
    pose.position = CalculateHeadCenter(face_mesh);
    
    // 5. Get scale from face mesh
    pose.scale = face_mesh.scale;
    
    // 6. Build 4x4 transform matrix for OpenGL
    pose.transform_matrix = BuildTransformMatrix(pose.position, pose.rotation_quat, pose.scale);
    
    // 7. Timestamp
    pose.timestamp = GetCurrentTimeMs();
    
    return pose;
}

// Calculate all anchor points
std::vector<AnchorPoint> TransformCalculator::CalculateAnchors(const HeadPose& pose, const FaceMesh& face_mesh) {
    std::vector<AnchorPoint> anchors;
    anchors.reserve(7);
    
    // Calculate each anchor point
    anchors.push_back(CalculateNoseBridge(pose, face_mesh));
    anchors.push_back(CalculateForehead(pose, face_mesh));
    anchors.push_back(CalculateLeftEar(pose, face_mesh));
    anchors.push_back(CalculateRightEar(pose, face_mesh));
    anchors.push_back(CalculateChin(pose, face_mesh));
    anchors.push_back(CalculateLeftTemple(pose, face_mesh));
    anchors.push_back(CalculateRightTemple(pose, face_mesh));
    
    return anchors;
}

// =============================================================================
// Coordinate Transformations
// =============================================================================

// Convert Euler angles (degrees) to quaternion
cv::Vec4f TransformCalculator::EulerToQuaternion(const cv::Vec3f& euler_degrees) {
    // Convert degrees to radians
    float yaw_rad = euler_degrees[0] * kPI / 180.0f;
    float pitch_rad = euler_degrees[1] * kPI / 180.0f;
    float roll_rad = euler_degrees[2] * kPI / 180.0f;
    
    // Half angles
    float cy = std::cos(yaw_rad * 0.5f);
    float sy = std::sin(yaw_rad * 0.5f);
    float cp = std::cos(pitch_rad * 0.5f);
    float sp = std::sin(pitch_rad * 0.5f);
    float cr = std::cos(roll_rad * 0.5f);
    float sr = std::sin(roll_rad * 0.5f);
    
    // Quaternion multiplication (yaw * pitch * roll)
    cv::Vec4f quat;
    quat[0] = cr * cp * cy + sr * sp * sy;  // w
    quat[1] = sr * cp * cy - cr * sp * sy;  // x
    quat[2] = cr * sp * cy + sr * cp * sy;  // y
    quat[3] = cr * cp * sy - sr * sp * cy;  // z
    
    return NormalizeQuaternion(quat);
}

// Convert quaternion to Euler angles (degrees)
cv::Vec3f TransformCalculator::QuaternionToEuler(const cv::Vec4f& quat) {
    float w = quat[0], x = quat[1], y = quat[2], z = quat[3];
    
    // Roll (x-axis rotation)
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    float roll = std::atan2(sinr_cosp, cosr_cosp);
    
    // Pitch (y-axis rotation)
    float sinp = 2.0f * (w * y - z * x);
    float pitch;
    if (std::abs(sinp) >= 1.0f) {
        pitch = std::copysign(kPI / 2.0f, sinp);  // Use 90 degrees if out of range
    } else {
        pitch = std::asin(sinp);
    }
    
    // Yaw (z-axis rotation)
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    float yaw = std::atan2(siny_cosp, cosy_cosp);
    
    // Convert to degrees
    return cv::Vec3f(
        yaw * 180.0f / kPI,
        pitch * 180.0f / kPI,
        roll * 180.0f / kPI
    );
}

// Convert quaternion to 3x3 rotation matrix
cv::Mat TransformCalculator::QuaternionToRotationMatrix(const cv::Vec4f& quat) {
    float w = quat[0], x = quat[1], y = quat[2], z = quat[3];
    
    cv::Mat R = cv::Mat::eye(3, 3, CV_32F);
    
    // First row
    R.at<float>(0, 0) = 1.0f - 2.0f * (y * y + z * z);
    R.at<float>(0, 1) = 2.0f * (x * y - w * z);
    R.at<float>(0, 2) = 2.0f * (x * z + w * y);
    
    // Second row
    R.at<float>(1, 0) = 2.0f * (x * y + w * z);
    R.at<float>(1, 1) = 1.0f - 2.0f * (x * x + z * z);
    R.at<float>(1, 2) = 2.0f * (y * z - w * x);
    
    // Third row
    R.at<float>(2, 0) = 2.0f * (x * z - w * y);
    R.at<float>(2, 1) = 2.0f * (y * z + w * x);
    R.at<float>(2, 2) = 1.0f - 2.0f * (x * x + y * y);
    
    return R;
}

// Build 4x4 transform matrix (OpenGL format, column-major)
cv::Mat TransformCalculator::BuildTransformMatrix(const cv::Vec3f& position, const cv::Vec4f& rotation, float scale) {
    cv::Mat T = cv::Mat::eye(4, 4, CV_32F);
    
    // Get rotation matrix
    cv::Mat R = QuaternionToRotationMatrix(rotation);
    
    // Apply scale and rotation to upper-left 3x3
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            T.at<float>(i, j) = R.at<float>(i, j) * scale;
        }
    }
    
    // Set translation (last column)
    T.at<float>(0, 3) = position[0];
    T.at<float>(1, 3) = position[1];
    T.at<float>(2, 3) = position[2];
    
    return T;
}

// =============================================================================
// Quaternion Operations
// =============================================================================

// Normalize quaternion to unit length
cv::Vec4f TransformCalculator::NormalizeQuaternion(const cv::Vec4f& quat) {
    float length = std::sqrt(quat[0] * quat[0] + quat[1] * quat[1] + 
                             quat[2] * quat[2] + quat[3] * quat[3]);
    
    if (length < 1e-6f) {
        return cv::Vec4f(1, 0, 0, 0);  // Identity quaternion
    }
    
    return quat / length;
}

// Spherical linear interpolation between two quaternions
cv::Vec4f TransformCalculator::SlerpQuaternions(const cv::Vec4f& q1, const cv::Vec4f& q2, float t) {
    // Compute dot product
    float dot = QuaternionDot(q1, q2);
    
    // If negative, negate one quaternion to take shorter path
    cv::Vec4f q2_adjusted = (dot < 0) ? -q2 : q2;
    dot = std::abs(dot);
    
    // If very close, use linear interpolation
    if (dot > 0.9995f) {
        cv::Vec4f result = q1 * (1.0f - t) + q2_adjusted * t;
        return NormalizeQuaternion(result);
    }
    
    // SLERP formula
    float theta = std::acos(dot);
    float sin_theta = std::sin(theta);
    float w1 = std::sin((1.0f - t) * theta) / sin_theta;
    float w2 = std::sin(t * theta) / sin_theta;
    
    return q1 * w1 + q2_adjusted * w2;
}

// Quaternion dot product
float TransformCalculator::QuaternionDot(const cv::Vec4f& q1, const cv::Vec4f& q2) {
    return q1[0] * q2[0] + q1[1] * q2[1] + q1[2] * q2[2] + q1[3] * q2[3];
}

// =============================================================================
// Smoothing
// =============================================================================

// Smooth head pose using exponential moving average and SLERP
HeadPose TransformCalculator::SmoothPose(const HeadPose& current, const HeadPose& target) {
    HeadPose smoothed;
    
    float alpha = smoothing_alpha_;
    
    // 1. Smooth position (EMA)
    smoothed.position = SmoothVector(current.position, target.position, alpha);
    
    // 2. Smooth rotation (SLERP)
    smoothed.rotation_quat = SlerpQuaternions(current.rotation_quat, target.rotation_quat, alpha);
    
    // 3. Smooth scale (EMA)
    smoothed.scale = current.scale * (1.0f - alpha) + target.scale * alpha;
    
    // 4. Rebuild derived data
    smoothed.euler_angles = QuaternionToEuler(smoothed.rotation_quat);
    smoothed.rotation_matrix = QuaternionToRotationMatrix(smoothed.rotation_quat);
    smoothed.transform_matrix = BuildTransformMatrix(smoothed.position, smoothed.rotation_quat, smoothed.scale);
    
    smoothed.timestamp = target.timestamp;
    
    return smoothed;
}

// Smooth 3D vector using exponential moving average
cv::Vec3f TransformCalculator::SmoothVector(const cv::Vec3f& current, const cv::Vec3f& target, float alpha) {
    return current * (1.0f - alpha) + target * alpha;
}

// =============================================================================
// Anchor Calculations (Continued in next part...)
// =============================================================================

// Calculate nose bridge anchor (for glasses, sunglasses)
AnchorPoint TransformCalculator::CalculateNoseBridge(const HeadPose& pose, const FaceMesh& face_mesh) {
    AnchorPoint anchor;
    anchor.name = "nose_bridge";
    
    // Average of landmarks 6, 197, 195 (nose bridge area)
    if (face_mesh.landmarks_pixel.size() > 195) {
        cv::Point2f p1 = face_mesh.landmarks_pixel[6];
        cv::Point2f p2 = face_mesh.landmarks_pixel[197];
        cv::Point2f p3 = face_mesh.landmarks_pixel[195];
        
        anchor.position_local = cv::Vec3f(
            (p1.x + p2.x + p3.x) / 3.0f,
            (p1.y + p2.y + p3.y) / 3.0f,
            0.0f  // Z will be estimated from face depth
        );
        
        // Transform to world space
        anchor.position_world = anchor.position_local;  // Simplified for now
        
        // Calculate forward-facing orientation
        cv::Vec3f forward = CalculateFaceNormal(face_mesh);
        anchor.orientation_quat = DirectionToQuaternion(forward);
        
        // Build transform matrix
        anchor.transform_matrix = BuildTransformMatrix(
            anchor.position_world,
            anchor.orientation_quat,
            pose.scale
        );
        
        // Check visibility
        anchor.is_visible = IsWithinFrame(anchor.position_world);
        anchor.scale = pose.scale;
    }
    
    return anchor;
}

// Calculate forehead anchor (for hats, crowns, tiaras)
AnchorPoint TransformCalculator::CalculateForehead(const HeadPose& pose, const FaceMesh& face_mesh) {
    AnchorPoint anchor;
    anchor.name = "forehead";
    
    // Landmark 10 is forehead center, with nearby landmarks for orientation
    if (face_mesh.landmarks_pixel.size() > 297) {
        cv::Point2f center = face_mesh.landmarks_pixel[10];
        cv::Point2f left = face_mesh.landmarks_pixel[67];
        cv::Point2f right = face_mesh.landmarks_pixel[297];
        
        anchor.position_local = cv::Vec3f(center.x, center.y, 0.0f);
        anchor.position_world = anchor.position_local;
        
        // Upward-facing orientation
        cv::Vec3f up(0, -1, 0);  // Negative Y is up in image coordinates
        anchor.orientation_quat = DirectionToQuaternion(up);
        
        anchor.transform_matrix = BuildTransformMatrix(
            anchor.position_world,
            anchor.orientation_quat,
            pose.scale
        );
        
        anchor.is_visible = IsWithinFrame(anchor.position_world);
        anchor.scale = pose.scale;
    }
    
    return anchor;
}

// Calculate left ear anchor (for earrings, headphones)
AnchorPoint TransformCalculator::CalculateLeftEar(const HeadPose& pose, const FaceMesh& face_mesh) {
    AnchorPoint anchor;
    anchor.name = "left_ear";
    
    // Landmarks 234, 127, 162 define left ear area
    if (face_mesh.landmarks_pixel.size() > 234) {
        cv::Point2f p1 = face_mesh.landmarks_pixel[234];
        cv::Point2f p2 = face_mesh.landmarks_pixel[127];
        cv::Point2f p3 = face_mesh.landmarks_pixel[162];
        
        anchor.position_local = cv::Vec3f(
            (p1.x + p2.x + p3.x) / 3.0f,
            (p1.y + p2.y + p3.y) / 3.0f,
            0.0f
        );
        
        anchor.position_world = anchor.position_local;
        
        // Left-facing orientation
        cv::Vec3f left(-1, 0, 0);
        anchor.orientation_quat = DirectionToQuaternion(left);
        
        anchor.transform_matrix = BuildTransformMatrix(
            anchor.position_world,
            anchor.orientation_quat,
            pose.scale
        );
        
        anchor.is_visible = IsWithinFrame(anchor.position_world);
        anchor.scale = pose.scale;
    }
    
    return anchor;
}

// Calculate right ear anchor (for earrings, headphones)
AnchorPoint TransformCalculator::CalculateRightEar(const HeadPose& pose, const FaceMesh& face_mesh) {
    AnchorPoint anchor;
    anchor.name = "right_ear";
    
    // Landmarks 454, 356, 389 define right ear area
    if (face_mesh.landmarks_pixel.size() > 454) {
        cv::Point2f p1 = face_mesh.landmarks_pixel[454];
        cv::Point2f p2 = face_mesh.landmarks_pixel[356];
        cv::Point2f p3 = face_mesh.landmarks_pixel[389];
        
        anchor.position_local = cv::Vec3f(
            (p1.x + p2.x + p3.x) / 3.0f,
            (p1.y + p2.y + p3.y) / 3.0f,
            0.0f
        );
        
        anchor.position_world = anchor.position_local;
        
        // Right-facing orientation
        cv::Vec3f right(1, 0, 0);
        anchor.orientation_quat = DirectionToQuaternion(right);
        
        anchor.transform_matrix = BuildTransformMatrix(
            anchor.position_world,
            anchor.orientation_quat,
            pose.scale
        );
        
        anchor.is_visible = IsWithinFrame(anchor.position_world);
        anchor.scale = pose.scale;
    }
    
    return anchor;
}

// Calculate chin anchor (for beards, masks)
AnchorPoint TransformCalculator::CalculateChin(const HeadPose& pose, const FaceMesh& face_mesh) {
    AnchorPoint anchor;
    anchor.name = "chin";
    
    // Landmarks 152, 199, 175 define chin area
    if (face_mesh.landmarks_pixel.size() > 199) {
        cv::Point2f center = face_mesh.landmarks_pixel[152];
        cv::Point2f left = face_mesh.landmarks_pixel[199];
        cv::Point2f right = face_mesh.landmarks_pixel[175];
        
        anchor.position_local = cv::Vec3f(
            (center.x + left.x + right.x) / 3.0f,
            (center.y + left.y + right.y) / 3.0f,
            0.0f
        );
        
        anchor.position_world = anchor.position_local;
        
        // Downward-facing orientation
        cv::Vec3f down(0, 1, 0);  // Positive Y is down
        anchor.orientation_quat = DirectionToQuaternion(down);
        
        anchor.transform_matrix = BuildTransformMatrix(
            anchor.position_world,
            anchor.orientation_quat,
            pose.scale
        );
        
        anchor.is_visible = IsWithinFrame(anchor.position_world);
        anchor.scale = pose.scale;
    }
    
    return anchor;
}

// Calculate left temple anchor (for glasses arms)
AnchorPoint TransformCalculator::CalculateLeftTemple(const HeadPose& pose, const FaceMesh& face_mesh) {
    AnchorPoint anchor;
    anchor.name = "left_temple";
    
    // Use same landmarks as left ear but with different orientation
    if (face_mesh.landmarks_pixel.size() > 162) {
        cv::Point2f p1 = face_mesh.landmarks_pixel[139];
        cv::Point2f p2 = face_mesh.landmarks_pixel[127];
        cv::Point2f p3 = face_mesh.landmarks_pixel[162];
        
        anchor.position_local = cv::Vec3f(
            (p1.x + p2.x + p3.x) / 3.0f,
            (p1.y + p2.y + p3.y) / 3.0f,
            0.0f
        );
        
        anchor.position_world = anchor.position_local;
        
        // Backward-left orientation (glasses arm direction)
        cv::Vec3f back_left(-0.707f, 0, -0.707f);
        anchor.orientation_quat = DirectionToQuaternion(back_left);
        
        anchor.transform_matrix = BuildTransformMatrix(
            anchor.position_world,
            anchor.orientation_quat,
            pose.scale
        );
        
        anchor.is_visible = IsWithinFrame(anchor.position_world);
        anchor.scale = pose.scale;
    }
    
    return anchor;
}

// Calculate right temple anchor (for glasses arms)
AnchorPoint TransformCalculator::CalculateRightTemple(const HeadPose& pose, const FaceMesh& face_mesh) {
    AnchorPoint anchor;
    anchor.name = "right_temple";
    
    // Use same landmarks as right ear but with different orientation
    if (face_mesh.landmarks_pixel.size() > 389) {
        cv::Point2f p1 = face_mesh.landmarks_pixel[368];
        cv::Point2f p2 = face_mesh.landmarks_pixel[356];
        cv::Point2f p3 = face_mesh.landmarks_pixel[389];
        
        anchor.position_local = cv::Vec3f(
            (p1.x + p2.x + p3.x) / 3.0f,
            (p1.y + p2.y + p3.y) / 3.0f,
            0.0f
        );
        
        anchor.position_world = anchor.position_local;
        
        // Backward-right orientation (glasses arm direction)
        cv::Vec3f back_right(0.707f, 0, -0.707f);
        anchor.orientation_quat = DirectionToQuaternion(back_right);
        
        anchor.transform_matrix = BuildTransformMatrix(
            anchor.position_world,
            anchor.orientation_quat,
            pose.scale
        );
        
        anchor.is_visible = IsWithinFrame(anchor.position_world);
        anchor.scale = pose.scale;
    }
    
    return anchor;
}

// =============================================================================
// Utility Methods
// =============================================================================

// Calculate head center position from face landmarks
cv::Vec3f TransformCalculator::CalculateHeadCenter(const FaceMesh& face_mesh) {
    // Use key landmarks to estimate head center
    // Simplified: average of nose tip, forehead, chin
    if (face_mesh.landmarks_pixel.size() > 152) {
        cv::Point2f nose = face_mesh.landmarks_pixel[1];   // Nose tip
        cv::Point2f forehead = face_mesh.landmarks_pixel[10];  // Forehead
        cv::Point2f chin = face_mesh.landmarks_pixel[152];  // Chin
        
        return cv::Vec3f(
            (nose.x + forehead.x + chin.x) / 3.0f,
            (nose.y + forehead.y + chin.y) / 3.0f,
            0.0f  // Depth estimation would go here
        );
    }
    
    return cv::Vec3f(0, 0, 0);
}

// Calculate face normal vector (perpendicular to face plane)
cv::Vec3f TransformCalculator::CalculateFaceNormal(const FaceMesh& face_mesh) {
    // Simplified: use forward direction from face plane
    // In real implementation, would calculate from 3 non-collinear points
    return cv::Vec3f(0, 0, 1);  // Forward in camera space
}

// Convert direction vector to quaternion
cv::Vec4f TransformCalculator::DirectionToQuaternion(const cv::Vec3f& forward_dir) {
    // Simplified: assume forward is Z-axis, calculate rotation from (0,0,1) to forward_dir
    // For now, return identity quaternion
    // TODO: Implement proper direction-to-quaternion conversion
    return cv::Vec4f(1, 0, 0, 0);  // Identity
}

// Check if position is within frame bounds
bool TransformCalculator::IsWithinFrame(const cv::Vec3f& position) const {
    return position[0] >= 0 && position[0] < frame_width_ &&
           position[1] >= 0 && position[1] < frame_height_;
}

// Get current time in milliseconds
double TransformCalculator::GetCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double, std::milli>(duration).count();
}

}  // namespace segmecam
