#include "include/mediapipe_manager/gpu_detector.h"
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <dlfcn.h>
#include <EGL/egl.h>
#include <GL/gl.h>

bool GPUDetector::testing_mode = false;

GPUCapabilities GPUDetector::DetectGPUCapabilities() {
    return DetectGPUCapabilitiesForTesting(false, false);
}

GPUCapabilities GPUDetector::DetectGPUCapabilitiesForTesting(bool force_no_nvidia, bool force_no_mesa) {
    GPUCapabilities caps;
    
    // Detect runtime environment first
    caps.environment = DetectEnvironment();
    
    // Find available EGL libraries
    caps.egl_library_paths = FindEGLLibraries();
    
    // Test different GPU backends in priority order
    GPUBackend detected_backend = DetectBestGPUBackend(force_no_nvidia, force_no_mesa);
    SetCapabilitiesFromBackend(caps, detected_backend);
    
    return caps;
}

GPUBackend GPUDetector::DetectBestGPUBackend(bool force_no_nvidia, bool force_no_mesa) {
    // Test backends in priority order
    GPUBackend backend = TestGPUBackendsInPriority(force_no_nvidia, force_no_mesa);
    return backend != GPUBackend::NONE ? backend : GPUBackend::CPU_ONLY;
}

GPUBackend GPUDetector::TestGPUBackendsInPriority(bool force_no_nvidia, bool force_no_mesa) {
    if (!force_no_nvidia && TestNvidiaEGL()) {
        return GPUBackend::NVIDIA_EGL;
    }
    if (!force_no_mesa && TestAMDRadeon()) {
        return GPUBackend::AMD_RADEON;
    }
    if (!force_no_mesa && TestIntelGPU()) {
        return GPUBackend::INTEL_GPU;
    }
    if (!force_no_mesa && TestMesaEGL()) {
        return GPUBackend::MESA_EGL;
    }
    return GPUBackend::NONE;
}

void GPUDetector::SetCapabilitiesFromBackend(GPUCapabilities& caps, GPUBackend backend) {
    switch (backend) {
        case GPUBackend::NVIDIA_EGL:
            caps.backend = GPUBackend::NVIDIA_EGL;
            caps.egl_available = true;
            caps.opengl_available = true;
            caps.vendor = "NVIDIA";
            break;
        case GPUBackend::AMD_RADEON:
            caps.backend = GPUBackend::AMD_RADEON;
            caps.egl_available = true;
            caps.opengl_available = true;
            caps.vendor = "AMD Radeon";
            break;
        case GPUBackend::INTEL_GPU:
            caps.backend = GPUBackend::INTEL_GPU;
            caps.egl_available = true;
            caps.opengl_available = true;
            caps.vendor = "Intel GPU";
            break;
        case GPUBackend::MESA_EGL:
            caps.backend = GPUBackend::MESA_EGL;
            caps.egl_available = true;
            caps.opengl_available = true;
            caps.vendor = "Mesa";
            break;
        default:
            caps.backend = GPUBackend::CPU_ONLY;
            caps.vendor = "CPU";
            break;
    }
}

RuntimeEnvironment GPUDetector::DetectEnvironment() {
    if (IsInFlatpak()) return RuntimeEnvironment::FLATPAK;
    if (IsInDocker()) return RuntimeEnvironment::DOCKER;
    return RuntimeEnvironment::NATIVE;
}

bool GPUDetector::SetupOptimalEGLPath(const GPUCapabilities& caps) {
    std::vector<std::string> search_paths = GetSearchPathsForEnvironment(caps);
    std::string new_path = BuildLDLibraryPath(search_paths);
    return SetLDLibraryPath(new_path);
}

std::vector<std::string> GPUDetector::GetSearchPathsForEnvironment(const GPUCapabilities& caps) {
    if (caps.environment == RuntimeEnvironment::FLATPAK) {
        return GetFlatpakSearchPaths(caps.backend);
    } else {
        return GetNativeSearchPaths();
    }
}

std::vector<std::string> GPUDetector::GetFlatpakSearchPaths(GPUBackend backend) {
    if (backend == GPUBackend::NVIDIA_EGL) {
        return {
            "/usr/lib/x86_64-linux-gnu/GL/nvidia-580-82-07/lib",
            "/usr/lib/x86_64-linux-gnu/GL/nvidia-*/lib",
            "/usr/lib/x86_64-linux-gnu"
        };
    } else {
        return {
            "/usr/lib/x86_64-linux-gnu",
            "/app/lib"
        };
    }
}

std::vector<std::string> GPUDetector::GetNativeSearchPaths() {
    return {
        "/usr/lib/x86_64-linux-gnu",
        "/usr/local/lib",
        "/lib/x86_64-linux-gnu"
    };
}

std::string GPUDetector::BuildLDLibraryPath(const std::vector<std::string>& search_paths) {
    std::string current_path = std::getenv("LD_LIBRARY_PATH") ?: "";
    std::string new_path;
    
    for (const auto& path : search_paths) {
        if (std::filesystem::exists(path)) {
            if (!new_path.empty()) new_path += ":";
            new_path += path;
        }
    }
    
    if (!current_path.empty()) {
        new_path += ":" + current_path;
    }
    
    return new_path;
}

