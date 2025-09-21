#include "mediapipe/examples/desktop/segmecam/include/camera/camera_manager.h"
#include <iostream>
#include <vector>
#include <dlfcn.h>
#include <cstdlib>

namespace segmecam {

bool CameraManager::InitializeGStreamer() {
    if (gst_initialized_) {
        return true;
    }

    std::cout << "🎬 Initializing GStreamer for PipeWire support..." << std::endl;

    // Load GStreamer core library
    std::cout << "🔍 DEBUG: Attempting to load libgstreamer-1.0.so.0" << std::endl;
    void* gst_lib = dlopen("libgstreamer-1.0.so.0", RTLD_LAZY);
    if (!gst_lib) {
        std::cerr << "❌ Failed to load libgstreamer-1.0.so.0: " << dlerror() << std::endl;
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libgstreamer-1.0.so.0" << std::endl;

    // Load GStreamer app library (needed for gst_app_sink_* functions)
    std::cout << "🔍 DEBUG: Attempting to load libgstapp-1.0.so.0" << std::endl;
    void* gstapp_lib = dlopen("libgstapp-1.0.so.0", RTLD_LAZY);
    if (!gstapp_lib) {
        std::cerr << "❌ Failed to load libgstapp-1.0.so.0: " << dlerror() << std::endl;
        dlclose(gst_lib);
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libgstapp-1.0.so.0" << std::endl;

    // Load GStreamer video library (needed for video format helpers)
    std::cout << "🔍 DEBUG: Attempting to load libgstvideo-1.0.so.0" << std::endl;
    void* gstvideo_lib = dlopen("libgstvideo-1.0.so.0", RTLD_LAZY);
    if (!gstvideo_lib) {
        std::cerr << "❌ Failed to load libgstvideo-1.0.so.0: " << dlerror() << std::endl;
        dlclose(gst_lib);
        dlclose(gstapp_lib);
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libgstvideo-1.0.so.0" << std::endl;

    // Load GLib library
    std::cout << "🔍 DEBUG: Attempting to load libglib-2.0.so.0" << std::endl;
    void* glib_lib = dlopen("libglib-2.0.so.0", RTLD_LAZY);
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
    void* gobject_lib = dlopen("libgobject-2.0.so.0", RTLD_LAZY);
    if (!gobject_lib) {
        std::cerr << "❌ Failed to load libgobject-2.0.so.0: " << dlerror() << std::endl;
        dlclose(gst_lib);
        dlclose(gstapp_lib);
        dlclose(gstvideo_lib);
        dlclose(glib_lib);
        return false;
    }
    std::cout << "✅ DEBUG: Successfully loaded libgobject-2.0.so.0" << std::endl;

    // Load GStreamer functions from core library
    gst_init = (gst_init_func)dlsym(gst_lib, "gst_init");
    if (!gst_init) std::cerr << "❌ gst_init dlerror: " << dlerror() << std::endl;
    gst_pipeline_new = (GstElement* (*)(const char*))dlsym(gst_lib, "gst_pipeline_new");
    if (!gst_pipeline_new) std::cerr << "❌ gst_pipeline_new dlerror: " << dlerror() << std::endl;
    gst_element_factory_make = (GstElement* (*)(const char*, const char*))dlsym(gst_lib, "gst_element_factory_make");
    if (!gst_element_factory_make) std::cerr << "❌ gst_element_factory_make dlerror: " << dlerror() << std::endl;
    gst_element_set_state = (int (*)(GstElement*, int))dlsym(gst_lib, "gst_element_set_state");
    if (!gst_element_set_state) std::cerr << "❌ gst_element_set_state dlerror: " << dlerror() << std::endl;
    gst_element_get_state = (int (*)(GstElement*, int*, int*, uint64_t))dlsym(gst_lib, "gst_element_get_state");
    if (!gst_element_get_state) std::cerr << "❌ gst_element_get_state dlerror: " << dlerror() << std::endl;
    gst_bin_add_many = (int (*)(void*, ...))dlsym(gst_lib, "gst_bin_add_many");
    if (!gst_bin_add_many) std::cerr << "❌ gst_bin_add_many dlerror: " << dlerror() << std::endl;
    gst_element_link_many = (int (*)(void*, ...))dlsym(gst_lib, "gst_element_link_many");
    if (!gst_element_link_many) std::cerr << "❌ gst_element_link_many dlerror: " << dlerror() << std::endl;
    gst_object_unref = (void (*)(void*))dlsym(gst_lib, "gst_object_unref");
    if (!gst_object_unref) std::cerr << "❌ gst_object_unref dlerror: " << dlerror() << std::endl;
    gst_sample_get_buffer = (void* (*)(GstSample*))dlsym(gst_lib, "gst_sample_get_buffer");
    if (!gst_sample_get_buffer) std::cerr << "❌ gst_sample_get_buffer dlerror: " << dlerror() << std::endl;
    gst_sample_get_caps = (void* (*)(GstSample*))dlsym(gst_lib, "gst_sample_get_caps");
    if (!gst_sample_get_caps) std::cerr << "❌ gst_sample_get_caps dlerror: " << dlerror() << std::endl;
    gst_caps_from_string = (gst_caps_from_string_func)dlsym(gst_lib, "gst_caps_from_string");
    if (!gst_caps_from_string) std::cerr << "❌ gst_caps_from_string dlerror: " << dlerror() << std::endl;
    gst_caps_unref = (gst_caps_unref_func)dlsym(gst_lib, "gst_caps_unref");
    if (!gst_caps_unref) std::cerr << "❌ gst_caps_unref dlerror: " << dlerror() << std::endl;
    gst_caps_get_structure = (GstStructure* (*)(GstCaps*, unsigned int))dlsym(gst_lib, "gst_caps_get_structure");
    if (!gst_caps_get_structure) std::cerr << "❌ gst_caps_get_structure dlerror: " << dlerror() << std::endl;
    gst_structure_get_int = (int (*)(const GstStructure*, const char*, int*))dlsym(gst_lib, "gst_structure_get_int");
    if (!gst_structure_get_int) std::cerr << "❌ gst_structure_get_int dlerror: " << dlerror() << std::endl;
    gst_structure_get_string = (const char* (*)(const GstStructure*, const char*))dlsym(gst_lib, "gst_structure_get_string");
    if (!gst_structure_get_string) std::cerr << "⚠️  gst_structure_get_string dlerror: " << dlerror() << std::endl;
    gst_element_link = (int (*)(void*, void*))dlsym(gst_lib, "gst_element_link");
    if (!gst_element_link) std::cerr << "❌ gst_element_link dlerror: " << dlerror() << std::endl;
    gst_element_link_filtered = (int (*)(void*, void*, GstCaps*))dlsym(gst_lib, "gst_element_link_filtered");
    if (!gst_element_link_filtered) std::cerr << "❌ gst_element_link_filtered dlerror: " << dlerror() << std::endl;
    gst_parse_launch = (GstElement* (*)(const char*, void**))dlsym(gst_lib, "gst_parse_launch");
    if (!gst_parse_launch) std::cerr << "❌ gst_parse_launch dlerror: " << dlerror() << std::endl;
    gst_bin_get_by_name = (GstElement* (*)(void*, const char*))dlsym(gst_lib, "gst_bin_get_by_name");
    if (!gst_bin_get_by_name) std::cerr << "❌ gst_bin_get_by_name dlerror: " << dlerror() << std::endl;
    gst_buffer_map = (int (*)(GstBuffer*, GstMapInfo*, int))dlsym(gst_lib, "gst_buffer_map");
    if (!gst_buffer_map) std::cerr << "❌ gst_buffer_map dlerror: " << dlerror() << std::endl;
    gst_buffer_unmap = (void (*)(GstBuffer*, GstMapInfo*))dlsym(gst_lib, "gst_buffer_unmap");
    if (!gst_buffer_unmap) std::cerr << "❌ gst_buffer_unmap dlerror: " << dlerror() << std::endl;
    gst_sample_unref = (void (*)(GstSample*))dlsym(gst_lib, "gst_sample_unref");
    if (!gst_sample_unref) std::cerr << "❌ gst_sample_unref dlerror: " << dlerror() << std::endl;

    // Load GStreamer app functions from app library
    gst_app_sink_pull_sample = (void* (*)(GstAppSink*))dlsym(gstapp_lib, "gst_app_sink_pull_sample");
    if (!gst_app_sink_pull_sample) std::cerr << "❌ gst_app_sink_pull_sample dlerror: " << dlerror() << std::endl;
    gst_app_sink_is_eos = (int (*)(GstAppSink*))dlsym(gstapp_lib, "gst_app_sink_is_eos");
    if (!gst_app_sink_is_eos) std::cerr << "❌ gst_app_sink_is_eos dlerror: " << dlerror() << std::endl;

    // Load GLib functions
    g_object_set = (void (*)(void*, const char*, ...))dlsym(gobject_lib, "g_object_set");
    if (!g_object_set) std::cerr << "❌ g_object_set dlerror: " << dlerror() << std::endl;
    g_main_loop_quit = (g_main_loop_quit_func)dlsym(glib_lib, "g_main_loop_quit");
    if (!g_main_loop_quit) std::cerr << "❌ g_main_loop_quit dlerror: " << dlerror() << std::endl;
    g_main_loop_unref = (g_main_loop_unref_func)dlsym(glib_lib, "g_main_loop_unref");
    if (!g_main_loop_unref) std::cerr << "❌ g_main_loop_unref dlerror: " << dlerror() << std::endl;
    g_signal_connect = (g_signal_connect_func)dlsym(gobject_lib, "g_signal_connect_data");
    if (!g_signal_connect) std::cerr << "❌ g_signal_connect dlerror: " << dlerror() << std::endl;
    g_main_loop_new = (g_main_loop_new_func)dlsym(glib_lib, "g_main_loop_new");
    if (!g_main_loop_new) std::cerr << "❌ g_main_loop_new dlerror: " << dlerror() << std::endl;
    g_main_loop_run = (g_main_loop_run_func)dlsym(glib_lib, "g_main_loop_run");
    if (!g_main_loop_run) std::cerr << "❌ g_main_loop_run dlerror: " << dlerror() << std::endl;
    g_object_unref_ptr = (g_object_unref_func)dlsym(gobject_lib, "g_object_unref");
    if (!g_object_unref_ptr) std::cerr << "❌ g_object_unref dlerror: " << dlerror() << std::endl;
    g_usleep = (g_usleep_func)dlsym(glib_lib, "g_usleep");
    if (!g_usleep) std::cerr << "❌ g_usleep dlerror: " << dlerror() << std::endl;
    g_error_free = (g_error_free_func)dlsym(glib_lib, "g_error_free");
    if (!g_error_free) std::cerr << "❌ g_error_free dlerror: " << dlerror() << std::endl;

    // Check if all functions were loaded
    std::vector<std::string> failed_functions;
    if (!gst_init) failed_functions.push_back("gst_init");
    if (!gst_pipeline_new) failed_functions.push_back("gst_pipeline_new");
    if (!gst_element_factory_make) failed_functions.push_back("gst_element_factory_make");
    if (!gst_element_set_state) failed_functions.push_back("gst_element_set_state");
    if (!gst_element_get_state) failed_functions.push_back("gst_element_get_state");
    if (!gst_bin_add_many) failed_functions.push_back("gst_bin_add_many");
    if (!gst_element_link_many) failed_functions.push_back("gst_element_link_many");
    if (!gst_object_unref) failed_functions.push_back("gst_object_unref");
    if (!gst_app_sink_pull_sample) failed_functions.push_back("gst_app_sink_pull_sample");
    if (!gst_app_sink_is_eos) failed_functions.push_back("gst_app_sink_is_eos");
    if (!gst_sample_get_buffer) failed_functions.push_back("gst_sample_get_buffer");
    if (!gst_sample_get_caps) failed_functions.push_back("gst_sample_get_caps");
    if (!gst_caps_from_string) failed_functions.push_back("gst_caps_from_string");
    if (!gst_caps_unref) failed_functions.push_back("gst_caps_unref");
    if (!gst_caps_get_structure) failed_functions.push_back("gst_caps_get_structure");
    if (!gst_structure_get_int) failed_functions.push_back("gst_structure_get_int");
    if (!gst_element_link) failed_functions.push_back("gst_element_link");
    if (!gst_element_link_filtered) failed_functions.push_back("gst_element_link_filtered");
    if (!gst_parse_launch) failed_functions.push_back("gst_parse_launch");
    if (!gst_bin_get_by_name) failed_functions.push_back("gst_bin_get_by_name");
    if (!gst_buffer_map) failed_functions.push_back("gst_buffer_map");
    if (!gst_buffer_unmap) failed_functions.push_back("gst_buffer_unmap");
    if (!gst_sample_unref) failed_functions.push_back("gst_sample_unref");
    if (!g_object_set) failed_functions.push_back("g_object_set");
    if (!g_main_loop_quit) failed_functions.push_back("g_main_loop_quit");
    if (!g_main_loop_unref) failed_functions.push_back("g_main_loop_unref");
    if (!g_signal_connect) failed_functions.push_back("g_signal_connect");
    if (!g_main_loop_new) failed_functions.push_back("g_main_loop_new");
    if (!g_main_loop_run) failed_functions.push_back("g_main_loop_run");
    if (!g_object_unref_ptr) failed_functions.push_back("g_object_unref");
    if (!g_usleep) failed_functions.push_back("g_usleep");
    if (!g_error_free) failed_functions.push_back("g_error_free");

    if (!failed_functions.empty()) {
        std::cerr << "❌ Failed to load GStreamer/GLib functions: ";
        for (size_t i = 0; i < failed_functions.size(); ++i) {
            if (i > 0) std::cerr << ", ";
            std::cerr << failed_functions[i];
        }
        std::cerr << std::endl;
        dlclose(gst_lib);
        dlclose(gstapp_lib);
        dlclose(gstvideo_lib);
        dlclose(glib_lib);
        return false;
    }

    // Initialize GStreamer
    gst_init(nullptr, nullptr);
    gst_initialized_ = true;

    std::cout << "✅ GStreamer initialized successfully" << std::endl;
    return true;
}

} // namespace segmecam