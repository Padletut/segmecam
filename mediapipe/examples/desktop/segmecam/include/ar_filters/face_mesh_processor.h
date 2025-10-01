#ifndef SEGMECAM_FACE_MESH_PROCESSOR_H
#define SEGMECAM_FACE_MESH_PROCESSOR_H

#include <array>
#include <opencv2/opencv.hpp>
#include "mediapipe/framework/formats/landmark.pb.h"

namespace segmecam {

/**
 * Face mesh landmark indices for key AR attachment points
 * Based on MediaPipe's 478-point face mesh model
 */
struct FaceMeshLandmarks {
    // Nose landmarks
    static constexpr int NOSE_TIP = 1;
    static constexpr int NOSE_BRIDGE = 6;
    static constexpr int NOSE_BOTTOM = 2;
    
    // Forehead landmarks
    static constexpr int FOREHEAD_CENTER = 10;
    static constexpr int FOREHEAD_LEFT = 338;
    static constexpr int FOREHEAD_RIGHT = 109;
    
    // Eye landmarks
    static constexpr int LEFT_EYE_CENTER = 468;
    static constexpr int RIGHT_EYE_CENTER = 473;
    static constexpr int LEFT_EYE_INNER = 133;
    static constexpr int RIGHT_EYE_INNER = 362;
    static constexpr int LEFT_EYE_OUTER = 33;
    static constexpr int RIGHT_EYE_OUTER = 263;
    
    // Temple landmarks (for glasses arms)
    static constexpr int LEFT_TEMPLE = 234;
    static constexpr int RIGHT_TEMPLE = 454;
    
    // Cheek landmarks
    static constexpr int LEFT_CHEEK = 205;
    static constexpr int RIGHT_CHEEK = 425;
    
    // Chin landmarks
    static constexpr int CHIN_CENTER = 152;
    static constexpr int CHIN_LEFT = 172;
    static constexpr int CHIN_RIGHT = 397;
    
    // Mouth landmarks
    static constexpr int MOUTH_CENTER = 13;
    static constexpr int MOUTH_LEFT = 61;
    static constexpr int MOUTH_RIGHT = 291;
    static constexpr int UPPER_LIP_CENTER = 0;
    static constexpr int LOWER_LIP_CENTER = 17;
    
    // Eyebrow landmarks
    static constexpr int LEFT_EYEBROW_INNER = 66;
    static constexpr int LEFT_EYEBROW_OUTER = 105;
    static constexpr int RIGHT_EYEBROW_INNER = 296;
    static constexpr int RIGHT_EYEBROW_OUTER = 334;
};

/**
 * Face mesh data structure holding 478 3D landmarks
 * Includes both normalized (0-1) and pixel coordinates
 */
struct FaceMesh {
    std::array<cv::Point3f, 478> landmarks_3d;  // 3D coordinates (normalized 0-1 for x,y; z is depth)
    std::array<cv::Point2f, 478> landmarks_2d;  // Pixel coordinates
    
    // Face pose information
    cv::Mat rotation_matrix;        // 3x3 rotation matrix (will be calculated in Phase 2)
    cv::Vec3f translation_vector;   // Translation vector (will be calculated in Phase 2)
    cv::Vec3f euler_angles;         // Euler angles (pitch, yaw, roll) in degrees
    float scale;                    // Face scale relative to image
    
    // Metadata
    float confidence;               // Detection confidence (0.0 - 1.0)
    int64_t timestamp_us;           // Timestamp in microseconds
    int image_width;                // Source image width
    int image_height;               // Source image height
    
    // Accessors
    cv::Point3f GetLandmark3D(int index) const {
        if (index >= 0 && index < 478) {
            return landmarks_3d[index];
        }
        return cv::Point3f(0, 0, 0);
    }
    
    cv::Point2f GetLandmark2D(int index) const {
        if (index >= 0 && index < 478) {
            return landmarks_2d[index];
        }
        return cv::Point2f(0, 0);
    }
    
    // Get landmark by name (for key attachment points)
    cv::Point3f GetNoseTip3D() const { return GetLandmark3D(FaceMeshLandmarks::NOSE_TIP); }
    cv::Point2f GetNoseTip2D() const { return GetLandmark2D(FaceMeshLandmarks::NOSE_TIP); }
    
    cv::Point3f GetNoseBridge3D() const { return GetLandmark3D(FaceMeshLandmarks::NOSE_BRIDGE); }
    cv::Point2f GetNoseBridge2D() const { return GetLandmark2D(FaceMeshLandmarks::NOSE_BRIDGE); }
    
    cv::Point3f GetForeheadCenter3D() const { return GetLandmark3D(FaceMeshLandmarks::FOREHEAD_CENTER); }
    cv::Point2f GetForeheadCenter2D() const { return GetLandmark2D(FaceMeshLandmarks::FOREHEAD_CENTER); }
    
    cv::Point3f GetLeftTemple3D() const { return GetLandmark3D(FaceMeshLandmarks::LEFT_TEMPLE); }
    cv::Point2f GetLeftTemple2D() const { return GetLandmark2D(FaceMeshLandmarks::LEFT_TEMPLE); }
    
