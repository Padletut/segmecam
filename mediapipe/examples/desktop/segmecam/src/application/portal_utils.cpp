#include "include/application/portal_utils.h"

#include <vector>  
#include <iostream>
#include <dlfcn.h>
#include <gio/gio.h>

#ifndef XDP_PUBLIC
#define XDP_PUBLIC
#endif
#include <libportal/filechooser.h> // NOLINT - Standard library header resolved by build system

namespace segmecam {
namespace {

class PortalFileChooser {
public:
    bool EnsureAvailable();
    bool OpenFileDialog(const std::string& initial_path_hint, std::string& out_uri);

    // Helper methods for ReadImageFromUri
    GFile* CreateFileFromUri(const std::string& uri);
    bool ReadFileWithRetry(GFile* file, std::vector<unsigned char>& buffer);
    bool DecodeImageData(const std::vector<unsigned char>& buffer, cv::Mat& image_out, std::string& resolved_path, GFile* file, const std::string& uri);

private:
    struct FileDialogContext {
        PortalFileChooser* owner = nullptr;
        GMainLoop* loop = nullptr;
        std::string uri;
        bool success = false;
        GVariant* response = nullptr;
    };

    static void OnOpenFileFinished(GObject* source_object, GAsyncResult* result, gpointer user_data);

    bool LoadSymbols();
    bool LoadLibrary();
    bool LoadFunctionSymbols();
    bool CreatePortalInstance();
    bool ValidateContext(FileDialogContext* ctx);
    bool ProcessPortalResponse(GObject* source_object, GAsyncResult* result, FileDialogContext* ctx);
    void ExtractUriFromResponse(FileDialogContext* ctx);
    void CleanupAndQuit(FileDialogContext* ctx);

    void* portal_library_handle_ = nullptr;
    XdpPortal* portal_instance_ = nullptr;
    bool load_attempted_ = false;