bool GPUDetector::SetLDLibraryPath(const std::string& new_path) {
    if (setenv("LD_LIBRARY_PATH", new_path.c_str(), 1) == 0) {
        std::cout << "🔧 Set optimal EGL path: " << new_path << std::endl;
        return true;
    }
    return false;
}

bool GPUDetector::TestNvidiaEGL() {
    // Check for NVIDIA driver files
    std::vector<std::string> nvidia_paths = {
        "/usr/lib/x86_64-linux-gnu/GL/nvidia-580-82-07/lib/libEGL_nvidia.so.0",
        "/usr/lib/x86_64-linux-gnu/GL/nvidia-*/lib/libEGL_nvidia.so*",
        "/proc/driver/nvidia/version"
    };
    
    for (const auto& path : nvidia_paths) {
        if (std::filesystem::exists(path)) {
            std::cout << "🎮 NVIDIA GPU detected: " << path << std::endl;
            return true;
        }
    }
    
    return false;
}

bool GPUDetector::TestMesaEGL() {
    // Check for Mesa EGL libraries
    std::vector<std::string> mesa_paths = {
        "/usr/lib/x86_64-linux-gnu/libEGL_mesa.so.0",
        "/app/lib/libEGL_mesa.so.0",
        "/usr/lib/libEGL_mesa.so.0"
    };
    
    for (const auto& path : mesa_paths) {
        if (std::filesystem::exists(path)) {
            std::cout << "🖥️  Mesa EGL detected: " << path << std::endl;
            return true;
        }
    }
    
    return false;
}

bool GPUDetector::TestAMDRadeon() {
    // Theoretical AMD Radeon detection - would check for:
    // - AMD driver files: /usr/lib/x86_64-linux-gnu/dri/radeonsi_dri.so
    // - AMD GPU entries: /sys/class/drm/card*/device/vendor (0x1002 = AMD)
    // - amdgpu kernel module: lsmod | grep amdgpu
    
    std::vector<std::string> amd_indicators = {
        "/usr/lib/x86_64-linux-gnu/dri/radeonsi_dri.so",
        "/usr/lib/dri/radeonsi_dri.so",
        "/sys/module/amdgpu"  // AMD GPU kernel module
    };
    
    for (const auto& path : amd_indicators) {
        if (std::filesystem::exists(path)) {
            std::cout << "🟡 AMD Radeon detected: " << path << " (theoretical - not tested)" << std::endl;
            return true;
        }
    }
    
    return false;
}

bool GPUDetector::TestIntelGPU() {
    // Theoretical Intel GPU detection - would check for:
    // - Intel driver files: /usr/lib/x86_64-linux-gnu/dri/iris_dri.so or i965_dri.so
    // - Intel GPU entries: /sys/class/drm/card*/device/vendor (0x8086 = Intel)
    // - i915 kernel module: lsmod | grep i915
    
    std::vector<std::string> intel_indicators = {
        "/usr/lib/x86_64-linux-gnu/dri/iris_dri.so",   // Modern Intel GPUs
        "/usr/lib/x86_64-linux-gnu/dri/i965_dri.so",   // Legacy Intel GPUs
        "/usr/lib/dri/iris_dri.so",
        "/usr/lib/dri/i965_dri.so",
        "/sys/module/i915"  // Intel GPU kernel module
    };
    
    for (const auto& path : intel_indicators) {
        if (std::filesystem::exists(path)) {
            std::cout << "🔷 Intel GPU detected: " << path << " (theoretical - not tested)" << std::endl;
            return true;
        }
    }
    
    return false;
}

std::vector<std::string> GPUDetector::FindEGLLibraries() {
    std::vector<std::string> paths;
    std::vector<std::string> search_dirs = {
        "/usr/lib/x86_64-linux-gnu/GL/nvidia-580-82-07/lib",
        "/usr/lib/x86_64-linux-gnu",
        "/app/lib",
        "/usr/local/lib"
    };
    
    for (const auto& dir : search_dirs) {
        if (std::filesystem::exists(dir + "/libEGL.so.1") ||
            std::filesystem::exists(dir + "/libEGL_nvidia.so.0") ||
            std::filesystem::exists(dir + "/libEGL_mesa.so.0")) {
            paths.push_back(dir);
        }
    }
    
    return paths;
}

bool GPUDetector::IsInFlatpak() {
    return std::filesystem::exists("/.flatpak-info") ||
           std::getenv("FLATPAK_ID") != nullptr;
}

bool GPUDetector::IsInDocker() {
    return std::filesystem::exists("/.dockerenv") ||
           std::getenv("DOCKER_CONTAINER") != nullptr;
}

GPUBackend GPUDetector::GetBestAvailableBackend() {
    auto caps = DetectGPUCapabilities();
    SetupOptimalEGLPath(caps);
    return caps.backend;
}