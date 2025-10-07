#include "include/camera/camera_manager.h"
#include <iostream>
#include <vector>
#include <dlfcn.h>
#include <cstdlib>

namespace segmecam {

template<typename T>
bool CameraManager::LoadFunctionPointer(T*& func_ptr, void* library, const char* symbol_name, const char* func_name) {
    func_ptr = reinterpret_cast<T*>(dlsym(library, symbol_name));
    if (!func_ptr) {
        std::cerr << "❌ " << func_name << " dlerror: " << dlerror() << std::endl;
        return false;
    }
    return true;
}

bool CameraManager::ValidateFunctionPointers(const std::vector<std::pair<void*, const char*>>& functions_to_check) {
    std::vector<std::string> failed_functions;
    
    for (const auto& func_pair : functions_to_check) {
        if (!func_pair.first) {
            failed_functions.push_back(func_pair.second);
        }
    }

    if (!failed_functions.empty()) {
        std::cerr << "❌ Failed to load GStreamer/GLib functions: ";
        for (size_t i = 0; i < failed_functions.size(); ++i) {
            if (i > 0) std::cerr << ", ";
            std::cerr << failed_functions[i];
        }
        std::cerr << std::endl;
        return false;
    }

    return true;
}

bool CameraManager::LoadRequiredLibraries(void*& gst_lib, void*& gstapp_lib, void*& gstvideo_lib, void*& glib_lib, void*& gobject_lib) {
    // Load GStreamer core library
    std::cout << "🔍 DEBUG: Attempting to load libgstreamer-1.0.so.0" << std::endl;
    gst_lib = dlopen("libgstreamer-1.0.so.0", RTLD_LAZY);
    if (!gst_lib) {
        std::cerr << "❌ Failed to load libgstreamer-1.0.so.0: " << dlerror() << std::endl;
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libgstreamer-1.0.so.0" << std::endl;

    // Load GStreamer app library (needed for gst_app_sink_* functions)
    std::cout << "🔍 DEBUG: Attempting to load libgstapp-1.0.so.0" << std::endl;
    gstapp_lib = dlopen("libgstapp-1.0.so.0", RTLD_LAZY);
    if (!gstapp_lib) {
        std::cerr << "❌ Failed to load libgstapp-1.0.so.0: " << dlerror() << std::endl;
        dlclose(gst_lib);
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libgstapp-1.0.so.0" << std::endl;

    // Load GStreamer video library (needed for video format helpers)
    std::cout << "🔍 DEBUG: Attempting to load libgstvideo-1.0.so.0" << std::endl;
    gstvideo_lib = dlopen("libgstvideo-1.0.so.0", RTLD_LAZY);
    if (!gstvideo_lib) {
        std::cerr << "❌ Failed to load libgstvideo-1.0.so.0: " << dlerror() << std::endl;
        dlclose(gst_lib);
        dlclose(gstapp_lib);
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libgstvideo-1.0.so.0" << std::endl;

    // Load GLib library
    std::cout << "🔍 DEBUG: Attempting to load libglib-2.0.so.0" << std::endl;
    glib_lib = dlopen("libglib-2.0.so.0", RTLD_LAZY);
    if (!glib_lib) {
        std::cerr << "❌ Failed to load libglib-2.0.so.0: " << dlerror() << std::endl;
        std::cout << "🔍 DEBUG: dlerror() says: " << dlerror() << std::endl;
        dlclose(gst_lib);
        dlclose(gstapp_lib);
        dlclose(gstvideo_lib);
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libglib-2.0.so.0" << std::endl;

    // Load GObject library (needed for g_object_set, g_signal_connect)
    std::cout << "🔍 DEBUG: Attempting to load libgobject-2.0.so.0" << std::endl;
    gobject_lib = dlopen("libgobject-2.0.so.0", RTLD_LAZY);
    if (!gobject_lib) {
        std::cerr << "❌ Failed to load libgobject-2.0.so.0: " << dlerror() << std::endl;
        dlclose(gst_lib);
        dlclose(gstapp_lib);
        dlclose(gstvideo_lib);
        dlclose(glib_lib);
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libgobject-2.0.so.0" << std::endl;

    return true;
}

bool CameraManager::LoadGStreamerCoreFunctions(void* gst_lib) {
    // Load GStreamer functions from core library using helper
    bool success = true;
    success &= LoadFunctionPointer(gst_init, gst_lib, "gst_init", "gst_init");
    success &= LoadFunctionPointer(gst_pipeline_new, gst_lib, "gst_pipeline_new", "gst_pipeline_new");
    success &= LoadFunctionPointer(gst_element_factory_make, gst_lib, "gst_element_factory_make", "gst_element_factory_make");
    success &= LoadFunctionPointer(gst_element_set_state, gst_lib, "gst_element_set_state", "gst_element_set_state");
    success &= LoadFunctionPointer(gst_element_get_state, gst_lib, "gst_element_get_state", "gst_element_get_state");
    success &= LoadFunctionPointer(gst_bin_add_many, gst_lib, "gst_bin_add_many", "gst_bin_add_many");
    success &= LoadFunctionPointer(gst_element_link_many, gst_lib, "gst_element_link_many", "gst_element_link_many");
    success &= LoadFunctionPointer(gst_object_unref, gst_lib, "gst_object_unref", "gst_object_unref");
    success &= LoadFunctionPointer(gst_sample_get_buffer, gst_lib, "gst_sample_get_buffer", "gst_sample_get_buffer");
    success &= LoadFunctionPointer(gst_sample_get_caps, gst_lib, "gst_sample_get_caps", "gst_sample_get_caps");
    success &= LoadFunctionPointer(gst_caps_from_string, gst_lib, "gst_caps_from_string", "gst_caps_from_string");
    success &= LoadFunctionPointer(gst_caps_unref, gst_lib, "gst_caps_unref", "gst_caps_unref");
    success &= LoadFunctionPointer(gst_caps_get_structure, gst_lib, "gst_caps_get_structure", "gst_caps_get_structure");
    success &= LoadFunctionPointer(gst_structure_get_int, gst_lib, "gst_structure_get_int", "gst_structure_get_int");
    success &= LoadFunctionPointer(gst_structure_get_string, gst_lib, "gst_structure_get_string", "gst_structure_get_string");
    success &= LoadFunctionPointer(gst_element_link, gst_lib, "gst_element_link", "gst_element_link");
    success &= LoadFunctionPointer(gst_element_link_filtered, gst_lib, "gst_element_link_filtered", "gst_element_link_filtered");
    success &= LoadFunctionPointer(gst_parse_launch, gst_lib, "gst_parse_launch", "gst_parse_launch");
    success &= LoadFunctionPointer(gst_bin_get_by_name, gst_lib, "gst_bin_get_by_name", "gst_bin_get_by_name");
    success &= LoadFunctionPointer(gst_buffer_map, gst_lib, "gst_buffer_map", "gst_buffer_map");
    success &= LoadFunctionPointer(gst_buffer_unmap, gst_lib, "gst_buffer_unmap", "gst_buffer_unmap");
    success &= LoadFunctionPointer(gst_sample_unref, gst_lib, "gst_sample_unref", "gst_sample_unref");

    return success;
}

bool CameraManager::LoadGStreamerAppFunctions(void* gstapp_lib) {
    // Load GStreamer app functions from app library
    gst_app_sink_pull_sample = reinterpret_cast<void* (*)(GstAppSink*)>(dlsym(gstapp_lib, "gst_app_sink_pull_sample"));
    if (!gst_app_sink_pull_sample) std::cerr << "❌ gst_app_sink_pull_sample dlerror: " << dlerror() << std::endl;
    gst_app_sink_is_eos = reinterpret_cast<int (*)(GstAppSink*)>(dlsym(gstapp_lib, "gst_app_sink_is_eos"));
    if (!gst_app_sink_is_eos) std::cerr << "❌ gst_app_sink_is_eos dlerror: " << dlerror() << std::endl;

    return true;
}

bool CameraManager::LoadGLibFunctions(void* glib_lib, void* gobject_lib) {
    // Load GLib functions using helper
    bool success = true;
    success &= LoadFunctionPointer(g_object_set, gobject_lib, "g_object_set", "g_object_set");
    success &= LoadFunctionPointer(g_main_loop_quit, glib_lib, "g_main_loop_quit", "g_main_loop_quit");
    success &= LoadFunctionPointer(g_main_loop_unref, glib_lib, "g_main_loop_unref", "g_main_loop_unref");
    success &= LoadFunctionPointer(g_signal_connect, gobject_lib, "g_signal_connect_data", "g_signal_connect");
    success &= LoadFunctionPointer(g_main_loop_new, glib_lib, "g_main_loop_new", "g_main_loop_new");
    success &= LoadFunctionPointer(g_main_loop_run, glib_lib, "g_main_loop_run", "g_main_loop_run");
    success &= LoadFunctionPointer(g_object_unref_ptr, gobject_lib, "g_object_unref", "g_object_unref");
    success &= LoadFunctionPointer(g_usleep, glib_lib, "g_usleep", "g_usleep");
    success &= LoadFunctionPointer(g_error_free, glib_lib, "g_error_free", "g_error_free");

    return success;
}

bool CameraManager::ValidateFunctionLoading() {
    // Check if all functions were loaded using helper
    std::vector<std::pair<void*, const char*>> functions_to_check;
    functions_to_check.reserve(32); // Reserve space for efficiency
    
    functions_to_check.push_back({reinterpret_cast<void*>(gst_init), "gst_init"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_pipeline_new), "gst_pipeline_new"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_element_factory_make), "gst_element_factory_make"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_element_set_state), "gst_element_set_state"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_element_get_state), "gst_element_get_state"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_bin_add_many), "gst_bin_add_many"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_element_link_many), "gst_element_link_many"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_object_unref), "gst_object_unref"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_app_sink_pull_sample), "gst_app_sink_pull_sample"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_app_sink_is_eos), "gst_app_sink_is_eos"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_sample_get_buffer), "gst_sample_get_buffer"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_sample_get_caps), "gst_sample_get_caps"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_caps_from_string), "gst_caps_from_string"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_caps_unref), "gst_caps_unref"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_caps_get_structure), "gst_caps_get_structure"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_structure_get_int), "gst_structure_get_int"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_element_link), "gst_element_link"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_element_link_filtered), "gst_element_link_filtered"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_parse_launch), "gst_parse_launch"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_bin_get_by_name), "gst_bin_get_by_name"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_buffer_map), "gst_buffer_map"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_buffer_unmap), "gst_buffer_unmap"});
    functions_to_check.push_back({reinterpret_cast<void*>(gst_sample_unref), "gst_sample_unref"});
    functions_to_check.push_back({reinterpret_cast<void*>(g_object_set), "g_object_set"});
    functions_to_check.push_back({reinterpret_cast<void*>(g_main_loop_quit), "g_main_loop_quit"});
    functions_to_check.push_back({reinterpret_cast<void*>(g_main_loop_unref), "g_main_loop_unref"});
    functions_to_check.push_back({reinterpret_cast<void*>(g_signal_connect), "g_signal_connect"});
    functions_to_check.push_back({reinterpret_cast<void*>(g_main_loop_new), "g_main_loop_new"});
    functions_to_check.push_back({reinterpret_cast<void*>(g_main_loop_run), "g_main_loop_run"});
    functions_to_check.push_back({reinterpret_cast<void*>(g_object_unref_ptr), "g_object_unref"});
    functions_to_check.push_back({reinterpret_cast<void*>(g_usleep), "g_usleep"});
    functions_to_check.push_back({reinterpret_cast<void*>(g_error_free), "g_error_free"});

    return ValidateFunctionPointers(functions_to_check);
}