    XdpPortal* (*xdp_portal_new_fn_)(void) = nullptr;
    void (*xdp_portal_open_file_fn_)(XdpPortal*, XdpParent*, const char*, GVariant*, GVariant*, GVariant*, unsigned int, GCancellable*, GAsyncReadyCallback, gpointer) = nullptr;
    GVariant* (*xdp_portal_open_file_finish_fn_)(XdpPortal*, GAsyncResult*, GError**) = nullptr;
};

PortalFileChooser g_portal_file_chooser;

bool PortalFileChooser::LoadSymbols() {
    if (portal_library_handle_) {
        return xdp_portal_new_fn_ && xdp_portal_open_file_fn_ && xdp_portal_open_file_finish_fn_;
    }

    if (load_attempted_) {
        return false;
    }

    if (!LoadLibrary()) {
        return false;
    }

    if (!LoadFunctionSymbols()) {
        return false;
    }

    if (!CreatePortalInstance()) {
        return false;
    }

    return true;
}

bool PortalFileChooser::LoadLibrary() {
    load_attempted_ = true;

    static const char* kLibNames[] = {"libportal-1.so.0", "libportal.so.1", "libportal.so.0", "libportal.so"};
    for (const char* name : kLibNames) {
        portal_library_handle_ = dlopen(name, RTLD_LAZY);
        if (portal_library_handle_) {
            break;
        }
    }
    if (!portal_library_handle_) {
        std::cerr << "❌ Unable to load libportal" << std::endl;
        return false;
    }
    return true;
}

bool PortalFileChooser::LoadFunctionSymbols() {
    xdp_portal_new_fn_ = reinterpret_cast<XdpPortal* (*)()>(dlsym(portal_library_handle_, "xdp_portal_new"));
    xdp_portal_open_file_fn_ = reinterpret_cast<void (*)(XdpPortal*, XdpParent*, const char*, GVariant*, GVariant*, GVariant*, unsigned int, GCancellable*, GAsyncReadyCallback, gpointer)>(
        dlsym(portal_library_handle_, "xdp_portal_open_file"));
    xdp_portal_open_file_finish_fn_ = reinterpret_cast<GVariant* (*)(XdpPortal*, GAsyncResult*, GError**)>(
        dlsym(portal_library_handle_, "xdp_portal_open_file_finish"));

    if (!xdp_portal_new_fn_ || !xdp_portal_open_file_fn_ || !xdp_portal_open_file_finish_fn_) {
        std::cerr << "❌ Missing libportal symbols" << std::endl;
        return false;
    }
    return true;
}

bool PortalFileChooser::CreatePortalInstance() {
    portal_instance_ = xdp_portal_new_fn_();
    if (!portal_instance_) {
        std::cerr << "❌ Failed to create XdpPortal instance" << std::endl;
        return false;
    }
    return true;
}

bool PortalFileChooser::ValidateContext(FileDialogContext* ctx) {
    return ctx && ctx->owner;
}

bool PortalFileChooser::ProcessPortalResponse(GObject* source_object, GAsyncResult* result, FileDialogContext* ctx) {
    XdpPortal* portal = reinterpret_cast<XdpPortal*>(source_object);
    GError* error = nullptr;
    GVariant* response = ctx->owner->xdp_portal_open_file_finish_fn_(portal, result, &error);
    if (!response) {
        if (error) {
            std::cerr << "❌ Portal open file failed: " << error->message << std::endl;
            g_error_free(error);
        }
        return false;
    }

    // response is guaranteed to be non-null after the early return above
    gchar* dump = g_variant_print(response, TRUE);
    if (dump) {
        std::cout << "📬 FileChooser raw response: " << dump << std::endl;
        g_free(dump);
    }

    ctx->response = response;
    return true;
}

void PortalFileChooser::ExtractUriFromResponse(FileDialogContext* ctx) {
    g_autoptr(GVariant) uris_variant = g_variant_lookup_value(ctx->response, "uris", G_VARIANT_TYPE("as"));
    if (uris_variant) {
        gsize n_uris = 0;
        g_auto(GStrv) uris = g_variant_dup_strv(uris_variant, &n_uris);
        if (uris && n_uris > 0 && uris[0] && *uris[0]) {
            ctx->uri = uris[0];
            ctx->success = true;
            std::cout << "✅ FileChooser selected URI: " << ctx->uri << std::endl;
        } else {
            std::cout << "⚠️  FileChooser returned empty URI list" << std::endl;
        }
    } else {
        std::cout << "⚠️  FileChooser response missing 'uris' key" << std::endl;
    }

    g_variant_unref(ctx->response);
}

void PortalFileChooser::CleanupAndQuit(FileDialogContext* ctx) {
    if (ctx->loop) {
        g_main_loop_quit(ctx->loop);
    }
}

GFile* PortalFileChooser::CreateFileFromUri(const std::string& uri) {
    if (g_str_has_prefix(uri.c_str(), "file://")) {
        return g_file_new_for_uri(uri.c_str());
    } else {
        return g_file_new_for_commandline_arg(uri.c_str());
    }
}

bool PortalFileChooser::ReadFileWithRetry(GFile* file, std::vector<unsigned char>& buffer) {
    const gsize kChunkSize = 16 * 1024;
    std::vector<unsigned char> chunk(kChunkSize);
    constexpr int kMaxAttempts = 10;
    
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        g_autoptr(GError) error = nullptr;
        g_autoptr(GFileInputStream) stream = g_file_read(file, nullptr, &error);
        if (!stream) {
            if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
                if (attempt < kMaxAttempts - 1) {
                    g_usleep(100 * 1000); // 100 ms
                    continue;
                }
            }
            if (error) {
                std::cerr << "❌ Failed to read file: " << error->message << std::endl;
            }
            return false;
        }

        bool read_success = false;
        while (!read_success) {
            g_autoptr(GError) read_error = nullptr;
            gssize bytes_read = g_input_stream_read(G_INPUT_STREAM(stream), chunk.data(), chunk.size(), nullptr, &read_error);
            if (bytes_read > 0) {
                buffer.insert(buffer.end(), chunk.begin(), chunk.begin() + bytes_read);
            } else if (bytes_read == 0) {
                read_success = true;
            } else {
                std::cerr << "❌ Error reading image data: " << (read_error ? read_error->message : "unknown") << std::endl;
                return false;
            }
        }
        return true;
    }
    std::cerr << "❌ Unable to read image data after retries" << std::endl;
    return false;
}

bool PortalFileChooser::DecodeImageData(const std::vector<unsigned char>& buffer, cv::Mat& image_out, std::string& resolved_path, GFile* file, const std::string& uri) {
    if (buffer.empty()) {
        std::cerr << "❌ Portal returned empty file data" << std::endl;
        return false;
    }

    cv::Mat data_mat(1, static_cast<int>(buffer.size()), CV_8UC1, const_cast<unsigned char*>(buffer.data()));
    cv::Mat decoded = cv::imdecode(data_mat, cv::IMREAD_COLOR);
    if (decoded.empty()) {
        std::cerr << "❌ Failed to decode image data" << std::endl;
        return false;
    }

    image_out = decoded;

    g_autofree gchar* path_c = g_file_get_path(file);
    if (path_c) {
        resolved_path.assign(path_c);
    } else {
        resolved_path = uri;
    }

    std::cout << "📄 Loaded image from URI: " << resolved_path
              << " (" << image_out.cols << "x" << image_out.rows << ")" << std::endl;
    return true;
}

