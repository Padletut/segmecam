#include "include/application/gpu_setup.h"

#include <iostream>

namespace segmecam {

GPUSetupState GPUSetup::DetectAndSetupGPU() {
    std::cout << "🔍 Detecting GPU capabilities..." << std::endl;
    
    GPUSetupState state = CheckEnvironmentVariables();
    
    if (state.force_cpu) {
        std::cout << "🧪 TESTING MODE: Forced CPU-only via SEGMECAM_FORCE_CPU" << std::endl;
        state.gpu_caps.backend = GPUBackend::CPU_ONLY;
        state.gpu_caps.environment = GPUDetector::DetectEnvironment();
        state.gpu_caps.vendor = "CPU (Forced)";
        state.gpu_caps.egl_available = false;
        state.gpu_caps.opengl_available = false;
    } else {
        state.gpu_caps = DetectCapabilities(state.force_no_nvidia, state.force_no_mesa);
        if (state.force_no_nvidia) {
            std::cout << "🧪 TESTING MODE: NVIDIA disabled via SEGMECAM_NO_NVIDIA" << std::endl;
        }
        if (state.force_no_mesa) {
            std::cout << "🧪 TESTING MODE: Mesa disabled via SEGMECAM_NO_MESA" << std::endl;
        }
    }
    
    state.use_gpu = (state.gpu_caps.backend != GPUBackend::CPU_ONLY);
    
    PrintGPUInfo(state);
    
    // Setup optimal EGL path for GPU if available
    if (state.use_gpu) {
        std::cout << "🚀 Setting up optimal EGL path for GPU..." << std::endl;
        GPUDetector::SetupOptimalEGLPath(state.gpu_caps);
    }
    
    return state;
}

void GPUSetup::PrintGPUInfo(const GPUSetupState& state) {
    std::cout << "🖥️  Environment: " << 
        (state.gpu_caps.environment == RuntimeEnvironment::FLATPAK ? "Flatpak" :
         state.gpu_caps.environment == RuntimeEnvironment::DOCKER ? "Docker" : "Native") << std::endl;
    
    std::cout << "🎮 GPU Backend: " << 
        (state.gpu_caps.backend == GPUBackend::NVIDIA_EGL ? "NVIDIA EGL" :
         state.gpu_caps.backend == GPUBackend::MESA_EGL ? "Mesa EGL" :
         state.gpu_caps.backend == GPUBackend::AMD_RADEON ? "AMD Radeon" :
         state.gpu_caps.backend == GPUBackend::INTEL_GPU ? "Intel GPU" : "CPU Only") << std::endl;
}

GPUSetupState GPUSetup::CheckEnvironmentVariables() {
    GPUSetupState state;
    state.force_cpu = (std::getenv("SEGMECAM_FORCE_CPU") != nullptr);
    state.force_no_nvidia = (std::getenv("SEGMECAM_NO_NVIDIA") != nullptr);
    state.force_no_mesa = (std::getenv("SEGMECAM_NO_MESA") != nullptr);
    return state;
}

GPUCapabilities GPUSetup::DetectCapabilities(bool force_no_nvidia, bool force_no_mesa) {
    return GPUDetector::DetectGPUCapabilitiesForTesting(force_no_nvidia, force_no_mesa);
}

} // namespace segmecam