bool CameraManager::InitializeGStreamer() {
    if (gst_initialized_) {
        return true;
    }

    std::cout << "🎬 Initializing GStreamer for PipeWire support..." << std::endl;

    // Load required libraries
    void* gst_lib = nullptr;
    void* gstapp_lib = nullptr;
    void* gstvideo_lib = nullptr;
    void* glib_lib = nullptr;
    void* gobject_lib = nullptr;

    if (!LoadRequiredLibraries(gst_lib, gstapp_lib, gstvideo_lib, glib_lib, gobject_lib)) {
        return false;
    }

    // Load GStreamer core functions
    LoadGStreamerCoreFunctions(gst_lib);

    // Load GStreamer app functions
    LoadGStreamerAppFunctions(gstapp_lib);

    // Load GLib functions
    LoadGLibFunctions(glib_lib, gobject_lib);

    // Validate that all functions were loaded successfully
    if (!ValidateFunctionLoading()) {
        // Note: Libraries should NOT be closed here as function pointers depend on them
        // The libraries will remain loaded for the lifetime of the application
        return false;
    }

    // Initialize GStreamer
    gst_init(nullptr, nullptr);
    gst_initialized_ = true;

    std::cout << "✅ GStreamer initialized successfully" << std::endl;
    return true;
}

} // namespace segmecam