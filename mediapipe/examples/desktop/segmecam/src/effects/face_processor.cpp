#include "include/effects/face_processor.h"
#include "include/effects/face_regions.h"
#include "include/effects/segmecam_face_effects.h"
#include "include/effects/advanced_skin_effects.h"
#include "mediapipe/examples/desktop/segmecam/src/ar_filters/face_mesh_processor.h"
#include "mediapipe/tasks/cc/vision/face_landmarker/face_landmarks_connections.h"
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace segmecam {

FaceProcessor::FaceProcessor() {
}

FaceProcessor::~FaceProcessor() {
}

FaceRegions FaceProcessor::ExtractFaceRegionsFromLandmarks(const mediapipe::NormalizedLandmarkList& landmarks,
                                                          const cv::Size& frame_size) {
    FaceRegions regions;
    ExtractFaceRegions(landmarks, frame_size, &regions, false, false, false);
    return regions;
}

void FaceProcessor::DrawFaceMeshBase(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks,
                                    const cv::Scalar& face_oval_color, const cv::Scalar& eye_color, const cv::Scalar& eyebrow_color) {
    using Conn = mediapipe::tasks::vision::face_landmarker::FaceLandmarksConnections;
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksFaceOval, face_oval_color);
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksLeftEye, eye_color);
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksRightEye, eye_color);
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksLeftEyeBrow, eyebrow_color);
    DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksRightEyeBrow, eyebrow_color);
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
    DrawFaceMeshBase(frame_bgr, landmarks, cv::Scalar(0, 200, 255), cv::Scalar(255, 200, 80), cv::Scalar(180, 180, 255));
}

void FaceProcessor::DrawMesh(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks, bool dense) {
    // Draw face mesh connections (face oval, eyes, eyebrows)
    DrawFaceMeshBase(frame_bgr, landmarks, cv::Scalar(255, 200, 0), cv::Scalar(80, 200, 255), cv::Scalar(180, 180, 255));
    
    // Optional dense tessellation
    if (dense) {
        using Conn = mediapipe::tasks::vision::face_landmarker::FaceLandmarksConnections;
        DrawConnections(frame_bgr, landmarks, Conn::kFaceLandmarksTesselation, cv::Scalar(120, 120, 120));
    }
}