bool PortalFileChooser::EnsureAvailable() {
    return LoadSymbols();
}

void PortalFileChooser::OnOpenFileFinished(GObject* source_object, GAsyncResult* result, gpointer user_data) {
    auto* ctx = static_cast<FileDialogContext*>(user_data);
    if (!ctx || !ctx->owner) {
        return;
    }

    if (!ctx->owner->ValidateContext(ctx)) {
        ctx->owner->CleanupAndQuit(ctx);
        return;
    }

    if (!ctx->owner->ProcessPortalResponse(source_object, result, ctx)) {
        ctx->owner->CleanupAndQuit(ctx);
        return;
    }

    ctx->owner->ExtractUriFromResponse(ctx);
    ctx->owner->CleanupAndQuit(ctx);
}

bool PortalFileChooser::OpenFileDialog(const std::string& initial_path_hint, std::string& out_uri) {
    (void)initial_path_hint;
    if (!EnsureAvailable()) {
        std::cout << "❌ FileChooser portal unavailable" << std::endl;
        return false;
    }

    FileDialogContext ctx;
    ctx.owner = this;
    ctx.loop = g_main_loop_new(nullptr, FALSE);

    XdpParent* parent = nullptr;
    const char* title = "Select Background Image";
    xdp_portal_open_file_fn_(portal_instance_, parent, title, nullptr, nullptr, nullptr, XDP_OPEN_FILE_FLAG_NONE,
                             nullptr, &PortalFileChooser::OnOpenFileFinished, &ctx);

    if (ctx.loop) {
        g_main_loop_run(ctx.loop);
        g_main_loop_unref(ctx.loop);
        ctx.loop = nullptr;
    }

    if (!ctx.success || ctx.uri.empty()) {
        std::cout << "⚠️  FileChooser returned empty result" << std::endl;
        return false;
    }

    out_uri = ctx.uri;
    return true;
}

} // namespace

bool ReadImageFromUri(const std::string& uri, cv::Mat& image_out, std::string& resolved_path) {
    std::cout << "🧷 Attempting to read image from URI: " << uri << std::endl;

    g_autoptr(GFile) file = g_portal_file_chooser.CreateFileFromUri(uri);
    if (!file) {
        std::cerr << "❌ Unable to parse URI: " << uri << std::endl;
        return false;
    }

    std::vector<unsigned char> buffer;
    buffer.reserve(64 * 1024);
    
    if (!g_portal_file_chooser.ReadFileWithRetry(file, buffer)) {
        return false;
    }

    return g_portal_file_chooser.DecodeImageData(buffer, image_out, resolved_path, file, uri);
}

bool LoadBackgroundImageWithPortal(const std::string& original_path,
                                   cv::Mat& image_out,
                                   std::string& resolved_path) {
    // First try reading directly with OpenCV
    cv::Mat direct = cv::imread(original_path);
    if (!direct.empty()) {
        image_out = direct;
        resolved_path = original_path;
        std::cout << "✅ Loaded background directly: " << original_path << std::endl;
        return true;
    }

    // Next, try to resolve the host path via the Documents portal without showing a dialog
    if (ReadImageFromUri(original_path, image_out, resolved_path)) {
        std::cout << "✅ Loaded background via document portal mapping: " << resolved_path << std::endl;
        return true;
    }

    // Finally, fall back to a portal file chooser dialog
    std::cout << "⚠️  Direct read failed. Opening portal file chooser..." << std::endl;
    return OpenBackgroundImagePortalDialog(image_out, resolved_path);
}

bool OpenBackgroundImagePortalDialog(cv::Mat& image_out, std::string& resolved_path) {
    std::string portal_uri;
    if (!g_portal_file_chooser.OpenFileDialog("", portal_uri)) {
        std::cout << "❌ FileChooser dialog cancelled or failed" << std::endl;
        return false;
    }

    return ReadImageFromUri(portal_uri, image_out, resolved_path);
}

} // namespace segmecam
