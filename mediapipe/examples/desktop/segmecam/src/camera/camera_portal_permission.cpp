// cppcheck-suppress missingInclude
#include "include/camera/camera_manager.h"
#include <iostream>
#include <dlfcn.h>

// Constants from original file
constexpr unsigned int kXdpCameraFlagNone = 0;

namespace {

// Portal request context for async operations
struct PortalRequestContext {
    void* self = nullptr;
    GMainLoop* loop = nullptr;
    bool success = false;
};

} // anonymous namespace

namespace segmecam {

bool CameraManager::RequestCameraPermission() {
    std::cout << "📷 Requesting camera permission via Portal..." << std::endl;

    if (camera_permission_granted_ && portal_fd_ >= 0) {
        return true;
    }

    if (!LoadPortalLibrary()) {
        return false;
    }

    LoadPortalFunctions();
    if (!ValidatePortalFunctions()) {
        return false;
    }

    if (!CreatePortalInstance()) {
        return false;
    }

    PortalRequestContext ctx;
    if (!SetupAsyncRequest(ctx)) {
        return false;
    }

    RunMainLoop(ctx);
    return HandlePermissionResult(ctx);
}

// RequestCameraPermission helper methods
bool CameraManager::LoadPortalLibrary() {
    if (portal_library_handle_) {
        return true;  // Already loaded
    }

    portal_library_handle_ = dlopen("libportal-1.so.0", RTLD_LAZY);
    if (!portal_library_handle_) {
        portal_library_handle_ = dlopen("libportal.so.1", RTLD_LAZY);
    }
    if (!portal_library_handle_) {
        portal_library_handle_ = dlopen("libportal.so.0", RTLD_LAZY);
    }
    if (!portal_library_handle_) {
        portal_library_handle_ = dlopen("libportal.so", RTLD_LAZY);
    }
    if (!portal_library_handle_) {
        std::cerr << "❌ Failed to load libportal: " << dlerror() << std::endl;
        state_.status_message = "Flatpak camera portal unavailable";
        return false;
    }
    return true;
}

bool CameraManager::LoadPortalFunctions() {
    xdp_portal_new = reinterpret_cast<XdpPortal* (*)()>(dlsym(portal_library_handle_, "xdp_portal_new"));
    if (!xdp_portal_new) std::cerr << "❌ xdp_portal_new dlerror: " << dlerror() << std::endl;
    
    xdp_portal_is_camera_present = reinterpret_cast<gboolean (*)(XdpPortal*)>(dlsym(portal_library_handle_, "xdp_portal_is_camera_present"));
    if (!xdp_portal_is_camera_present) std::cerr << "❌ xdp_portal_is_camera_present dlerror: " << dlerror() << std::endl;
    
    xdp_portal_access_camera = reinterpret_cast<void (*)(XdpPortal*, XdpParent*, XdpCameraFlags, GCancellable*, GAsyncReadyCallback, gpointer)>(dlsym(portal_library_handle_, "xdp_portal_access_camera"));
    if (!xdp_portal_access_camera) std::cerr << "❌ xdp_portal_access_camera dlerror: " << dlerror() << std::endl;
    
    xdp_portal_access_camera_finish = reinterpret_cast<gboolean (*)(XdpPortal*, GAsyncResult*, GError**)>(dlsym(portal_library_handle_, "xdp_portal_access_camera_finish"));
    if (!xdp_portal_access_camera_finish) std::cerr << "❌ xdp_portal_access_camera_finish dlerror: " << dlerror() << std::endl;
    
    xdp_portal_open_pipewire_remote_for_camera = reinterpret_cast<int (*)(XdpPortal*)>(dlsym(portal_library_handle_, "xdp_portal_open_pipewire_remote_for_camera"));
    if (!xdp_portal_open_pipewire_remote_for_camera) std::cerr << "❌ xdp_portal_open_pipewire_remote_for_camera dlerror: " << dlerror() << std::endl;

    return true;
}

bool CameraManager::ValidatePortalFunctions() {
    if (!xdp_portal_new || !xdp_portal_access_camera || !xdp_portal_access_camera_finish || !xdp_portal_open_pipewire_remote_for_camera) {
        std::cerr << "❌ Portal functions unavailable" << std::endl;
        state_.status_message = "Flatpak camera portal unavailable";
        return false;
    }
    return true;
}

bool CameraManager::CreatePortalInstance() {
    if (!portal_instance_) {
        portal_instance_ = xdp_portal_new();
        if (!portal_instance_) {
            std::cerr << "❌ Unable to create XdpPortal instance" << std::endl;
            return false;
        }
    }

    if (xdp_portal_is_camera_present && !xdp_portal_is_camera_present(portal_instance_)) {
        std::cerr << "⚠️  Camera portal reports no camera present" << std::endl;
    }

    return true;
}

bool CameraManager::SetupAsyncRequest(PortalRequestContext& ctx) {
    if (portal_fd_ >= 0) {
        close(portal_fd_);
        portal_fd_ = -1;
    }

    ctx.self = this;
    if (g_main_loop_new) {
        ctx.loop = g_main_loop_new(nullptr, FALSE);
    } else {
        std::cerr << "❌ g_main_loop_new unavailable" << std::endl;
        return false;
    }

    xdp_portal_access_camera(portal_instance_, nullptr, (XdpCameraFlags)kXdpCameraFlagNone, nullptr,
                             &CameraManager::OnPortalCameraAccessFinished, &ctx);
    return true;
}

bool CameraManager::RunMainLoop(PortalRequestContext& ctx) {
    if (ctx.loop) {
        if (g_main_loop_run) {
            g_main_loop_run(ctx.loop);
        }
        if (g_main_loop_unref) {
            g_main_loop_unref(ctx.loop);
        }
        ctx.loop = nullptr;
    }
    return true;
}

bool CameraManager::HandlePermissionResult(const PortalRequestContext& ctx) {
    camera_permission_granted_ = ctx.success;
    if (!camera_permission_granted_) {
        if (portal_fd_ >= 0) {
            close(portal_fd_);
            portal_fd_ = -1;
        }
        std::cerr << "❌ Camera permission denied by portal" << std::endl;
        state_.status_message = "Camera permission denied";
        return false;
    }

    std::cout << "✅ Camera portal granted PipeWire remote (fd=" << portal_fd_ << ")" << std::endl;
    state_.status_message.clear();
    return true;
}

// Missing methods called from camera_gstreamer_callbacks.cpp
bool CameraManager::ProcessPortalAccessResult(GAsyncResult* result, bool& granted) {
    if (!xdp_portal_access_camera_finish) {
        return false;
    }

    GError* error = nullptr;
    gboolean allow = xdp_portal_access_camera_finish(portal_instance_, result, &error);
    
    if (allow) {
        granted = true;
    } else {
        std::cerr << "❌ Camera access denied by portal" << std::endl;
        state_.status_message = "Camera permission denied";
    }

    HandlePortalError(error);
    return true;
}

bool CameraManager::OpenPipeWireRemote(bool& granted) {
    if (!xdp_portal_open_pipewire_remote_for_camera) {
        return false;
    }

    if (portal_fd_ >= 0) {
        close(portal_fd_);
        portal_fd_ = -1;
    }

    int fd = xdp_portal_open_pipewire_remote_for_camera(portal_instance_);
    if (fd >= 0) {
        portal_fd_ = fd;
        granted = true;
        return true;
    } else {
        std::cerr << "❌ Unable to open PipeWire remote via portal" << std::endl;
        return false;
    }
}

void CameraManager::CleanupPortalRequestContext(PortalRequestContext* ctx) {
    if (ctx && ctx->loop && g_main_loop_quit) {
        g_main_loop_quit(ctx->loop);
    }
}

void CameraManager::HandlePortalError(GError* error) {
    if (error && g_error_free) {
        g_error_free(error);
    }
}

} // namespace segmecam