void FaceProcessor::DrawFaceMesh(cv::Mat& frame_bgr, const FaceMesh& face_mesh, bool dense, bool show_pose) {
    if (frame_bgr.empty() || face_mesh.confidence < 0.1f) {
        return;
    }
    
    // Draw all 478 landmarks as colored points
    for (int i = 0; i < 478; ++i) {
        cv::Point2f pt = face_mesh.landmarks_2d[i];
        
        // Skip invalid points
        if (pt.x < 0 || pt.y < 0 || pt.x >= frame_bgr.cols || pt.y >= frame_bgr.rows) {
            continue;
        }
        
        // Color code by face region for better visualization
        cv::Scalar color;
        if (i >= 0 && i < 17) {
            color = cv::Scalar(255, 200, 0);  // Face oval - yellow
        } else if ((i >= 33 && i < 133) || (i >= 263 && i < 362)) {
            color = cv::Scalar(80, 200, 255);  // Eyes - orange
        } else if ((i >= 156 && i < 178) || (i >= 385 && i < 407)) {
            color = cv::Scalar(180, 180, 255); // Eyebrows - pink
        } else if (i >= 0 && i < 40) {
            color = cv::Scalar(0, 128, 255);   // Lips - red
        } else if (i >= 6 && i < 28) {
            color = cv::Scalar(255, 255, 0);   // Nose - cyan
        } else {
            color = cv::Scalar(200, 200, 200); // Other - gray
        }
        
        // Draw point
        int radius = dense ? 1 : 2;
        cv::circle(frame_bgr, pt, radius, color, -1, cv::LINE_AA);
    }
    
    // Draw key attachment points with larger circles
    auto drawKeyPoint = [&](int index, const cv::Scalar& color, const std::string& label = "") {
        if (index >= 0 && index < 478) {
            cv::Point2f pt = face_mesh.landmarks_2d[index];
            cv::circle(frame_bgr, pt, 4, color, 2, cv::LINE_AA);
            if (!label.empty() && !dense) {
                cv::putText(frame_bgr, label, pt + cv::Point2f(8, 0), 
                           cv::FONT_HERSHEY_SIMPLEX, 0.4, color, 1, cv::LINE_AA);
            }
        }
    };
    
    using LM = FaceMeshLandmarks;
    drawKeyPoint(LM::NOSE_TIP, cv::Scalar(0, 255, 0), "nose");
    drawKeyPoint(LM::NOSE_BRIDGE, cv::Scalar(0, 255, 255), "bridge");
    drawKeyPoint(LM::FOREHEAD_CENTER, cv::Scalar(255, 0, 255), "forehead");
    drawKeyPoint(LM::LEFT_TEMPLE, cv::Scalar(255, 128, 0), "L temple");
    drawKeyPoint(LM::RIGHT_TEMPLE, cv::Scalar(255, 128, 0), "R temple");
    drawKeyPoint(LM::CHIN_CENTER, cv::Scalar(128, 255, 0), "chin");
    
    // Draw face pose axes if requested
    if (show_pose) {
        DrawFacePose(frame_bgr, face_mesh);
    }
    
    // Draw info overlay
    if (!dense) {
        std::string info = cv::format("Face Mesh: 478 pts | Yaw: %.1f° | Pitch: %.1f° | Roll: %.1f° | Scale: %.3f",
                                     face_mesh.euler_angles[1], face_mesh.euler_angles[0], 
                                     face_mesh.euler_angles[2], face_mesh.scale);
        cv::putText(frame_bgr, info, cv::Point(10, frame_bgr.rows - 50),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
        
        // Draw face orientation status
        std::string orientation;
        if (std::abs(face_mesh.euler_angles[1]) < 20.0f) {
            orientation = "Frontal";
        } else if (face_mesh.euler_angles[1] < -30.0f) {
            orientation = "Left Profile";
        } else if (face_mesh.euler_angles[1] > 30.0f) {
            orientation = "Right Profile";
        } else {
            orientation = "Turned";
        }
        
        if (std::abs(face_mesh.euler_angles[0]) > 15.0f) {
            orientation += face_mesh.euler_angles[0] < 0 ? " + Looking Up" : " + Looking Down";
        }
        
        cv::putText(frame_bgr, "Orientation: " + orientation, cv::Point(10, frame_bgr.rows - 30),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
    }
}

void FaceProcessor::DrawFacePose(cv::Mat& frame_bgr, const FaceMesh& face_mesh) {
    // Get nose tip as the origin point for pose axes
    cv::Point2f origin = face_mesh.GetNoseTip2D();
    
    if (origin.x < 0 || origin.y < 0 || origin.x >= frame_bgr.cols || origin.y >= frame_bgr.rows) {
        return;
    }
    
    // Calculate axis length based on face scale
    float axis_length = face_mesh.scale * std::sqrt(frame_bgr.cols * frame_bgr.cols + 
                                                     frame_bgr.rows * frame_bgr.rows) * 0.1f;
    axis_length = std::max(30.0f, std::min(axis_length, 100.0f)); // Clamp to reasonable range
    
    // Convert Euler angles to radians
    float pitch_rad = face_mesh.euler_angles[0] * M_PI / 180.0f;
    float yaw_rad = face_mesh.euler_angles[1] * M_PI / 180.0f;
    float roll_rad = face_mesh.euler_angles[2] * M_PI / 180.0f;
    
    // Calculate rotation matrix from Euler angles (ZYX order: roll, pitch, yaw)
    float cos_roll = std::cos(roll_rad);
    float sin_roll = std::sin(roll_rad);
    float cos_pitch = std::cos(pitch_rad);
    float sin_pitch = std::sin(pitch_rad);
    float cos_yaw = std::cos(yaw_rad);
    float sin_yaw = std::sin(yaw_rad);
    
    // Define 3D axis directions
    cv::Point3f x_axis(axis_length, 0, 0);  // Right (red)
    cv::Point3f y_axis(0, -axis_length, 0); // Up (green)
    cv::Point3f z_axis(0, 0, -axis_length); // Forward (blue)
    
    // Rotate axes by Euler angles
    auto rotatePoint = [&](const cv::Point3f& pt) -> cv::Point2f {
        // Apply roll, pitch, yaw rotations
        float x = pt.x;
        float y = pt.y;
        float z = pt.z;
        
        // Roll (Z-axis rotation)
        float x1 = x * cos_roll - y * sin_roll;
        float y1 = x * sin_roll + y * cos_roll;
        float z1 = z;
        
        // Pitch (X-axis rotation)
        float y2 = y1 * cos_pitch - z1 * sin_pitch;
        float z2 = y1 * sin_pitch + z1 * cos_pitch;
        float x2 = x1;
        
        // Yaw (Y-axis rotation)
        float x3 = x2 * cos_yaw + z2 * sin_yaw;
        float z3 = -x2 * sin_yaw + z2 * cos_yaw;
        
        // Project to 2D (simple orthographic projection)
        return cv::Point2f(origin.x + x3, origin.y + y2);
    };
    
    cv::Point2f x_end = rotatePoint(x_axis);
    cv::Point2f y_end = rotatePoint(y_axis);
    cv::Point2f z_end = rotatePoint(z_axis);
    
    // Draw axes with thicker lines
    cv::arrowedLine(frame_bgr, origin, x_end, cv::Scalar(0, 0, 255), 3, cv::LINE_AA, 0, 0.3); // Red = X (right)
    cv::arrowedLine(frame_bgr, origin, y_end, cv::Scalar(0, 255, 0), 3, cv::LINE_AA, 0, 0.3); // Green = Y (up)
    cv::arrowedLine(frame_bgr, origin, z_end, cv::Scalar(255, 0, 0), 3, cv::LINE_AA, 0, 0.3); // Blue = Z (forward)
    
    // Draw labels
    cv::putText(frame_bgr, "X", x_end, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
    cv::putText(frame_bgr, "Y", y_end, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
    cv::putText(frame_bgr, "Z", z_end, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 2, cv::LINE_AA);
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

void FaceProcessor::ApplySkinSmoothingWithProcessingScale(cv::Mat& frame_bgr, const FaceRegions& regions,
                                                        const mediapipe::NormalizedLandmarkList& landmarks,
                                                        const BeautyState& beauty_state,
                                                        const cv::Mat& wrinkle_mask,
                                                        cv::Mat* wrinkle_inpaint_debug) {
    const bool wrinkle_processing_requested = beauty_state.fx_skin_wrinkle ||
                                              beauty_state.fx_wrinkle_preview ||
                                              beauty_state.fx_skin_smile_wrinkle_gain > 1e-3f;
    if (beauty_state.fx_skin_amount < kMinSkinSmoothingAmount && !wrinkle_processing_requested) {
        return;
    }
    cv::Rect roi = CalculateProcessingROI(regions, frame_bgr.size());
    if (roi.width < 8 || roi.height < 8) {
        ApplyFullResolutionSkinSmoothing(frame_bgr, regions, landmarks, beauty_state, wrinkle_mask, wrinkle_inpaint_debug);
        return;
    }

    FaceRegions fr_small = TransformFaceRegionsToScaledROI(regions, roi, beauty_state.fx_adv_scale);
    mediapipe::NormalizedLandmarkList lms_roi = TransformLandmarksToROI(landmarks, roi, frame_bgr.size());

    cv::Mat wrinkle_roi;
    if (!wrinkle_mask.empty()) {
        cv::Rect mask_bounds(0, 0, wrinkle_mask.cols, wrinkle_mask.rows);
        cv::Rect roi_clamped = roi & mask_bounds;
        wrinkle_roi = cv::Mat::zeros(roi.size(), CV_32F);
        if (roi_clamped.width > 0 && roi_clamped.height > 0) {
            cv::Rect dst_rect(roi_clamped.x - roi.x, roi_clamped.y - roi.y,
                              roi_clamped.width, roi_clamped.height);
            cv::Mat dst = wrinkle_roi(dst_rect);
            cv::Mat src = wrinkle_mask(roi_clamped);
            if (src.type() == CV_32F) {
                src.copyTo(dst);
            } else {
                cv::Mat src_float;
                src.convertTo(src_float, CV_32F, 1.0 / 255.0);
                src_float.copyTo(dst);
            }
        }
    }

    ProcessAndUpsampleROI(frame_bgr, roi, fr_small, lms_roi, beauty_state, beauty_state.fx_adv_scale, wrinkle_roi, wrinkle_inpaint_debug);
}

cv::Rect FaceProcessor::CalculateProcessingROI(const FaceRegions& regions, const cv::Size& frame_size) {
    cv::Rect face_bb = cv::boundingRect(regions.face_oval);
    int pad = std::max(8, (int)std::round(8.0f + 4.0f * 2.0f)); // Using default values for edge and radius
    cv::Rect roi(face_bb.x - pad, face_bb.y - pad, face_bb.width + 2*pad, face_bb.height + 2*pad);
    roi &= cv::Rect(0, 0, frame_size.width, frame_size.height);
    return roi;
}

void FaceProcessor::SetupSkinSmoothingConfig(SkinSmoothingConfig& config, const BeautyState& beauty_state, float scale) {
    config.amount = NormalizeAdvancedAmount(beauty_state.fx_skin_amount);
    config.radius_px = beauty_state.fx_skin_radius * scale;
    config.texture_thresh = beauty_state.fx_skin_tex;
    config.edge_feather_px = beauty_state.fx_skin_edge * scale;
    config.expression.smile_boost = beauty_state.fx_skin_smile_boost;
    config.expression.squint_boost = beauty_state.fx_skin_squint_boost;
    config.expression.forehead_boost = beauty_state.fx_skin_forehead_boost;
    config.expression.forehead_margin_px = 10.0f * scale;
    config.boost_gain = beauty_state.fx_skin_wrinkle_gain;
    config.smile_wrinkle_gain = beauty_state.fx_skin_smile_wrinkle_gain;
    config.wrinkle_enabled = beauty_state.fx_skin_wrinkle;
    config.wrinkle.region_gates.suppress_lower_face = beauty_state.fx_wrinkle_suppress_lower;
    config.wrinkle.region_gates.lower_face_ratio = beauty_state.fx_wrinkle_lower_ratio;
    config.wrinkle.region_gates.ignore_glasses = beauty_state.fx_wrinkle_ignore_glasses;
    config.wrinkle.region_gates.glasses_margin_px = beauty_state.fx_wrinkle_glasses_margin * scale;
    config.wrinkle.keep_ratio = beauty_state.fx_wrinkle_keep_ratio;
    config.wrinkle.line_min_px = beauty_state.fx_wrinkle_custom_scales ? beauty_state.fx_wrinkle_min_px * scale : 1.5f;
    config.wrinkle.line_max_px = beauty_state.fx_wrinkle_custom_scales ? beauty_state.fx_wrinkle_max_px * scale : 3.0f;
    config.wrinkle_preview = beauty_state.fx_wrinkle_preview;
    config.baseline_boost = beauty_state.fx_wrinkle_baseline;
    config.wrinkle.use_skin_gate = beauty_state.fx_wrinkle_use_skin_gate;
    config.wrinkle.mask_gain = beauty_state.fx_wrinkle_mask_gain;
    config.neg_atten_cap = beauty_state.fx_wrinkle_neg_cap;
    config.use_cv_inpaint = beauty_state.fx_wrinkle_use_cv_inpaint;
    // boost_gain is now properly set above from fx_skin_wrinkle_gain
}

void FaceProcessor::ApplyFullResolutionSkinSmoothing(cv::Mat& frame_bgr, const FaceRegions& regions,
                                                   const mediapipe::NormalizedLandmarkList& landmarks,
                                                   const BeautyState& beauty_state,
                                                   const cv::Mat& wrinkle_mask,
                                                   cv::Mat* wrinkle_inpaint_debug) {
    const bool wrinkle_processing_requested = beauty_state.fx_skin_wrinkle ||
                                              beauty_state.fx_wrinkle_preview ||
                                              beauty_state.fx_skin_smile_wrinkle_gain > 1e-3f;
    if (beauty_state.fx_skin_amount < kMinSkinSmoothingAmount && !wrinkle_processing_requested) {
        return;
    }
    // Create config with scale = 1.0 for full resolution
    SkinSmoothingConfig config;
    SetupSkinSmoothingConfig(config, beauty_state, 1.0f);
    FacialExpressionMetrics metrics = ApplySkinSmoothingAdvBGR(frame_bgr, regions, config, &landmarks, wrinkle_mask, wrinkle_inpaint_debug);
    // Note: metrics are not stored here as this is called from multiple places
    // The caller (EffectsManager) should store the metrics if needed
}

FaceRegions FaceProcessor::TransformFaceRegionsToScaledROI(const FaceRegions& regions, const cv::Rect& roi, float scale) {
    // Transform to ROI coordinates, then scale
    FaceRegions fr_roi = ShiftFaceRegionsToROI(regions, roi);
    FaceRegions fr_small = ScaleFaceRegions(fr_roi, scale);
    return fr_small;
}

FaceRegions FaceProcessor::ShiftFaceRegionsToROI(const FaceRegions& regions, const cv::Rect& roi) {
    FaceRegions fr_roi;
    fr_roi.face_oval = ShiftPolygon(regions.face_oval, -roi.x, -roi.y);
    fr_roi.lips_outer = ShiftPolygon(regions.lips_outer, -roi.x, -roi.y);
    fr_roi.lips_inner = ShiftPolygon(regions.lips_inner, -roi.x, -roi.y);
    fr_roi.left_eye = ShiftPolygon(regions.left_eye, -roi.x, -roi.y);
    fr_roi.right_eye = ShiftPolygon(regions.right_eye, -roi.x, -roi.y);
    return fr_roi;
}

FaceRegions FaceProcessor::ScaleFaceRegions(const FaceRegions& regions, float scale) {
    FaceRegions fr_scaled;
    fr_scaled.face_oval = ScalePolygon(regions.face_oval, scale);
    fr_scaled.lips_outer = ScalePolygon(regions.lips_outer, scale);
    fr_scaled.lips_inner = ScalePolygon(regions.lips_inner, scale);
    fr_scaled.left_eye = ScalePolygon(regions.left_eye, scale);
    fr_scaled.right_eye = ScalePolygon(regions.right_eye, scale);
    return fr_scaled;
}

std::vector<cv::Point> FaceProcessor::ShiftPolygon(const std::vector<cv::Point>& poly, int dx, int dy) {
    std::vector<cv::Point> out;
    out.reserve(poly.size());
    for (auto p : poly) {
        out.emplace_back(p.x + dx, p.y + dy);
    }
    return out;
}

std::vector<cv::Point> FaceProcessor::ScalePolygon(const std::vector<cv::Point>& poly, float scale) {
    std::vector<cv::Point> out;
    out.reserve(poly.size());
    for (auto p : poly) {
        out.emplace_back((int)std::round(p.x * scale), (int)std::round(p.y * scale));
    }
    return out;
}

mediapipe::NormalizedLandmarkList FaceProcessor::TransformLandmarksToROI(const mediapipe::NormalizedLandmarkList& landmarks,
                                                                       const cv::Rect& roi, const cv::Size& frame_size) {
    mediapipe::NormalizedLandmarkList lms_roi = landmarks;
    for (int i = 0; i < lms_roi.landmark_size(); ++i) {
        auto* p = lms_roi.mutable_landmark(i);
        float px = p->x() * frame_size.width;
        float py = p->y() * frame_size.height;
        float xr = (px - roi.x) / (float)roi.width;
        float yr = (py - roi.y) / (float)roi.height;
        p->set_x(xr);
        p->set_y(yr);
    }
    return lms_roi;
}

void FaceProcessor::ProcessAndUpsampleROI(cv::Mat& frame_bgr, const cv::Rect& roi, const FaceRegions& fr_small,
                                        const mediapipe::NormalizedLandmarkList& lms_roi, const BeautyState& beauty_state, float scale,
                                        const cv::Mat& wrinkle_mask,
                                        cv::Mat* wrinkle_inpaint_debug) {
    // Use direct ROI reference to avoid unnecessary copy
    cv::Mat roi_bgr = frame_bgr(roi);
    
    // Optimize: Use INTER_LINEAR for downscaling (faster than INTER_AREA, good enough quality)
    cv::Size target_size(std::max(8, (int)std::round(roi.width * scale)),
                        std::max(8, (int)std::round(roi.height * scale)));
    cv::Mat small;
    cv::resize(roi_bgr, small, target_size, 0, 0, cv::INTER_LINEAR);

    cv::Mat wrinkle_scaled;
    if (!wrinkle_mask.empty()) {
        cv::Mat wrinkle_float;
        if (wrinkle_mask.type() == CV_32F) {
            wrinkle_float = wrinkle_mask;
        } else {
            wrinkle_mask.convertTo(wrinkle_float, CV_32F, 1.0 / 255.0);
        }
        if (wrinkle_float.size() != small.size()) {
            cv::resize(wrinkle_float, wrinkle_scaled, small.size(), 0, 0, cv::INTER_LINEAR);
        } else {
            wrinkle_scaled = wrinkle_float;
        }
    }

    cv::Mat local_inpaint;
    ApplySkinSmoothingToScaledImage(small, fr_small, lms_roi, beauty_state, scale, wrinkle_scaled,
                                    wrinkle_inpaint_debug ? &local_inpaint : nullptr);

    // Optimize: Use INTER_LINEAR for upsampling (much faster than INTER_LANCZOS4)
    cv::Mat up;
    cv::resize(small, up, roi.size(), 0, 0, cv::INTER_LINEAR);
    
    // Apply detail preservation if needed
    ApplyDetailPreservationIfNeeded(up, roi_bgr, fr_small, beauty_state);

    // Direct copy to ROI (no intermediate copy)
    up.copyTo(roi_bgr);

    if (wrinkle_inpaint_debug && !local_inpaint.empty()) {
        cv::Mat up_mask;
        cv::resize(local_inpaint, up_mask, roi.size(), 0, 0, cv::INTER_LINEAR);
        if (wrinkle_inpaint_debug->empty()) {
            *wrinkle_inpaint_debug = cv::Mat::zeros(frame_bgr.size(), CV_32F);
        } else if (wrinkle_inpaint_debug->type() != CV_32F || wrinkle_inpaint_debug->size() != frame_bgr.size()) {
            wrinkle_inpaint_debug->create(frame_bgr.size(), CV_32F);
            wrinkle_inpaint_debug->setTo(0.0f);
        }
        cv::Mat roi_mask = (*wrinkle_inpaint_debug)(roi);
        up_mask.copyTo(roi_mask);
    }
}

// DownscaleROI removed - inlined in ProcessAndUpsampleROI for optimization

void FaceProcessor::ApplySkinSmoothingToScaledImage(cv::Mat& small, const FaceRegions& fr_small,
                                                  const mediapipe::NormalizedLandmarkList& lms_roi,
                                                  const BeautyState& beauty_state, float scale,
                                                  const cv::Mat& wrinkle_mask,
                                                  cv::Mat* wrinkle_inpaint_debug) {
    SkinSmoothingConfig config;
    SetupSkinSmoothingConfig(config, beauty_state, scale);
    // For scaled processing, we don't need to store metrics
    ApplySkinSmoothingAdvBGR(small, fr_small, config, &lms_roi, wrinkle_mask, wrinkle_inpaint_debug);
}

// UpsampleProcessedImage removed - inlined in ProcessAndUpsampleROI for optimization

void FaceProcessor::ApplyDetailPreservationIfNeeded(cv::Mat& up, const cv::Mat& roi_bgr, const FaceRegions& fr_roi,
                                                  const BeautyState& beauty_state) {
    float dp = std::clamp(beauty_state.fx_adv_detail_preserve, 0.0f, 0.5f);
    // Optimize: Only apply detail preservation if the value is meaningful (> 0.05)
    if (dp > 0.05f) {
        ApplyDetailPreservation(up, roi_bgr, fr_roi, dp);
    }
}

void FaceProcessor::ApplyDetailPreservation(cv::Mat& up, const cv::Mat& roi_bgr, const FaceRegions& fr_roi, float dp) {
    cv::Mat mask_roi_u8 = CreateFaceMask(fr_roi, up.size());

    int fk = std::max(3, (int)std::round(8.0f) | 1); // Using default edge value
    cv::GaussianBlur(mask_roi_u8, mask_roi_u8, cv::Size(fk, fk), 0);
    cv::Mat mask_f;
    mask_roi_u8.convertTo(mask_f, CV_32F, 1.0/255.0);

    // Unsharp masking: add fraction of high-frequency detail from original ROI
    cv::Mat base;
    cv::GaussianBlur(roi_bgr, base, cv::Size(0,0), 0.8);
    cv::Mat roi32, base32;
    roi_bgr.convertTo(roi32, CV_32F, 1.0/255.0);
    base.convertTo(base32, CV_32F, 1.0/255.0);
    cv::Mat hi = roi32 - base32; // High frequency component

    std::vector<cv::Mat> uch(3), hi_ch(3);
    cv::split(up, uch);
    cv::split(hi, hi_ch);

    for (int i = 0; i < 3; ++i) {
        cv::Mat u32;
        uch[i].convertTo(u32, CV_32F, 1.0/255.0);
        cv::Mat out32 = u32 + hi_ch[i].mul(mask_f * dp);
        out32 = cv::min(cv::max(out32, 0.0f), 1.0f);
        out32.convertTo(uch[i], CV_8U, 255.0);
    }
    cv::merge(uch, up);
}

cv::Mat FaceProcessor::CreateFaceMask(const FaceRegions& fr_roi, const cv::Size& size) {
    cv::Mat mask(size, CV_8U, cv::Scalar(0));
    if (!fr_roi.face_oval.empty()) cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{fr_roi.face_oval}, cv::Scalar(255));
    if (!fr_roi.lips_outer.empty()) cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{fr_roi.lips_outer}, cv::Scalar(0));
    if (!fr_roi.left_eye.empty()) cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{fr_roi.left_eye}, cv::Scalar(0));
    if (!fr_roi.right_eye.empty()) cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{fr_roi.right_eye}, cv::Scalar(0));
    return mask;
}

} // namespace segmecam
