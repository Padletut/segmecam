#pragma once

#include <dlfcn.h>
#include <string>
#include <cstdint>

// Runtime detection of Flatpak environment
static bool IsRunningInFlatpak() {
    const char* flatpak_id = std::getenv("FLATPAK_ID");
    // FLATPAK_ID is set by Flatpak runtime, presence indicates sandboxed environment
    // This is a safe check as we only verify existence, not use the value
    return flatpak_id != nullptr && flatpak_id[0] != '\0';
}

// PipeWire/GStreamer support (loaded at runtime when in Flatpak)
#ifdef __cplusplus
extern "C" {
#endif
// GStreamer types (forward declared to avoid header dependencies)
typedef struct _GstElement GstElement;
typedef struct _GstAppSink GstAppSink;
typedef struct _GstSample GstSample;
typedef struct _GstBuffer GstBuffer;
typedef struct _GstCaps GstCaps;
typedef struct _GstMapInfo GstMapInfo;
typedef struct _GstStructure GstStructure;
typedef struct _GMainLoop GMainLoop;
typedef struct _GError GError;

// GLib types
typedef int gboolean;
typedef void* gpointer;

// GStreamer functions (loaded dynamically)
typedef void (*gst_init_func)(int*, char***);
typedef GstElement* (*gst_pipeline_new_func)(const char*);
typedef GstElement* (*gst_element_factory_make_func)(const char*, const char*);
typedef int (*gst_element_set_state_func)(GstElement*, int);
typedef int (*gst_element_get_state_func)(GstElement*, int*, int*, uint64_t);
typedef int (*gst_bin_add_many_func)(void*, ...);
typedef int (*gst_element_link_many_func)(void*, ...);
typedef void (*gst_object_unref_func)(void*);
typedef void* (*gst_app_sink_pull_sample_func)(GstAppSink*);
typedef int (*gst_app_sink_is_eos_func)(GstAppSink*);
typedef void* (*gst_sample_get_buffer_func)(GstSample*);
typedef void* (*gst_sample_get_caps_func)(GstSample*);
typedef int (*gst_buffer_map_func)(GstBuffer*, GstMapInfo*, int);
typedef void (*gst_buffer_unmap_func)(GstBuffer*, GstMapInfo*);

// GStreamer sample unref function
typedef void (*gst_sample_unref_func)(GstSample*);

// Additional GStreamer function pointer types
typedef GstCaps* (*gst_caps_new_simple_func)(const char*, ...);
typedef GstCaps* (*gst_caps_from_string_func)(const char*);
typedef void (*gst_caps_unref_func)(GstCaps*);
typedef void (*gst_message_parse_error_func)(void*, void**, void**);
typedef void* (*gst_bus_timed_pop_filtered_func)(void*, uint64_t, int);
typedef void* (*gst_element_get_bus_func)(GstElement*);
typedef void (*gst_bus_unref_func)(void*);
typedef void (*gst_message_unref_func)(void*);
typedef GstElement* (*gst_parse_launch_func)(const char*, void**);
typedef GstElement* (*gst_bin_get_by_name_func)(void*, const char*);

// GLib function pointer types
typedef void (*g_main_loop_quit_func)(GMainLoop*);
typedef void (*g_main_loop_unref_func)(GMainLoop*);
typedef void (*g_object_set_func)(void*, const char*, ...);
typedef unsigned long (*g_signal_connect_func)(void*, const char*, void*, void*);

typedef GMainLoop* (*g_main_loop_new_func)(void*, gboolean);
typedef void (*g_main_loop_run_func)(GMainLoop*);
typedef void (*g_object_unref_func)(void*);

// Additional utility function pointer types
typedef const char* (*gst_structure_get_string_func)(const GstStructure*, const char*);
typedef void (*g_usleep_func)(unsigned long);
typedef void (*g_error_free_func)(void*);

// External function pointers (shared across camera files)
extern gst_init_func gst_init;
extern gst_pipeline_new_func gst_pipeline_new;
extern gst_element_factory_make_func gst_element_factory_make;
extern gst_element_set_state_func gst_element_set_state;
extern gst_element_get_state_func gst_element_get_state;
extern gst_bin_add_many_func gst_bin_add_many;
extern gst_element_link_many_func gst_element_link_many;
extern gst_object_unref_func gst_object_unref;
extern gst_app_sink_pull_sample_func gst_app_sink_pull_sample;
extern gst_app_sink_is_eos_func gst_app_sink_is_eos;
extern gst_sample_get_buffer_func gst_sample_get_buffer;
extern gst_sample_get_caps_func gst_sample_get_caps;
extern gst_buffer_map_func gst_buffer_map;
extern gst_buffer_unmap_func gst_buffer_unmap;
extern gst_sample_unref_func gst_sample_unref;

// GLib functions
void g_main_loop_new(gpointer context, gboolean is_running);
void g_main_loop_run(GMainLoop* loop);
void g_main_loop_quit(GMainLoop* loop);
void g_main_loop_unref(GMainLoop* loop);
void g_error_free(GError* error);
void g_object_unref(gpointer object);

// GStreamer constants
const int TRUE = 1;
const int FALSE = 0;
const uint64_t GST_CLOCK_TIME_NONE = (uint64_t)-1;

// Macros for GStreamer/GLib
#define G_CALLBACK(f) ((void*)(f))
#define GST_BIN(obj) ((void*)(obj))
#ifdef __cplusplus
}
#endif

// Utility function to load GStreamer function pointers
bool LoadGStreamerFunctions(void* gst_lib);