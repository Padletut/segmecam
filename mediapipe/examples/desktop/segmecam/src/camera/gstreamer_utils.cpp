#include "include/camera/gstreamer_utils.h"

// GStreamer function pointers (shared across camera files)
gst_init_func gst_init = nullptr;
gst_pipeline_new_func gst_pipeline_new = nullptr;
gst_element_factory_make_func gst_element_factory_make = nullptr;
gst_element_set_state_func gst_element_set_state = nullptr;
gst_element_get_state_func gst_element_get_state = nullptr;
gst_bin_add_many_func gst_bin_add_many = nullptr;
gst_element_link_many_func gst_element_link_many = nullptr;
gst_object_unref_func gst_object_unref = nullptr;
gst_app_sink_pull_sample_func gst_app_sink_pull_sample = nullptr;
gst_app_sink_is_eos_func gst_app_sink_is_eos = nullptr;
gst_sample_get_buffer_func gst_sample_get_buffer = nullptr;
gst_sample_get_caps_func gst_sample_get_caps = nullptr;
gst_buffer_map_func gst_buffer_map = nullptr;
gst_buffer_unmap_func gst_buffer_unmap = nullptr;
gst_sample_unref_func gst_sample_unref = nullptr;

// Structure to define GStreamer functions for loading
struct GStreamerFunction {
    const char* name;
    void** func_ptr;
};

bool LoadGStreamerFunctions(void* gst_lib) {
    if (!gst_lib) return false;

    // Define all GStreamer functions to load
    static const GStreamerFunction functions[] = {
        {"gst_init", reinterpret_cast<void**>(&gst_init)},
        {"gst_pipeline_new", reinterpret_cast<void**>(&gst_pipeline_new)},
        {"gst_element_factory_make", reinterpret_cast<void**>(&gst_element_factory_make)},
        {"gst_element_set_state", reinterpret_cast<void**>(&gst_element_set_state)},
        {"gst_element_get_state", reinterpret_cast<void**>(&gst_element_get_state)},
        {"gst_bin_add_many", reinterpret_cast<void**>(&gst_bin_add_many)},
        {"gst_element_link_many", reinterpret_cast<void**>(&gst_element_link_many)},
        {"gst_object_unref", reinterpret_cast<void**>(&gst_object_unref)},
        {"gst_app_sink_pull_sample", reinterpret_cast<void**>(&gst_app_sink_pull_sample)},
        {"gst_app_sink_is_eos", reinterpret_cast<void**>(&gst_app_sink_is_eos)},
        {"gst_sample_get_buffer", reinterpret_cast<void**>(&gst_sample_get_buffer)},
        {"gst_sample_get_caps", reinterpret_cast<void**>(&gst_sample_get_caps)},
        {"gst_buffer_map", reinterpret_cast<void**>(&gst_buffer_map)},
        {"gst_buffer_unmap", reinterpret_cast<void**>(&gst_buffer_unmap)},
        {"gst_sample_unref", reinterpret_cast<void**>(&gst_sample_unref)}
    };

    // Load all functions without individual checks
    for (const auto& func : functions) {
        *func.func_ptr = dlsym(gst_lib, func.name);
    }

    // Single validation check for all functions
    for (const auto& func : functions) {
        if (!*func.func_ptr) {
            return false;
        }
    }

    return true;
}