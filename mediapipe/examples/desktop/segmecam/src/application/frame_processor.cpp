#include "include/application/frame_processor.h"
#include "include/application/mediapipe_processor.h"
#include "include/application/render_utils.h"
#include "include/application/portal_utils.h"
#include "include/application/application_run.h"
#include "include/effects/effects_manager.h"
#include "include/ui/ui_manager_enhanced.h"
#include "include/ui/ui_utils.h"
#include "include/application/app_state.h"
#include "include/ar_filters/model_loader.h"
#include "include/ar_filters/ar_filter_manager.h"  // Phase 8: For ARFilterManager methods

// For 3D rendering (Phase 3 Step 8)
#include "include/render/render_manager.h"
#include "ar_filters/opengl_renderer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Include MediaPipe for graph operations
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"

// Include OpenCV for image processing
#include <opencv2/opencv.hpp>

// Include SDL for timing
#include <SDL.h>

#include <iostream>
#include <chrono>
#include <thread>

namespace segmecam {

// Helper function to create RenderCommand from FilterObject (Phase 3 Step 8)
static segmecam::ar_filters::RenderCommand CreateRenderCommand(const FilterObject& filter, const AppState& app_state) {
    segmecam::ar_filters::RenderCommand cmd;
    
    // Set model pointer
    cmd.model = filter.model.get();
    cmd.visible = filter.visible && filter.enabled;
    
    // Build model matrix from position, rotation, scale
    glm::mat4 model_matrix = glm::mat4(1.0f);
    
    // Apply position (convert to NDC coordinates centered at origin)
    glm::vec3 position(filter.position[0], filter.position[1], filter.position[2]);
    model_matrix = glm::translate(model_matrix, position);
    
    // Apply rotation (in degrees)
    model_matrix = glm::rotate(model_matrix, glm::radians(filter.rotation[2]), glm::vec3(0.0f, 0.0f, 1.0f)); // Z
    model_matrix = glm::rotate(model_matrix, glm::radians(filter.rotation[1]), glm::vec3(0.0f, 1.0f, 0.0f)); // Y
    model_matrix = glm::rotate(model_matrix, glm::radians(filter.rotation[0]), glm::vec3(1.0f, 0.0f, 0.0f)); // X
    
    // Apply scale
    glm::vec3 scale(
        filter.scale[0] * filter.local_scale,
        filter.scale[1] * filter.local_scale,
        filter.scale[2] * filter.local_scale
    );
    model_matrix = glm::scale(model_matrix, scale);
    
    // Copy to command (column-major storage)
    memcpy(cmd.model_matrix, glm::value_ptr(model_matrix), 16 * sizeof(float));
    
    // Get material (use first material or default)
    if (filter.model && !filter.model->materials.empty()) {
        // Try to get mesh material first, fallback to first material
        if (!filter.model->meshes.empty()) {
            const std::string& material_name = filter.model->meshes[0].material_name;
            auto mat_it = filter.model->materials.find(material_name);
            if (mat_it != filter.model->materials.end()) {
                cmd.material = mat_it->second;
            } else {
                cmd.material = filter.model->materials.begin()->second;
            }
        } else {
            cmd.material = filter.model->materials.begin()->second;
        }
    } else {
        // Default material (white diffuse)
        cmd.material.ambient = cv::Vec3f(0.2f, 0.2f, 0.2f);
        cmd.material.diffuse = cv::Vec3f(1.0f, 1.0f, 1.0f);
        cmd.material.specular = cv::Vec3f(0.5f, 0.5f, 0.5f);
        cmd.material.shininess = 32.0f;
        cmd.material.opacity = 1.0f;
        cmd.material.texture_id = 0;
    }
    
    return cmd;
}

void FrameProcessor::MatToImageFrame(const cv::Mat& mat_bgr, std::unique_ptr<mediapipe::ImageFrame>& frame) {
    frame = std::make_unique<mediapipe::ImageFrame>(
        mediapipe::ImageFormat::SRGB, mat_bgr.cols, mat_bgr.rows);
    cv::Mat frame_rgb;
    cv::cvtColor(mat_bgr, frame_rgb, cv::COLOR_BGR2RGB);
    frame_rgb.copyTo(cv::Mat(frame->Height(), frame->Width(), CV_8UC3, frame->MutablePixelData()));
}

bool FrameProcessor::ProcessFrameCapture(CameraManager& camera_mgr, cv::Mat& frame_bgr, int frame_count) {
    if (frame_count <= 5) {
        std::cout << "📸 Calling CaptureFrame()..." << std::endl;
    }
    if (!camera_mgr.CaptureFrame(frame_bgr) || frame_bgr.empty()) {
        if (frame_count < 10) {  // Only log first few failures
            std::cout << "⚠️  Frame capture failed or empty on frame " << frame_count << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        return false;
    }

    if (frame_count <= 5) {
        std::cout << "✅ Frame " << frame_count << " captured successfully: " << frame_bgr.cols << "x" << frame_bgr.rows << std::endl;
        // Debug: Show camera frame format and sample pixel values
        if (!frame_bgr.empty()) {
            cv::Vec3b center_pixel = frame_bgr.at<cv::Vec3b>(frame_bgr.rows/2, frame_bgr.cols/2);
            cv::Vec3b corner_pixel = frame_bgr.at<cv::Vec3b>(10, 10);
            std::cout << "🔍 CAMERA DEBUG - Frame format: " << frame_bgr.type() << " (CV_8UC3=" << CV_8UC3 << ")" << std::endl;
            std::cout << "🔍 CAMERA DEBUG - Center pixel BGR(" << frame_bgr.rows/2 << "," << frame_bgr.cols/2 << "): ["
                      << (int)center_pixel[0] << "," << (int)center_pixel[1] << "," << (int)center_pixel[2] << "]" << std::endl;
            std::cout << "🔍 CAMERA DEBUG - Corner pixel BGR(10,10): ["
                      << (int)corner_pixel[0] << "," << (int)corner_pixel[1] << "," << (int)corner_pixel[2] << "]" << std::endl;
        }
    }
    return true;
}

void FrameProcessor::UpdateFPSTracking(double& fps, uint64_t& fps_frames, uint32_t& fps_last_ms) {
    fps_frames++;
    uint32_t now_ms = SDL_GetTicks();
    if (now_ms - fps_last_ms >= 500) {
        fps = (double)fps_frames * 1000.0 / (double)(now_ms - fps_last_ms);
        fps_frames = 0;
        fps_last_ms = now_ms;
    }
}

void FrameProcessor::UpdateCameraInfo(FrameProcessingParams& params) {
    params.app_state.fps = params.fps;
    params.app_state.camera_width = params.managers.camera->GetCurrentWidth();
    params.app_state.camera_height = params.managers.camera->GetCurrentHeight();
    params.app_state.camera_fps = params.managers.camera->GetCurrentFPS();
}

void FrameProcessor::UpdateAutoFPS(FrameProcessingParams& params) {
    // Use actual measured FPS instead of configured camera FPS
    // This accounts for dynamic framerate changes (e.g., exposure priority dropping 30→15 FPS)
    static float last_measured_fps = -1.0f;
    if (std::abs(params.fps - last_measured_fps) > 0.1f) {
        params.managers.effects->UpdateTargetFPSFromCamera(params.fps);
        params.app_state.target_fps = params.managers.effects->GetTargetFPS();
        last_measured_fps = params.fps;
    }
}

void FrameProcessor::UpdateAutoProcessingScale(FrameProcessingParams& params) {
    if (params.app_state.auto_processing_scale && params.fps > 0.0) {
        params.managers.effects->UpdateAutoProcessingScale(params.fps);
        params.app_state.current_fps = params.managers.effects->GetCurrentFPS();
        params.app_state.fx_adv_scale = params.managers.effects->GetProcessingScale();
    }
}

bool FrameProcessor::SendFrameToMediaPipe(const cv::Mat& frame_bgr, FrameProcessingParams& params) {
    std::unique_ptr<mediapipe::ImageFrame> frame;
    MatToImageFrame(frame_bgr, frame);
    auto ts = mediapipe::Timestamp(params.frame_id++);
    auto st = params.mediapipe_graph->AddPacketToInputStream("input_video", mediapipe::Adopt(frame.release()).At(ts));
    if (!st.ok()) {
        std::cerr << "❌ AddPacket failed: " << st.message() << std::endl;
        return false;
    }
    return true;
}

cv::Mat FrameProcessor::ProcessAndDisplayFrame(
    const cv::Mat& frame_bgr,
    const cv::Mat& last_mask_u8,
    const mediapipe::NormalizedLandmarkList* landmarks_ptr,
    EffectsManager& effects_mgr,
    AppState& app_state,
    bool have_lms) {

    cv::Mat processed_frame = frame_bgr;

    // Apply effects if EffectsManager is available
    if (!last_mask_u8.empty() || have_lms) {
        // Use EffectsManager to process the frame with segmentation mask and face landmarks
        processed_frame = effects_mgr.ProcessFrame(frame_bgr, last_mask_u8, landmarks_ptr);
        
        // Copy facial metrics for debug display
        app_state.last_facial_metrics = effects_mgr.GetLastFacialMetrics();
    }

    // EffectsManager now returns RGB directly, no conversion needed
    cv::Mat display_rgb = processed_frame.clone();

    // Update app state with current frame
    app_state.last_display_rgb = display_rgb.clone();

    return display_rgb;
}

void FrameProcessor::handle_auto_vcam_selection(
    CameraManager& camera_mgr,
    AppState& app_state,
    const cv::Mat& display_rgb) {

    if (!app_state.vcam.IsOpen() && app_state.vcam_auto_start) {
        auto vcam_list = camera_mgr.GetVCamList();
        if (!vcam_list.empty()) {
            app_state.ui_vcam_idx = 0;
            app_state.virtual_camera_path = vcam_list[0].path;
            app_state.vcam.Open(vcam_list[0].path, display_rgb.cols, display_rgb.rows);
            std::cout << "📹 Auto-selected virtual camera: " << vcam_list[0].name << " (" << vcam_list[0].path << ")" << std::endl;
        }
    }
}

void FrameProcessor::handle_vcam_resize(
    CameraManager& camera_mgr,
    AppState& app_state,
    const cv::Mat& display_rgb) {

    if (app_state.vcam.IsOpen() && (display_rgb.cols != app_state.vcam.Width() || display_rgb.rows != app_state.vcam.Height())) {
        auto vcam_list = camera_mgr.GetVCamList();
        if (app_state.ui_vcam_idx >= 0 && app_state.ui_vcam_idx < (int)vcam_list.size()) {
            app_state.vcam.Open(vcam_list[app_state.ui_vcam_idx].path, display_rgb.cols, display_rgb.rows);
        }
    }
}

void FrameProcessor::handle_frame_output(
    CameraManager& camera_mgr,
    AppState& app_state,
    const cv::Mat& display_rgb) {

    cv::Mat display_bgr;
    cv::cvtColor(display_rgb, display_bgr, cv::COLOR_RGB2BGR);

    if (app_state.vcam.IsOpen()) {
        app_state.vcam.WriteBGR(display_bgr);
    }

    if (camera_mgr.IsPipeWireOutputActive()) {
        camera_mgr.SendFrameToPipeWire(display_bgr);
    }
}

void FrameProcessor::RenderFilterPrimitives(cv::Mat& display_bgr, AppState& app_state) {
    // Phase 2 Step 5 → Phase 3 Step 8: AR filter visualization
    // Renders filter objects as:
    //  - 3D models (when ar_render_3d_models enabled) [NEW in Step 8]
    //  - Colored circles/shapes for 2D primitives or fallback
    
    if (!app_state.ar_filters_enabled) {
        return;
    }
    
    const auto& filters = app_state.attachment_controller.GetActiveFilters();
    if (filters.empty()) {
        return;
    }
    
    // Phase 3 Step 8: Render 3D models with OpenGL if enabled
    // NOTE: This currently renders to the default framebuffer (screen)
    // In Phase 4, we'll render to an FBO and composite with the video frame
    // For now, this is a proof-of-concept showing actual 3D models
    
    // TODO Phase 4: Implement FBO rendering and proper compositing
    //   1. Create FBO with color texture and depth buffer
    //   2. Render 3D models to FBO
    //   3. Read back FBO texture and composite with video frame
    //   4. This will eliminate the slow glReadPixels path
    
    // For now: Fall back to 2D circle visualization
    // (OpenGL 3D rendering requires FBO integration which is Phase 4 work)
    
    // Render each visible filter
    for (const auto& filter : filters) {
        if (!filter.visible || !filter.enabled) {
            continue;
        }
        
        // Get filter 2D position (x, y are already in pixel coordinates)
        cv::Point2f pos(filter.position[0], filter.position[1]);
        
        // Check if position is within frame bounds
        if (pos.x < 0 || pos.y < 0 || pos.x >= display_bgr.cols || pos.y >= display_bgr.rows) {
            continue;
        }
        
        // Determine color based on filter type
        cv::Scalar color;
        std::string type_label;
        
        if (filter.type == FilterObject::Type::MODEL_3D && filter.model != nullptr) {
            // For 3D models, use material diffuse color if available
            type_label = "[3D] ";
            if (!filter.model->materials.empty() && !filter.model->meshes.empty()) {
                // Get the material used by the first mesh
                const std::string& material_name = filter.model->meshes[0].material_name;
                auto mat_it = filter.model->materials.find(material_name);
                
                if (mat_it != filter.model->materials.end()) {
                    // Use the mesh's material
                    const auto& material = mat_it->second;
                    color = cv::Scalar(
                        static_cast<int>(material.diffuse[2] * 255),  // B
                        static_cast<int>(material.diffuse[1] * 255),  // G
                        static_cast<int>(material.diffuse[0] * 255)   // R
                    );
                } else {
                    // Fallback: use first material if mesh material not found
                    const auto& first_material = filter.model->materials.begin()->second;
                    color = cv::Scalar(
                        static_cast<int>(first_material.diffuse[2] * 255),  // B
                        static_cast<int>(first_material.diffuse[1] * 255),  // G
                        static_cast<int>(first_material.diffuse[0] * 255)   // R
                    );
                }
            } else {
                // Default to cyan for 3D models without materials
                color = cv::Scalar(255, 255, 0);  // Cyan
            }
        } else {
            // For primitives, use filter color
            type_label = "[2D] ";
            color = cv::Scalar(
                static_cast<int>(filter.color[2] * 255),  // B
                static_cast<int>(filter.color[1] * 255),  // G
                static_cast<int>(filter.color[0] * 255)   // R
            );
        }
        
        // Calculate radius based on scale (simple heuristic)
        float avg_scale = (filter.scale[0] + filter.scale[1] + filter.scale[2]) / 3.0f;
        int radius = static_cast<int>(avg_scale * filter.local_scale * 50.0f);  // Scale to screen space
        radius = std::max(8, std::min(60, radius));  // Clamp to reasonable range
        
        // Draw the filter as a filled circle
        cv::circle(display_bgr, pos, radius, color, -1, cv::LINE_AA);
        
        // Draw border (thicker for 3D models to distinguish them)
        int border_thickness = (filter.type == FilterObject::Type::MODEL_3D) ? 3 : 2;
        cv::Scalar border_color = (filter.type == FilterObject::Type::MODEL_3D) 
                                  ? cv::Scalar(0, 255, 255)  // Yellow border for 3D
                                  : cv::Scalar(255, 255, 255);  // White border for 2D
        cv::circle(display_bgr, pos, radius, border_color, border_thickness, cv::LINE_AA);
        
        // Phase 3 Step 8: Draw "3D" marker for MODEL_3D types
        if (filter.type == FilterObject::Type::MODEL_3D && filter.model != nullptr) {
            std::string mode_label = app_state.ar_render_3d_models ? "[3D GL]" : "[3D CIRCLE]";
            cv::Point2f mode_pos = pos + cv::Point2f(-30, -radius - 10);
            cv::putText(display_bgr, mode_label, mode_pos,
                       cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
        }
        
        // Draw filter name and info
        std::string label = type_label + filter.name;
        if (!filter.anchor_name.empty()) {
            label += " → " + filter.anchor_name;
        }
        
        cv::Point2f text_pos = pos + cv::Point2f(radius + 5, 5);
        cv::putText(display_bgr, label, text_pos,
                   cv::FONT_HERSHEY_SIMPLEX, 0.4, color, 1, cv::LINE_AA);
        
        // Draw additional info for 3D models
        if (filter.type == FilterObject::Type::MODEL_3D && filter.model != nullptr) {
            std::string model_info = "Meshes:" + std::to_string(filter.model->meshes.size()) +
                                   " V:" + std::to_string(filter.model->vertex_count);
            cv::Point2f info_pos = pos + cv::Point2f(radius + 5, 20);
            cv::putText(display_bgr, model_info, info_pos,
                       cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
        }
        
        // Draw frame count for debugging
        std::string frame_info = "f:" + std::to_string(filter.frame_count);
        cv::Point2f frame_info_pos = pos + cv::Point2f(radius + 5, -10);
        cv::putText(display_bgr, frame_info, frame_info_pos,
                   cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(200, 200, 200), 1, cv::LINE_AA);
    }
    
    // Draw statistics in corner
    auto stats = app_state.attachment_controller.GetStatistics();
    if (stats.total_filters > 0) {
        std::string info = "AR Filters: " + std::to_string(stats.visible_filters) + "/" + 
                          std::to_string(stats.total_filters) + " visible";
        cv::putText(display_bgr, info, cv::Point(10, display_bgr.rows - 30),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
        
        std::string perf = "Update: " + std::to_string(stats.average_update_time_ms).substr(0, 4) + "ms";
        cv::putText(display_bgr, perf, cv::Point(10, display_bgr.rows - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    }
}

void FrameProcessor::HandleVirtualCameraOutput(
    CameraManager& camera_mgr,
    AppState& app_state,
    const cv::Mat& display_rgb) {

    if (display_rgb.empty()) {
        return;
    }

    handle_auto_vcam_selection(camera_mgr, app_state, display_rgb);
    handle_vcam_resize(camera_mgr, app_state, display_rgb);
    handle_frame_output(camera_mgr, app_state, display_rgb);
}

void FrameProcessor::HandleDroppedFiles(
    UIManager& ui_manager,
    AppState& app_state) {

    auto dropped_files = ui_manager.GetDroppedFiles();
    for (const auto& file_path : dropped_files) {
        std::cout << "🖼️  Processing dropped file: " << file_path << std::endl;
        // Load image and set as background
        cv::Mat bg_image;
        std::string resolved_path;
        if (LoadBackgroundImageWithPortal(file_path, bg_image, resolved_path)) {
            app_state.bg_image = bg_image.clone();
            app_state.bg_mode = 2; // Image mode
            // Update the background path for profile persistence
            // Safe string copy with null-termination guarantee
            ui_utils::SafeStringCopy(app_state.bg_path_buf, sizeof(app_state.bg_path_buf), resolved_path);
            std::cout << "✅ Background image loaded: " << bg_image.cols << "x" << bg_image.rows
                      << " (auto-switched to Image mode)" << std::endl;
            std::cout << "🔖 Background path saved: " << app_state.bg_path_buf << std::endl;
        } else {
            std::cout << "❌ Failed to load dropped file as image after portal fallback." << std::endl;
        }
    }
}

bool FrameProcessor::ProcessFrame(FrameProcessingParams& params) {
    cv::Mat frame_bgr;
    cv::Mat display_rgb;

    // Process frame capture and initial updates
    if (!ProcessFrameCaptureAndUpdates(params, frame_bgr)) {
        return true; // Continue to next frame
    }

    // Process MediaPipe and effects
    if (!ProcessFrameMediaPipeAndEffects(params, frame_bgr, display_rgb)) {
        return false; // Exit on error
    }

    // Handle UI events and rendering
    return ProcessFrameUIAndRender(params, display_rgb);
}

bool FrameProcessor::ProcessFrameCaptureAndUpdates(FrameProcessingParams& params, cv::Mat& frame_bgr) {
    // Process frame capture
    if (!ProcessFrameCapture(*params.managers.camera, frame_bgr, params.frame_count)) {
        return false; // Continue to next frame
    }

    // Update FPS tracking
    UpdateFPSTracking(params.fps, params.fps_frames, params.fps_last_ms);

    // Update camera information in app state
    UpdateCameraInfo(params);

    // Update auto FPS settings if camera changed
    UpdateAutoFPS(params);

    // Update auto processing scale if enabled
    UpdateAutoProcessingScale(params);

    return true;
}

bool FrameProcessor::ProcessFrameMediaPipeAndEffects(FrameProcessingParams& params, const cv::Mat& frame_bgr, cv::Mat& display_rgb) {
    // Send frame to MediaPipe graph
    if (!SendFrameToMediaPipe(frame_bgr, params)) {
        return false; // Exit on error
    }

    // Process MediaPipe outputs
    MediaPipeOutputData output_data;
    MediaPipeProcessor::ProcessMediaPipeOutputs(output_data, params.mask_poller, params.multi_face_landmarks_poller, 
                           params.face_rects_poller, params.blendshapes_poller,
                           params.app_state, params.frame_count, params.has_landmarks);

    // Sync UI settings to EffectsManager before processing effects
    ApplicationRun::SyncSettingsToEffectsManager(*params.managers.effects, params.app_state);

    // Process frame effects and prepare for display
    const mediapipe::NormalizedLandmarkList* landmarks_ptr = (output_data.have_lms) ? &output_data.latest_lms : nullptr;
    display_rgb = ProcessAndDisplayFrame(frame_bgr, output_data.last_mask_u8, landmarks_ptr,
                                        *params.managers.effects, params.app_state, output_data.have_lms);

    // Phase 8 Day 4: Update and render AR filters directly to GPU texture
    // Uses new RenderToTexture() method - no CPU readback, no freeze!
    if (params.managers.ar_filter_manager && params.app_state.ar_filters_enabled) {
        // Update AR filter transforms based on face landmarks
        if (output_data.have_lms && params.managers.ar_filter_manager->HasActiveFilter()) {
            // Convert protobuf repeated field to vector
            std::vector<mediapipe::NormalizedLandmark> landmarks_vec(
                output_data.latest_lms.landmark().begin(),
                output_data.latest_lms.landmark().end()
            );
            
            // **Phase 8 Day 4: Convert BGR to RGB and pass to MiDaS depth estimator**
            cv::Mat frame_rgb;
            cv::cvtColor(frame_bgr, frame_rgb, cv::COLOR_BGR2RGB);
            
            params.managers.ar_filter_manager->Update(
                landmarks_vec,
                params.app_state.head_pose,  // Pass head pose from MediaPipe
                frame_bgr.cols,
                frame_bgr.rows,
                frame_rgb  // Phase 8 Day 4: Pass RGB frame for MiDaS depth estimation
            );
            
            // **NEW: Update debug anchor visualization from UI toggle (Finding #8)**
            params.managers.ar_filter_manager->SetDebugAnchorsEnabled(params.app_state.show_anchors);
            
            // Note: AR rendering will be done directly on GPU texture after upload
            // See below where we call RenderToTexture() after texture upload
        }
    }

    // Add face mesh visualization overlay if enabled
    if (params.app_state.show_mesh && params.app_state.face_mesh_available) {
        cv::Mat display_bgr;
        cv::cvtColor(display_rgb, display_bgr, cv::COLOR_RGB2BGR);
        
        // Use face_processor to draw face mesh (need to access through effects_manager's face_processor)
        // For now, draw simple overlay showing face mesh is available
        std::string info = cv::format("Face Mesh Active: 478 pts | Yaw: %.1f° | Pitch: %.1f° | Scale: %.3f",
                                      params.app_state.face_mesh.euler_angles[1],
                                      params.app_state.face_mesh.euler_angles[0],
                                      params.app_state.face_mesh.scale);
        cv::putText(display_bgr, info, cv::Point(10, 30),
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
        
        // Draw key attachment points
        auto drawKeyPoint = [&](const cv::Point2f& pt, const cv::Scalar& color, const std::string& label = "") {
            if (pt.x >= 0 && pt.y >= 0 && pt.x < display_bgr.cols && pt.y < display_bgr.rows) {
                cv::circle(display_bgr, pt, 5, color, 2, cv::LINE_AA);
                if (!label.empty()) {
                    cv::putText(display_bgr, label, pt + cv::Point2f(8, 0),
                               cv::FONT_HERSHEY_SIMPLEX, 0.4, color, 1, cv::LINE_AA);
                }
            }
        };
        
        drawKeyPoint(params.app_state.face_mesh.GetNoseTip2D(), cv::Scalar(0, 255, 0), "nose");
        drawKeyPoint(params.app_state.face_mesh.GetNoseBridge2D(), cv::Scalar(0, 255, 255), "bridge");
        drawKeyPoint(params.app_state.face_mesh.GetForeheadCenter2D(), cv::Scalar(255, 0, 255), "forehead");
        drawKeyPoint(params.app_state.face_mesh.GetLeftTemple2D(), cv::Scalar(255, 128, 0), "L temple");
        drawKeyPoint(params.app_state.face_mesh.GetRightTemple2D(), cv::Scalar(255, 128, 0), "R temple");
        drawKeyPoint(params.app_state.face_mesh.GetChinCenter2D(), cv::Scalar(128, 255, 0), "chin");
        
        cv::cvtColor(display_bgr, display_rgb, cv::COLOR_BGR2RGB);
    }
    
    // Add anchor point visualization if enabled (Phase 2 Step 4: Requires transform data)
    if (params.app_state.show_anchors && params.app_state.transform_data_available) {
        cv::Mat display_bgr;
        cv::cvtColor(display_rgb, display_bgr, cv::COLOR_RGB2BGR);
        
        // Draw anchor points using FaceProcessor
        params.managers.effects->GetFaceProcessor().DrawAnchorPoints(display_bgr, params.app_state, true);
        
        cv::cvtColor(display_bgr, display_rgb, cv::COLOR_BGR2RGB);
    }
    
    // Render AR filter primitives (Phase 2 Step 5)
    // 🚨 DISABLED when Phase 8 GPU rendering is active (conflicts with GPU-to-GPU pipeline)
    // Phase 3 draws 2D circles on CPU which overwrites the GPU-rendered glasses
    if (false && params.app_state.ar_filters_enabled && params.app_state.transform_data_available) {
        cv::Mat display_bgr;
        cv::cvtColor(display_rgb, display_bgr, cv::COLOR_RGB2BGR);
        
        // Draw filter primitives at their calculated positions
        RenderFilterPrimitives(display_bgr, params.app_state);
        
        cv::cvtColor(display_bgr, display_rgb, cv::COLOR_BGR2RGB);
    }

    // Handle virtual camera output
    HandleVirtualCameraOutput(*params.managers.camera, params.app_state, display_rgb);

    return true;
}

bool FrameProcessor::ProcessFrameUIAndRender(FrameProcessingParams& params, const cv::Mat& display_rgb) {
    // Let UIManager handle events first (pass AR filter manager for keyboard controls)
    if (!params.ui_manager.ProcessEvents(params.running, params.managers.ar_filter_manager.get())) {
        std::cout << "🛑 UIManager ProcessEvents returned false, exiting..." << std::endl;
        return false;
    }

    // Handle any dropped files
    HandleDroppedFiles(params.ui_manager, params.app_state);

    if (!params.running) {
        std::cout << "🛑 Running flag set to false by event handler, exiting..." << std::endl;
        return false;
    }

    // Update UI texture with processed video frame
    if (!display_rgb.empty()) {
        params.ui_manager.UploadTexture(display_rgb);
        
        // Phase 8 Day 4: Render AR filters directly to GPU texture (GPU-to-GPU, no CPU copy)
        if (params.managers.ar_filter_manager && 
            params.app_state.ar_filters_enabled && 
            params.managers.ar_filter_manager->HasActiveFilter()) {
            
            // Get the video texture ID from UI manager
            unsigned int video_texture_id = params.ui_manager.GetTexture();
            
            static int debug_render_count = 0;
            if (debug_render_count < 3 || debug_render_count % 60 == 0) {
                ABSL_LOG(INFO) << "🎨 AR Render attempt #" << debug_render_count 
                               << " - texture_id=" << video_texture_id
                               << " size=" << display_rgb.cols << "x" << display_rgb.rows;
            }
            
            if (video_texture_id != 0) {
                // Render AR filters directly onto the video texture
                auto render_status = params.managers.ar_filter_manager->RenderToTexture(
                    video_texture_id, 
                    display_rgb.cols, 
                    display_rgb.rows
                );
                
                if (!render_status.ok()) {
                    ABSL_LOG(WARNING) << "AR filter GPU rendering failed: " 
                                      << render_status.message();
                } else if (debug_render_count < 3) {
                    ABSL_LOG(INFO) << "✅ AR render successful!";
                }
            } else {
                if (debug_render_count < 3) {
                    ABSL_LOG(WARNING) << "❌ Video texture ID is 0, cannot render AR filters";
                }
            }
            debug_render_count++;
        }
    }

    // Render complete frame
    // Skip texture upload if AR filters were rendered (already on GPU texture)
    bool ar_was_rendered = params.managers.ar_filter_manager && 
                           params.app_state.ar_filters_enabled && 
                           params.managers.ar_filter_manager->HasActiveFilter();
    RenderUtils::RenderFrame(params.ui_manager, display_rgb, params.window, params.frame_count, params.running, ar_was_rendered);

    return true;
}

} // namespace segmecam
