#include "include/application/portal_utils.h"

#include <vector>
#include <iostream>
#include <dlfcn.h>
#include <gio/gio.h>

#ifndef XDP_PUBLIC
#define XDP_PUBLIC
#endif
#include <libportal/filechooser.h>

namespace segmecam {
namespace {

class PortalFileChooser {
public:
    bool EnsureAvailable();
    bool OpenFileDialog(const std::string& initial_path_hint, std::string& out_uri);

private:
    struct FileDialogContext {
        PortalFileChooser* owner = nullptr;
        GMainLoop* loop = nullptr;
        std::string uri;
        bool success = false;
    };

    static void OnOpenFileFinished(GObject* source_object, GAsyncResult* result, gpointer user_data);

    bool LoadSymbols();

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

    xdp_portal_new_fn_ = reinterpret_cast<XdpPortal* (*)()>(dlsym(portal_library_handle_, "xdp_portal_new"));
    xdp_portal_open_file_fn_ = reinterpret_cast<void (*)(XdpPortal*, XdpParent*, const char*, GVariant*, GVariant*, GVariant*, unsigned int, GCancellable*, GAsyncReadyCallback, gpointer)>(
        dlsym(portal_library_handle_, "xdp_portal_open_file"));
    xdp_portal_open_file_finish_fn_ = reinterpret_cast<GVariant* (*)(XdpPortal*, GAsyncResult*, GError**)>(
        dlsym(portal_library_handle_, "xdp_portal_open_file_finish"));

    if (!xdp_portal_new_fn_ || !xdp_portal_open_file_fn_ || !xdp_portal_open_file_finish_fn_) {
        std::cerr << "❌ Missing libportal symbols" << std::endl;
        return false;
    }

    portal_instance_ = xdp_portal_new_fn_();
    if (!portal_instance_) {
        std::cerr << "❌ Failed to create XdpPortal instance" << std::endl;
        return false;
    }

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

    XdpPortal* portal = reinterpret_cast<XdpPortal*>(source_object);
    GError* error = nullptr;
    GVariant* response = ctx->owner->xdp_portal_open_file_finish_fn_(portal, result, &error);
    if (!response) {
        if (error) {
            std::cerr << "❌ Portal open file failed: " << error->message << std::endl;
            g_error_free(error);
        }
        if (ctx->loop) {
            g_main_loop_quit(ctx->loop);
        }
        return;
    }

    if (response) {
        gchar* dump = g_variant_print(response, TRUE);
        if (dump) {
            std::cout << "📬 FileChooser raw response: " << dump << std::endl;
            g_free(dump);
        }
    }

    g_autoptr(GVariant) uris_variant = g_variant_lookup_value(response, "uris", G_VARIANT_TYPE("as"));
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

    g_variant_unref(response);

    if (error) {
        g_error_free(error);
    }

    if (ctx->loop) {
        g_main_loop_quit(ctx->loop);
    }
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

bool ReadImageFromUri(const std::string& uri, cv::Mat& image_out, std::string& resolved_path) {
    std::cout << "🧷 Attempting to read image from URI: " << uri << std::endl;

    g_autoptr(GFile) file = nullptr;
    if (g_str_has_prefix(uri.c_str(), "file://")) {
        file = g_file_new_for_uri(uri.c_str());
    } else {
        file = g_file_new_for_commandline_arg(uri.c_str());
    }
    if (!file) {
        std::cerr << "❌ Unable to parse URI: " << uri << std::endl;
        return false;
    }

    std::vector<unsigned char> buffer;
    buffer.reserve(64 * 1024);
    const gsize kChunkSize = 16 * 1024;
    std::vector<unsigned char> chunk(kChunkSize);

    constexpr int kMaxAttempts = 10;
    bool read_success = false;
    for (int attempt = 0; attempt < kMaxAttempts && !read_success; ++attempt) {
        g_autoptr(GError) error = nullptr;
        g_autoptr(GFileInputStream) stream = g_file_read(file, nullptr, &error);
        if (!stream) {
            if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
                // Document portal may still be exporting the file; retry shortly.
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

        while (true) {
            g_autoptr(GError) read_error = nullptr;
            gssize bytes_read = g_input_stream_read(G_INPUT_STREAM(stream), chunk.data(), chunk.size(), nullptr, &read_error);
            if (bytes_read > 0) {
                buffer.insert(buffer.end(), chunk.begin(), chunk.begin() + bytes_read);
            } else if (bytes_read == 0) {
                read_success = true;
                break;
            } else {
                std::cerr << "❌ Error reading image data: " << (read_error ? read_error->message : "unknown") << std::endl;
                return false;
            }
        }
    }

    if (!read_success) {
        std::cerr << "❌ Unable to read image data after retries" << std::endl;
        return false;
    }

    if (buffer.empty()) {
        std::cerr << "❌ Portal returned empty file data" << std::endl;
        return false;
    }

    cv::Mat data_mat(1, static_cast<int>(buffer.size()), CV_8UC1, buffer.data());
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

} // namespace

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
