#include "include/ar_filters/face_mesh_processor.h"
#include <cmath>
#include <algorithm>

namespace segmecam {

FaceMeshProcessor::FaceMeshProcessor() : is_available_(false) {
    Reset();
}

void FaceMeshProcessor::Update(const mediapipe::NormalizedLandmarkList& landmarks,
                               int image_width, int image_height) {
    if (landmarks.landmark_size() != 478) {
        is_available_ = false;
        return;
    }
    
    // Store image dimensions
    current_mesh_.image_width = image_width;
    current_mesh_.image_height = image_height;
    
    // Store normalized 3D landmarks and convert to pixel coordinates
    ConvertToPixelCoordinates(landmarks, image_width, image_height);
    
    // Calculate face pose (Euler angles)
    CalculateFacePose();
    
    // Calculate face scale
    current_mesh_.scale = CalculateFaceScale();
    
    // Calculate confidence
    current_mesh_.confidence = CalculateConfidence(landmarks);
    
    // Store timestamp
    current_mesh_.timestamp_us = 
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    
    is_available_ = true;
}

void FaceMeshProcessor::ConvertToPixelCoordinates(
    const mediapipe::NormalizedLandmarkList& landmarks,
    int image_width, int image_height) {
    
    for (int i = 0; i < 478; ++i) {
        const auto& landmark = landmarks.landmark(i);
        
        // Store normalized 3D coordinates
        current_mesh_.landmarks_3d[i] = cv::Point3f(
            landmark.x(),
            landmark.y(),
            landmark.z()  // Z is depth in normalized space
        );
        
        // Convert to pixel coordinates
        current_mesh_.landmarks_2d[i] = cv::Point2f(
            landmark.x() * image_width,
            landmark.y() * image_height
        );
    }
}

void FaceMeshProcessor::CalculateFacePose() {
    // Calculate face pose using key facial landmarks
    // This is a simplified version - full 3D pose will be in Phase 2
    
    // Get key points for pose estimation
    cv::Point3f nose_tip = current_mesh_.landmarks_3d[FaceMeshLandmarks::NOSE_TIP];
    cv::Point3f nose_bridge = current_mesh_.landmarks_3d[FaceMeshLandmarks::NOSE_BRIDGE];
    cv::Point3f chin = current_mesh_.landmarks_3d[FaceMeshLandmarks::CHIN_CENTER];
    cv::Point3f left_eye = current_mesh_.landmarks_3d[FaceMeshLandmarks::LEFT_EYE_CENTER];
    cv::Point3f right_eye = current_mesh_.landmarks_3d[FaceMeshLandmarks::RIGHT_EYE_CENTER];
    cv::Point3f forehead = current_mesh_.landmarks_3d[FaceMeshLandmarks::FOREHEAD_CENTER];
    
    // Calculate yaw (left-right rotation) from horizontal asymmetry
    // Positive yaw = face turned right
    float left_eye_dist = std::abs(nose_tip.x - left_eye.x);
    float right_eye_dist = std::abs(nose_tip.x - right_eye.x);
    float yaw_raw = (left_eye_dist - right_eye_dist) / (left_eye_dist + right_eye_dist + 0.001f);
    float yaw = yaw_raw * 90.0f;  // Scale to degrees
    
    // Calculate pitch (up-down rotation) from vertical positions
    // Positive pitch = looking down
    float nose_forehead_dist = forehead.y - nose_tip.y;
    float nose_chin_dist = chin.y - nose_tip.y;
    float pitch_raw = (nose_chin_dist - nose_forehead_dist) / (nose_chin_dist + nose_forehead_dist + 0.001f);
    float pitch = pitch_raw * 45.0f;  // Scale to degrees
    
    // Calculate roll (head tilt) from eye line angle
    // Positive roll = head tilted right
    float eye_dx = right_eye.x - left_eye.x;
    float eye_dy = right_eye.y - left_eye.y;
    float roll = std::atan2(eye_dy, eye_dx) * 180.0f / M_PI;
    
    // Store Euler angles
    current_mesh_.euler_angles = cv::Vec3f(pitch, yaw, roll);
    
    // Initialize rotation matrix and translation (will be properly computed in Phase 2)
    current_mesh_.rotation_matrix = cv::Mat::eye(3, 3, CV_32F);
    current_mesh_.translation_vector = cv::Vec3f(
        (left_eye.x + right_eye.x) / 2.0f,
        (left_eye.y + right_eye.y) / 2.0f,
        (left_eye.z + right_eye.z) / 2.0f
    );
}

float FaceMeshProcessor::CalculateFaceScale() {
    // Calculate face scale based on inter-eye distance
    cv::Point3f left_eye = current_mesh_.landmarks_3d[FaceMeshLandmarks::LEFT_EYE_CENTER];
    cv::Point3f right_eye = current_mesh_.landmarks_3d[FaceMeshLandmarks::RIGHT_EYE_CENTER];
    
    float eye_distance = cv::norm(right_eye - left_eye);
    
    // Normalize by image diagonal
    float image_diagonal = std::sqrt(
        current_mesh_.image_width * current_mesh_.image_width +
        current_mesh_.image_height * current_mesh_.image_height
    );
    
    return eye_distance / (image_diagonal + 0.001f);
}

float FaceMeshProcessor::CalculateConfidence(
    const mediapipe::NormalizedLandmarkList& landmarks) {
    
    // Calculate average visibility/presence score from landmarks
    float total_confidence = 0.0f;
    int count = 0;
    
    for (int i = 0; i < landmarks.landmark_size(); ++i) {
        const auto& landmark = landmarks.landmark(i);
        if (landmark.has_visibility()) {
            total_confidence += landmark.visibility();
            count++;
        }
    }
    
    if (count > 0) {
        return total_confidence / count;
    }
    
    // If no visibility info, assume high confidence if we got valid data
    return 0.9f;
}

bool FaceMeshProcessor::IsFrontalView(float yaw_threshold) const {
    if (!is_available_) return false;
    return std::abs(GetYaw()) < yaw_threshold;
}

bool FaceMeshProcessor::IsLeftProfile(float yaw_threshold) const {
    if (!is_available_) return false;
    return GetYaw() < -yaw_threshold;
}

bool FaceMeshProcessor::IsRightProfile(float yaw_threshold) const {
    if (!is_available_) return false;
    return GetYaw() > yaw_threshold;
}

bool FaceMeshProcessor::IsLookingUp(float pitch_threshold) const {
    if (!is_available_) return false;
    return GetPitch() < -pitch_threshold;
}

bool FaceMeshProcessor::IsLookingDown(float pitch_threshold) const {
    if (!is_available_) return false;
    return GetPitch() > pitch_threshold;
}

void FaceMeshProcessor::Reset() {
    is_available_ = false;
    current_mesh_ = FaceMesh();
    current_mesh_.rotation_matrix = cv::Mat::eye(3, 3, CV_32F);
    current_mesh_.translation_vector = cv::Vec3f(0, 0, 0);
    current_mesh_.euler_angles = cv::Vec3f(0, 0, 0);
    current_mesh_.scale = 0.0f;
    current_mesh_.confidence = 0.0f;
}

} // namespace segmecam
