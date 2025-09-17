#ifndef CAMERA_SETUP_H
#define CAMERA_SETUP_H

#include <string>
#include <vector>
#include <iostream>
#include "cam_enum.h"
#include "mediapipe/framework/port/opencv_core_inc.h"
#include "mediapipe/framework/port/opencv_highgui_inc.h"

struct CameraSetupState {
    int camera_id = 0;
    int width = 1280;
    int height = 720;
    std::vector<CameraDesc> camera_infos;  // Changed to CameraDesc from CameraInfo
    std::string selected_camera_name;
    
    CameraSetupState() = default;
};

class CameraSetup {
public:
    static bool SetupCamera(CameraSetupState& state, cv::VideoCapture& cap);
    static void EnumerateCameras(CameraSetupState& state);
    static void PrintCameraInfo(const CameraSetupState& state);
    static bool InitializeCapture(const CameraSetupState& state, cv::VideoCapture& cap);
    
private:
    static bool ConfigureCamera(cv::VideoCapture& cap, int width, int height);
    static void ValidateCameraSelection(CameraSetupState& state);
};

#endif // CAMERA_SETUP_H