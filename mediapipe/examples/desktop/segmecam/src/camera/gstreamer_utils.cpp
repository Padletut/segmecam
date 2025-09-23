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

bool LoadGStreamerFunctions(void* gst_lib) {
    if (!gst_lib) return false;

    // Load all GStreamer function pointers
    gst_init = reinterpret_cast<gst_init_func>(dlsym(gst_lib, "gst_init"));
    gst_pipeline_new = reinterpret_cast<gst_pipeline_new_func>(dlsym(gst_lib, "gst_pipeline_new"));
    gst_element_factory_make = reinterpret_cast<gst_element_factory_make_func>(dlsym(gst_lib, "gst_element_factory_make"));
    gst_element_set_state = reinterpret_cast<gst_element_set_state_func>(dlsym(gst_lib, "gst_element_set_state"));
    gst_element_get_state = reinterpret_cast<gst_element_get_state_func>(dlsym(gst_lib, "gst_element_get_state"));
    gst_bin_add_many = reinterpret_cast<gst_bin_add_many_func>(dlsym(gst_lib, "gst_bin_add_many"));
    gst_element_link_many = reinterpret_cast<gst_element_link_many_func>(dlsym(gst_lib, "gst_element_link_many"));
    gst_object_unref = reinterpret_cast<gst_object_unref_func>(dlsym(gst_lib, "gst_object_unref"));
    gst_app_sink_pull_sample = reinterpret_cast<gst_app_sink_pull_sample_func>(dlsym(gst_lib, "gst_app_sink_pull_sample"));
    gst_app_sink_is_eos = reinterpret_cast<gst_app_sink_is_eos_func>(dlsym(gst_lib, "gst_app_sink_is_eos"));
    gst_sample_get_buffer = reinterpret_cast<gst_sample_get_buffer_func>(dlsym(gst_lib, "gst_sample_get_buffer"));
    gst_sample_get_caps = reinterpret_cast<gst_sample_get_caps_func>(dlsym(gst_lib, "gst_sample_get_caps"));
    gst_buffer_map = reinterpret_cast<gst_buffer_map_func>(dlsym(gst_lib, "gst_buffer_map"));
    gst_buffer_unmap = reinterpret_cast<gst_buffer_unmap_func>(dlsym(gst_lib, "gst_buffer_unmap"));
    gst_sample_unref = reinterpret_cast<gst_sample_unref_func>(dlsym(gst_lib, "gst_sample_unref"));

    // Check if all functions loaded successfully
    return gst_init && gst_pipeline_new && gst_element_factory_make &&
           gst_element_set_state && gst_element_get_state && gst_bin_add_many &&
           gst_element_link_many && gst_object_unref && gst_app_sink_pull_sample &&
           gst_app_sink_is_eos && gst_sample_get_buffer && gst_sample_get_caps &&
           gst_buffer_map && gst_buffer_unmap && gst_sample_unref;
}