    cv::Point3f GetRightTemple3D() const { return GetLandmark3D(FaceMeshLandmarks::RIGHT_TEMPLE); }
    cv::Point2f GetRightTemple2D() const { return GetLandmark2D(FaceMeshLandmarks::RIGHT_TEMPLE); }
    
    cv::Point3f GetChinCenter3D() const { return GetLandmark3D(FaceMeshLandmarks::CHIN_CENTER); }
    cv::Point2f GetChinCenter2D() const { return GetLandmark2D(FaceMeshLandmarks::CHIN_CENTER); }
};

/**
 * Processor for MediaPipe 478-point face mesh
 * Extracts and processes face mesh landmarks for AR filter applications
 * 
 * Usage:
 *   FaceMeshProcessor processor;
 *   processor.Update(mediapipe_landmarks, image_width, image_height);
 *   FaceMesh mesh = processor.GetFaceMesh();
 *   cv::Point2f nose_tip = mesh.GetNoseTip2D();
 */
class FaceMeshProcessor {
public:
    FaceMeshProcessor();
    ~FaceMeshProcessor() = default;
    
    /**
     * Update face mesh from MediaPipe landmarks
     * @param landmarks MediaPipe NormalizedLandmarkList with 478 points
     * @param image_width Source image width in pixels
     * @param image_height Source image height in pixels
     */
    void Update(const mediapipe::NormalizedLandmarkList& landmarks,
                int image_width, int image_height);
    
    /**
     * Get current face mesh data
     * @return FaceMesh structure with all 478 landmarks
     */
    const FaceMesh& GetFaceMesh() const { return current_mesh_; }
    
    /**
     * Check if face mesh is available
     * @return true if Update() has been called successfully
     */
    bool IsAvailable() const { return is_available_; }
    
    // Face pose queries (Euler angles calculated from landmark positions)
    
    /**
     * Get face rotation as Euler angles
     * @return Vec3f (pitch, yaw, roll) in degrees
     */
    cv::Vec3f GetFaceRotation() const { return current_mesh_.euler_angles; }
    
    /**
     * Get face pitch angle (looking up/down)
     * @return Pitch angle in degrees (positive = looking down)
     */
    float GetPitch() const { return current_mesh_.euler_angles[0]; }
    
    /**
     * Get face yaw angle (looking left/right)
     * @return Yaw angle in degrees (positive = looking right)
     */
    float GetYaw() const { return current_mesh_.euler_angles[1]; }
    
    /**
     * Get face roll angle (head tilt)
     * @return Roll angle in degrees (positive = tilted right)
     */
    float GetRoll() const { return current_mesh_.euler_angles[2]; }
    
    /**
     * Get estimated face scale
     * @return Scale factor relative to image size
     */
    float GetFaceScale() const { return current_mesh_.scale; }
    
    /**
     * Get detection confidence
     * @return Confidence score (0.0 - 1.0)
     */
    float GetConfidence() const { return current_mesh_.confidence; }
    
    // Face visibility/orientation queries
    
    /**
     * Check if face is in frontal view (looking at camera)
     * @param yaw_threshold Maximum yaw angle for frontal view (default: 20 degrees)
     * @return true if face is facing camera
     */
    bool IsFrontalView(float yaw_threshold = 20.0f) const;
    
    /**
     * Check if face is in left profile view
     * @param yaw_threshold Minimum yaw angle for profile (default: 30 degrees)
     * @return true if face is turned left
     */
    bool IsLeftProfile(float yaw_threshold = 30.0f) const;
    
    /**
     * Check if face is in right profile view
     * @param yaw_threshold Minimum yaw angle for profile (default: 30 degrees)
     * @return true if face is turned right
     */
    bool IsRightProfile(float yaw_threshold = 30.0f) const;
    
    /**
     * Check if face is looking up
     * @param pitch_threshold Minimum pitch angle (default: 15 degrees)
     * @return true if face is tilted up
     */
    bool IsLookingUp(float pitch_threshold = 15.0f) const;
    
    /**
     * Check if face is looking down
     * @param pitch_threshold Minimum pitch angle (default: 15 degrees)
     * @return true if face is tilted down
     */
    bool IsLookingDown(float pitch_threshold = 15.0f) const;
    
    /**
     * Reset processor state
     */
    void Reset();
    
private:
    FaceMesh current_mesh_;
    bool is_available_;
    
    /**
     * Convert normalized coordinates to pixel coordinates
     * @param landmarks MediaPipe landmarks (normalized 0-1)
     * @param image_width Image width in pixels
     * @param image_height Image height in pixels
     */
    void ConvertToPixelCoordinates(const mediapipe::NormalizedLandmarkList& landmarks,
                                   int image_width, int image_height);
    
    /**
     * Calculate face pose (Euler angles) from landmark positions
     * Uses key facial landmarks to estimate head orientation
     */
    void CalculateFacePose();
    
    /**
     * Calculate face scale based on inter-landmark distances
     * @return Scale factor relative to image size
     */
    float CalculateFaceScale();
    
    /**
     * Calculate average confidence from landmarks
     * @param landmarks MediaPipe landmarks with confidence scores
     * @return Average confidence (0.0 - 1.0)
     */
    float CalculateConfidence(const mediapipe::NormalizedLandmarkList& landmarks);
};

} // namespace segmecam

#endif // SEGMECAM_FACE_MESH_PROCESSOR_H
