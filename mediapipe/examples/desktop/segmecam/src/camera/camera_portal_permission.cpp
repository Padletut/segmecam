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
        std::cout << "✅ Camera permission already granted (fd=" << portal_fd_ << ")" << std::endl;
        return true;
    }

    if (!LoadPortalLibrary()) {
        std::cerr << "❌ Failed to load portal library" << std::endl;
        return false;
    }

    LoadPortalFunctions();
    if (!ValidatePortalFunctions()) {
        std::cerr << "❌ Portal functions validation failed" << std::endl;
        return false;
    }

    if (!CreatePortalInstance()) {
        std::cerr << "❌ Failed to create portal instance" << std::endl;
        return false;
    }

    PortalRequestContext ctx;
    if (!SetupAsyncRequest(ctx)) {
        std::cerr << "❌ Failed to setup async request" << std::endl;
        return false;
    }

    std::cout << "⏳ Running main loop for portal request..." << std::endl;
    RunMainLoop(ctx);
    std::cout << "✅ Portal request completed" << std::endl;
    return HandlePermissionResult(ctx);
}

// RequestCameraPermission helper methods
bool CameraManager::LoadPortalLibrary() {
    if (portal_library_handle_) {
        std::cout << "✅ Portal library already loaded" << std::endl;
        return true;  // Already loaded
    }

    std::cout << "🔍 Loading libportal library..." << std::endl;
    portal_library_handle_ = dlopen("libportal-1.so.0", RTLD_LAZY);
    if (!portal_library_handle_) {
        std::cout << "⚠️  libportal-1.so.0 not found, trying libportal.so.1..." << std::endl;
        portal_library_handle_ = dlopen("libportal.so.1", RTLD_LAZY);
    }
    if (!portal_library_handle_) {
        std::cout << "⚠️  libportal.so.1 not found, trying libportal.so.0..." << std::endl;
        portal_library_handle_ = dlopen("libportal.so.0", RTLD_LAZY);
    }
    if (!portal_library_handle_) {
        std::cout << "⚠️  libportal.so.0 not found, trying libportal.so..." << std::endl;
        portal_library_handle_ = dlopen("libportal.so", RTLD_LAZY);
    }
    if (!portal_library_handle_) {
        std::cerr << "❌ Failed to load libportal: " << dlerror() << std::endl;
        state_.status_message = "Flatpak camera portal unavailable";
        return false;
    }
    std::cout << "✅ Portal library loaded successfully" << std::endl;
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
        std::cout << "🏗️  Creating XdpPortal instance..." << std::endl;
        portal_instance_ = xdp_portal_new();
        if (!portal_instance_) {
            std::cerr << "❌ Unable to create XdpPortal instance" << std::endl;
            return false;
        }
        std::cout << "✅ XdpPortal instance created" << std::endl;
    }

    if (xdp_portal_is_camera_present && !xdp_portal_is_camera_present(portal_instance_)) {
        std::cout << "⚠️  Camera portal reports no camera present" << std::endl;
    } else {
        std::cout << "📷 Camera portal reports camera is present" << std::endl;
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
        std::cerr << "❌ Camera permission denied by portal (ctx.success=false)" << std::endl;
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
        std::cerr << "❌ xdp_portal_access_camera_finish function unavailable" << std::endl;
        return false;
    }

    std::cout << "🔍 Processing portal access result..." << std::endl;
    GError* error = nullptr;
    gboolean allow = xdp_portal_access_camera_finish(portal_instance_, result, &error);
    
    if (allow) {
        std::cout << "✅ Portal access granted" << std::endl;
        granted = true;
    } else {
        std::cout << "❌ Portal access denied" << std::endl;
        std::cerr << "❌ Camera access denied by portal" << std::endl;
        state_.status_message = "Camera permission denied";
    }

    HandlePortalError(error);
    return true;
}

bool CameraManager::OpenPipeWireRemote(bool& granted) {
    if (!xdp_portal_open_pipewire_remote_for_camera) {
        std::cerr << "❌ xdp_portal_open_pipewire_remote_for_camera function unavailable" << std::endl;
        return false;
    }

    if (portal_fd_ >= 0) {
        close(portal_fd_);
        portal_fd_ = -1;
    }

    std::cout << "🔌 Opening PipeWire remote via portal..." << std::endl;
    int fd = xdp_portal_open_pipewire_remote_for_camera(portal_instance_);
    if (fd >= 0) {
        portal_fd_ = fd;
        granted = true;
        std::cout << "✅ PipeWire remote opened successfully (fd=" << fd << ")" << std::endl;
        return true;
    } else {
        std::cerr << "❌ Unable to open PipeWire remote via portal (fd=" << fd << ")" << std::endl;
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