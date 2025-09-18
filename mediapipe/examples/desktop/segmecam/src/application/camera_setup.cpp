#include "include/application/camera_setup.h"

bool CameraSetup::SetupCamera(CameraSetupState& state, cv::VideoCapture& cap) {
    std::cout << "Setting up camera..." << std::endl;
    
    // Enumerate available cameras
    EnumerateCameras(state);
    
    // Print available camera information
    PrintCameraInfo(state);
    
    // Validate camera selection
    ValidateCameraSelection(state);
    
    // Initialize camera capture
    if (!InitializeCapture(state, cap)) {
        std::cerr << "Error: Failed to initialize camera capture" << std::endl;
        return false;
    }
    
    std::cout << "Camera setup completed successfully" << std::endl;
    return true;
}void CameraSetup::EnumerateCameras(CameraSetupState& state) {
    state.camera_infos = ::EnumerateCameras();  // Use the global function from cam_enum.h
    
    if (state.camera_infos.empty()) {
        std::cerr << "Warning: No cameras found during enumeration" << std::endl;
        return;
    }
    
    // Use first camera by default if none selected
    if (state.camera_id >= static_cast<int>(state.camera_infos.size())) {
        std::cerr << "Warning: Selected camera ID " << state.camera_id 
                  << " out of range, using camera 0" << std::endl;
        state.camera_id = 0;
    }
    
    if (state.camera_id < static_cast<int>(state.camera_infos.size())) {
        state.selected_camera_name = state.camera_infos[state.camera_id].name;
    }
}

void CameraSetup::PrintCameraInfo(const CameraSetupState& state) {
    std::cout << "Available cameras:" << std::endl;
    for (size_t i = 0; i < state.camera_infos.size(); ++i) {
        const auto& info = state.camera_infos[i];
        std::cout << "  [" << i << "] " << info.name 
                  << " (index: " << info.index << ")";
        if (static_cast<int>(i) == state.camera_id) {
            std::cout << " [SELECTED]";
        }
        std::cout << std::endl;
    }
}

bool CameraSetup::InitializeCapture(const CameraSetupState& state, cv::VideoCapture& cap) {
    std::cout << "Initializing camera " << state.camera_id
              << " (" << state.selected_camera_name << ")" << std::endl;

    // Open the camera
    cap.open(state.camera_id);
    if (!cap.isOpened()) {
        std::cerr << "Error: Failed to open camera " << state.camera_id << std::endl;
        return false;
    }

    // Configure camera settings
    if (!ConfigureCamera(cap, state.width, state.height)) {
        std::cerr << "Warning: Failed to configure camera settings" << std::endl;
        // Continue anyway - some cameras may not support all settings
    }

    std::cout << "Camera initialized successfully" << std::endl;
    return true;
}bool CameraSetup::ConfigureCamera(cv::VideoCapture& cap, int width, int height) {
    // Set frame size
    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);

    // Verify actual settings
    double actual_width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    double actual_height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);

    std::cout << "Camera configured: " << actual_width << "x" << actual_height << std::endl;
    return true;
}

void CameraSetup::ValidateCameraSelection(CameraSetupState& state) {
    if (state.camera_infos.empty()) {
        std::cerr << "Error: No cameras available for selection" << std::endl;
        return;
    }
    
    if (state.camera_id < 0 || state.camera_id >= static_cast<int>(state.camera_infos.size())) {
        std::cerr << "Warning: Invalid camera ID " << state.camera_id 
                  << ", defaulting to camera 0" << std::endl;
        state.camera_id = 0;
    }
    
    // Update selected camera name
    state.selected_camera_name = state.camera_infos[state.camera_id].name;
    
    std::cout << "Selected camera: " << state.selected_camera_name 
              << " (ID: " << state.camera_id << ")" << std::endl;
}
