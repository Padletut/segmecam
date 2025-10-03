load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library") # type: ignore

# Export ar_filters headers for use by subdirectory BUILD files
exports_files([
    "include/ar_filters/blendshape_processor.h",
    "include/ar_filters/face_mesh_processor.h",
    "include/ar_filters/transform_calculator.h",
    "include/ar_filters/model_loader.h",
    "include/ar_filters/filter_object.h",
    "include/ar_filters/attachment_controller.h",
    "include/ar_filters/filter_test_demo.h",
    "include/ar_filters/filter_presets.h",
    "include/ar_filters/opengl_renderer.h",
    "include/ar_filters/texture_manager.h",
    "include/ar_filters/ar_renderer.h",
    "include/ar_filters/filter_asset.h",  # Phase 6
    "include/ar_filters/ar_filter_manager.h",  # Phase 7
])

cc_library( # type: ignore
    name = "compositing_utils",
    srcs = ["src/compositing_utils.cpp"],
    hdrs = ["include/compositing_utils.h"],
    includes = ["."],
    deps = [
    "//mediapipe/framework/port:opencv_core",
    "//mediapipe/framework/port:opencv_imgproc",
    "//mediapipe/framework/port:opencv_photo",
    ],
    copts = ["-I/usr/include/opencv4"],
)

cc_library( # type: ignore
    name = "segmecam_composite",
    srcs = ["src/render/segmecam_composite.cpp"],
    hdrs = ["include/render/segmecam_composite.h"],
    includes = ["."],
    deps = [
        ":compositing_utils",
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
)

cc_library( # type: ignore
    name = "segmecam_face_effects",
    srcs = ["src/effects/segmecam_face_effects.cpp", "src/camera/cam_enum.cpp"],
    hdrs = ["include/effects/segmecam_face_effects.h", "include/camera/cam_enum.h"],
    includes = [".", "include"],
    deps = [
        ":face_regions",
        ":lip_effects",
        ":teeth_effects",
        ":skin_effects",
        ":advanced_skin_effects",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = ["-I/usr/include/opencv4"],
)

cc_library( # type: ignore
    name = "presets",
    srcs = ["src/effects/presets.cpp"],
    hdrs = ["include/effects/presets.h"],
    includes = ["."],
    deps = [
        # ...existing deps...
        "//mediapipe/calculators/core:split_vector_calculator",
        "//mediapipe/calculators/core:split_vector_calculator_cc_proto",
        # ...existing deps...
    ],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
)

cc_library( # type: ignore
    name = "vcam",
    srcs = ["src/camera/vcam.cpp"],
    hdrs = ["include/camera/vcam.h"],
    includes = ["."],
    deps = [
        ":gstreamer_buffer_utils",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = ["-I/usr/include/opencv4"],
    linkopts = ["-lopencv_core", "-lopencv_imgproc"],
)

cc_library( # type: ignore
    name = "app_state",
    srcs = ["src/application/app_state.cpp"],
    hdrs = ["include/application/app_state.h"],
    includes = ["."],
    deps = [
        ":vcam",
        ":advanced_skin_effects",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:attachment_controller",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:blendshape_processor",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:face_mesh_processor",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:transform_calculator",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:filter_test_demo",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:filter_presets",
        "//mediapipe/framework/formats:classification_cc_proto",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/framework/port:opencv_imgcodecs",
    ],
    copts = ["-I/usr/include/opencv4"],
    linkopts = ["-lopencv_core", "-lopencv_imgproc", "-lopencv_imgcodecs"],
)

cc_library( # type: ignore
    name = "gpu_detector",
    srcs = ["src/mediapipe_manager/gpu_detector.cpp"],
    hdrs = ["include/mediapipe_manager/gpu_detector.h"],
    includes = ["."],
    deps = [],
    linkopts = [
        "-lEGL",
        "-lGL",
        "-ldl",
    ],
)

# Extracted Application Modules (Phase 1 Modular Refactoring)
cc_library( # type: ignore
    name = "application_config",
    srcs = ["src/application/application_config.cpp"],
    hdrs = ["include/application/application_config.h"],
    includes = [".", "include"],
    deps = [],
)

cc_library( # type: ignore
    name = "gpu_setup",
    srcs = ["src/application/gpu_setup.cpp"],
    hdrs = ["include/application/gpu_setup.h"],
    includes = [".", "include"],
    deps = [
        ":gpu_detector",
    ],
)

cc_library( # type: ignore
    name = "mediapipe_setup",
    srcs = ["src/application/mediapipe_setup.cpp"],
    hdrs = ["include/application/mediapipe_setup.h"],
    includes = [".", "include"],
    deps = [
        ":application_config",
        ":gpu_detector",
        "//mediapipe/framework:calculator_graph",
        "//mediapipe/framework/port:file_helpers",
        "//mediapipe/framework/port:parse_text_proto",
        "//mediapipe/framework/port:status",
        "//mediapipe/gpu:gpu_shared_data_internal",
        "//mediapipe/util:resource_util",
        "@com_google_absl//absl/flags:flag",
        # Required calculators for selfie segmentation
        "//mediapipe/graphs/selfie_segmentation:selfie_segmentation_gpu_deps",
        "//mediapipe/graphs/selfie_segmentation:selfie_segmentation_cpu_deps",
        "//mediapipe/calculators/util:to_image_calculator",
        "//mediapipe/gpu:gl_calculator_helper",
        "//mediapipe/gpu:gpu_buffer",
        "//mediapipe/gpu:gpu_buffer_to_image_frame_calculator",
        "//mediapipe/gpu:image_frame_to_gpu_buffer_calculator",
        # Flow limiter dependency
        "//mediapipe/calculators/core:flow_limiter_calculator",
        # For blendshapes vector extraction
        "//mediapipe/calculators/core:split_vector_calculator",
        "//mediapipe/calculators/core:split_vector_calculator_cc_proto",
    ],
)

cc_library( # type: ignore
    name = "camera_setup",
    srcs = ["src/application/camera_setup.cpp"],
    hdrs = ["include/application/camera_setup.h"],
    includes = [".", "include"],
    deps = [
        ":segmecam_face_effects",  # includes cam_enum.h and cam_enum.cc
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_highgui",
    ],
    copts = [
        "-I/usr/include/opencv4",
        "-I/usr/include/spa-0.2",
        "-I/usr/include/pipewire-0.3",
    ],
    linkopts = ["-lopencv_core", "-lopencv_highgui"],
)

cc_library( # type: ignore
    name = "manager_coordination",
    srcs = ["src/application/manager_coordination.cpp"],
    hdrs = ["include/application/manager_coordination.h"],
    includes = [".", "include"],
    deps = [
        ":app_state",
        ":application_run",  # Phase 8: Needed for ApplicationRun types
        ":camera_manager",
        ":effects_manager",  # Phase 8: EffectsManager included in cpp
        ":ui_manager",  # Phase 8: UIManager included in cpp
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:opengl_renderer",  # Phase 8: OpenGLRenderer for AR rendering
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:ar_filter_manager",  # Phase 8: AR filter system
        "//mediapipe/examples/desktop/segmecam/src/config:config_manager",
    ],
)

cc_library( # type: ignore
    name = "application_cleanup",
    srcs = ["src/application/application_cleanup.cpp"],
    hdrs = ["include/application/application_cleanup.h"],
    includes = [".", "include"],
    deps = [
        ":manager_coordination",
        "//mediapipe/framework:calculator_graph",
    ],
    copts = ["-I/usr/include/SDL2"],
    linkopts = ["-lSDL2"],
)

cc_library( # type: ignore
    name = "application_run",
    srcs = ["src/application/application_run.cpp"],
    hdrs = ["include/application/application_run.h"],
    includes = [".", "include"],
    deps = [
        ":manager_coordination",
        ":app_state",
        ":ui_manager_enhanced",
        "//mediapipe/framework:calculator_graph",
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/formats:image_frame_opencv",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "//third_party/imgui:imgui",
    ],
    copts = [
        "-I/usr/include/SDL2",
        "-I/usr/include/opencv4",
    ],
    linkopts = [
        "-lSDL2",
        "-lGL",
        "-lopencv_core",
        "-lopencv_imgproc",
    ],
)

cc_library( # type: ignore
    name = "application_initialization",
    srcs = ["src/application/application_initialization.cpp"],
    hdrs = ["include/application/application_initialization.h"],
    includes = [".", "include"],
    deps = [
        ":manager_coordination",
        ":mediapipe_setup",
        ":gpu_setup",
        ":app_state",
        "//mediapipe/framework:calculator_graph",
        "//mediapipe/framework:output_stream_poller",
        "//third_party/imgui:imgui_lib",
        "//third_party/imgui:imgui_impl_sdl2",
        "//third_party/imgui:imgui_impl_opengl3",
        "//mediapipe/calculators/core:split_vector_calculator",
        "//mediapipe/calculators/core:split_vector_calculator_cc_proto",
    ],
    copts = [
        "-I/usr/include/SDL2",
    ],
    linkopts = [
        "-lSDL2",
        "-lGL",
    ],
)

# MediaPipe Manager Library (Phase 2 Refactoring)
cc_library( # type: ignore
    name = "mediapipe_manager",
    srcs = ["src/mediapipe_manager/mediapipe_manager.cpp"],
    hdrs = ["include/mediapipe_manager/mediapipe_manager.h"],
    includes = [".", "include"],
    deps = [
        ":gpu_detector",
        "//mediapipe/framework:calculator_graph", 
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/port:file_helpers",
        "//mediapipe/framework/port:parse_text_proto",
        "//mediapipe/framework/port:status",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/gpu:gpu_shared_data_internal",
        # Required calculators for selfie segmentation
        "//mediapipe/graphs/selfie_segmentation:selfie_segmentation_gpu_deps",
        "//mediapipe/calculators/util:to_image_calculator",
        "//mediapipe/gpu:gl_calculator_helper",
        "//mediapipe/gpu:gpu_buffer",
        "//mediapipe/gpu:gpu_buffer_to_image_frame_calculator",
        "//mediapipe/gpu:image_frame_to_gpu_buffer_calculator",
        "@com_google_absl//absl/flags:flag",
    ],
)

# Camera Manager Library (Phase 4 Refactoring)
cc_library( # type: ignore
    name = "gstreamer_buffer_utils",
    srcs = ["src/camera/gstreamer_buffer_utils.cpp"],
    hdrs = ["include/camera/gstreamer_buffer_utils.h"],
    includes = [".", "include"],
    deps = [
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = [
        "-I/usr/include/opencv4",
    ] + select({
        "//conditions:default": [],
        ":flatpak_build": [],
    }),
    linkopts = [
        "-lopencv_core",
        "-lopencv_imgproc",
        "-lgstreamer-1.0",
        "-lgstapp-1.0",
        "-lgstvideo-1.0",
        "-lglib-2.0",
        "-lgio-2.0",
    ] + select({
        "//conditions:default": [],
        ":flatpak_build": [
            "-lgstreamer-1.0",
            "-lgstapp-1.0",
            "-lgstvideo-1.0",
            "-lglib-2.0",
            "-lgio-2.0",
        ],
    }),
)

# Camera Manager Library (Phase 4 Refactoring)
cc_library( # type: ignore
    name = "camera_manager",
    srcs = [
        "src/camera/camera_manager.cpp",
        "src/camera/camera_initialization.cpp",
        "src/camera/camera_opening.cpp",
        "src/camera/camera_capture.cpp",
        "src/camera/camera_gstreamer_init.cpp",
        "src/camera/camera_gstreamer_cleanup.cpp",
        "src/camera/camera_portal_permission.cpp",
        "src/camera/camera_pipewire.cpp",
        "src/camera/camera_gstreamer_open.cpp",
        "src/camera/camera_gstreamer_close.cpp",
        "src/camera/camera_gstreamer_capture.cpp",
        "src/camera/camera_gstreamer_conversion.cpp",
        "src/camera/camera_gstreamer_callbacks.cpp",
        "src/camera/gstreamer_utils.cpp",
        "src/camera/camera_controls.cpp",
        "src/camera/camera_enumeration.cpp",
        "src/camera/camera_setup.cpp",
        "src/camera/cam_enum.cpp",  # V4L2 control functions
    ],
    hdrs = [
        "include/camera/camera_manager.h", 
        "include/camera/gstreamer_utils.h",
        "include/camera/camera_controls.h",
        "include/camera/camera_enumeration.h",
        "include/camera/camera_setup.h",
    ],
    includes = [".", "include"],
    deps = [
        ":gstreamer_buffer_utils",  # for buffer conversion utilities
        ":segmecam_face_effects",  # for cam_enum.h
        ":pipewire_output",  # for PipeWire video output
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_highgui",
    ],
    copts = [
        "-I/usr/include/opencv4",
        "-I/usr/include/pipewire-0.3",
    ] + select({
        "//conditions:default": [],
        ":flatpak_build": [],
    }),
    linkopts = [
        "-lopencv_core",
        "-lopencv_imgproc",
        "-lopencv_highgui",
    ] + select({
        "//conditions:default": [],
        ":flatpak_build": [
            "-lgstreamer-1.0",
            "-lgstapp-1.0",
            "-lgstvideo-1.0",
            "-lglib-2.0",
            "-lgio-2.0",
            "-lportal",
        ],
    }),
)

config_setting(
    name = "flatpak_build",
    define_values = {
        "FLATPAK_BUILD": "true",
    },
)

# PipeWire Output Library (Flatpak Video Output)
cc_library( # type: ignore
    name = "pipewire_output",
    srcs = [
        "src/camera/pipewire_output.cpp",
        "src/camera/pipewire_frame_converters.cpp",
        "src/camera/pipewire_callbacks.cpp",
    ],
    hdrs = [
        "include/camera/pipewire_output.h",
        "include/camera/pipewire_frame_converters.h",
        "include/camera/pipewire_callbacks.h",
    ],
    includes = [".", "include"],
    deps = [
        ":gstreamer_buffer_utils",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = [
        "-I/usr/include/opencv4",
        "-I/usr/include/pipewire-0.3",
        "-I/usr/include/spa-0.2",
    ],
    linkopts = [
        "-lopencv_core",
        "-lopencv_imgproc",
        "-lpipewire-0.3",
    ],
)

# Shader Program Library (Phase 3 Step 8 - 3D Rendering)
cc_library( # pyright: ignore[reportUndefinedVariable]
    name = "shader_program",
    srcs = ["src/render/shader_program.cpp"],
    hdrs = ["include/render/shader_program.h"],
    includes = [".", "include"],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
    deps = [
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:absl_log",
    ],
    copts = [
        "-I/usr/include",
    ],
    linkopts = ["-lepoxy"],  # libepoxy provides modern OpenGL function loading
)

# Render Manager Library (Phase 3 Refactoring)
cc_library( # pyright: ignore[reportUndefinedVariable]
    name = "render_manager",
    srcs = ["src/render/render_manager.cpp"],
    hdrs = ["include/render/render_manager.h"],
    includes = [".", "include"],
    visibility = ["//visibility:public"],
    deps = [
        ":shader_program",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:model_loader",
        "//mediapipe/framework/port:opencv_core",
        "//third_party/imgui:imgui",
        "@glm//:glm",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:absl_log",
    ],
    copts = [
        "-I/usr/include/SDL2",
        "-I/usr/include/opencv4",
    ],
    linkopts = [
        "-lSDL2",
        "-lGL",
        "-ldl",
    ],
)

# Phase 4 Step 2: FBOManager - Framebuffer Object Management for offscreen rendering
cc_library(
    name = "fbo_manager",
    srcs = ["src/render/fbo_manager.cpp"],
    hdrs = ["include/render/fbo_manager.h"],
    includes = [".", "include"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:absl_log",
    ],
    copts = [
        "-std=c++17",
        "-I/usr/include",
    ],
    linkopts = [
        "-lepoxy",
        "-lGL",
    ],
)

# Profile Management System
cc_library( # type: ignore
    name = "profile_manager",
    srcs = ["src/ui/profile_panel.cpp"],
    hdrs = ["include/profile/profile_manager.h"],
    includes = [".", "include", "src/ui"],
    deps = [
        ":app_state",
        ":camera_manager", 
        ":effects_manager",
        "//mediapipe/examples/desktop/segmecam/src/config:config_manager",
        "//mediapipe/framework/port:opencv_core",
        "//third_party/imgui:imgui",
    ],
    copts = [
        "-I/usr/include/pipewire-0.3",
    ],
)

# UI Management System (Phase 6 Refactoring)
cc_library( # type: ignore
    name = "ui_panels",
    srcs = [
        "src/ui/camera_panel.cpp",
        "src/ui/background_panel.cpp", 
        "src/ui/beauty_panel.cpp",
        "src/ui/ui_utils.cpp",
        "src/ui/virtual_camera_panel.cpp",
        "src/ui/pipewire_output_panel.cpp",
        "src/ui/camera_controls_panel.cpp",
        "src/ui/profile_debug_panels.cpp",
    ],
    hdrs = [
        "include/ui/ui_panels.h",
        "include/ui/ui_panel_base.h",
        "include/ui/ui_utils.h",
        "include/ui/virtual_camera_panel.h",
        "include/ui/pipewire_output_panel.h",
        "include/ui/camera_controls_panel.h",
    ],
    includes = [".", "include"],
    deps = [
        ":app_state",
        ":camera_manager",
        ":effects_manager",
        ":presets",
        ":profile_manager",
        ":segmecam_face_effects",  # for cam_enum.h
        ":vcam",
        ":portal_utils",
        "//mediapipe/examples/desktop/segmecam/src/config:config_manager",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:ar_filter_manager",  # Phase 8 Day 2
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/framework/port:opencv_highgui",
        "//third_party/imgui:imgui",
    ],
    copts = [
        "-I/usr/include/opencv4",
        "-I/usr/include/pipewire-0.3",
    ],
    linkopts = [
        "-lopencv_core",
        "-lopencv_imgproc", 
        "-lopencv_highgui",
    ],
)

cc_library(
    name = "portal_utils",
    srcs = ["src/application/portal_utils.cpp"],
    hdrs = ["include/application/portal_utils.h"],
    includes = [".", "include"],
    deps = [
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgcodecs",
        "@glib//:glib",
        "@libportal//:libportal",
    ],
    copts = [
        "-I/usr/include/opencv4",
    ],
)

cc_library( # type: ignore
    name = "ui_manager_enhanced",
    srcs = ["src/ui/ui_manager_enhanced.cpp"],
    hdrs = ["include/ui/ui_manager_enhanced.h"],
    includes = [".", "include"],
    deps = [
        ":ui_panels",
        "//mediapipe/framework/port:opencv_core",
        "//third_party/imgui:imgui",
    ],
    copts = [
        "-I/usr/include/SDL2",
        "-I/usr/include/opencv4",
        "-I/usr/include/pipewire-0.3",
    ],
    linkopts = [
        "-lSDL2",
        "-lGL",
        "-ldl",
    ],
)

# Effects Manager Library (Phase 5 Refactoring)
cc_library( # type: ignore
    name = "face_regions",
    srcs = ["src/effects/face_regions.cpp"],
    hdrs = ["include/effects/face_regions.h"],
    includes = [".", "include"],
    deps = [
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = ["-I/usr/include/opencv4"],
)

cc_library( # type: ignore
    name = "effects_utils",
    srcs = ["src/effects/effects_utils.cpp"],
    hdrs = ["include/effects/effects_utils.h"],
    includes = [".", "include"],
    deps = [
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = ["-I/usr/include/opencv4"],
)

cc_library( # type: ignore
    name = "lip_effects",
    srcs = ["src/effects/lip_effects.cpp"],
    hdrs = ["include/effects/lip_effects.h"],
    includes = [".", "include"],
    deps = [
        ":effects_utils",
        ":face_regions",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = ["-I/usr/include/opencv4"],
)

cc_library( # type: ignore
    name = "teeth_effects",
    srcs = ["src/effects/teeth_effects.cpp"],
    hdrs = ["include/effects/teeth_effects.h"],
    includes = [".", "include"],
    deps = [
        ":effects_utils",
        ":face_regions",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = ["-I/usr/include/opencv4"],
)

cc_library( # type: ignore
    name = "skin_effects",
    srcs = ["src/effects/skin_effects.cpp"],
    hdrs = ["include/effects/skin_effects.h"],
    includes = [".", "include"],
    deps = [
        ":effects_utils",
        ":face_regions",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = ["-I/usr/include/opencv4"],
)

cc_library( # type: ignore
    name = "advanced_skin_effects",
    srcs = ["src/effects/advanced_skin_effects.cpp"],
    hdrs = ["include/effects/advanced_skin_effects.h"],
    includes = [".", "include"],
    deps = [
        ":face_regions",
        ":skin_effects",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/formats:classification_cc_proto",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = ["-I/usr/include/opencv4"],
)

cc_library( # type: ignore
    name = "wrinkle_segmenter",
    srcs = ["src/effects/wrinkle_segmenter.cpp"],
    hdrs = ["include/effects/wrinkle_segmenter.h"],
    includes = [".", "include"],
    deps = [
        ":face_regions",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = ["-I/usr/include/opencv4", "-I/usr/local/include"],
    linkopts = [
        "-L/usr/local/lib",
        "-lonnxruntime",
        "-Wl,-rpath,/usr/local/lib",
    ],
)

# Face Effects Manager Library (Phase 5 Refactoring)
cc_library( # type: ignore
    name = "effects_manager",
    srcs = ["src/effects/effects_manager.cpp"],
    hdrs = ["include/effects/effects_manager.h"],
    includes = [".", "include"],
    deps = [
        ":background_effects",
        ":performance_monitor",
        ":effects_config",
        ":face_processor",
        ":advanced_skin_effects",
        ":wrinkle_segmenter",
        ":segmecam_composite",
        ":segmecam_face_effects",
        ":presets",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/framework/port:opencv_highgui",
        "//mediapipe/tasks/cc/vision/face_landmarker:face_landmarks_connections",
    ],
    copts = [
        "-I/usr/include/opencv4",
    ],
    linkopts = [
        "-lopencv_core",
        "-lopencv_imgproc",
        "-lopencv_highgui",
        "-lopencv_dnn",
    ],
)

# Background Effects Module
cc_library( # type: ignore
    name = "background_effects",
    srcs = ["src/effects/background/background_effects.cpp"],
    hdrs = ["include/effects/background/background_effects.h"],
    includes = [".", "include"],
    deps = [
        ":segmecam_composite",
        ":segmecam_face_effects",
        ":presets",
        ":app_state",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = ["-I/usr/include/opencv4"],
)

# Performance Monitor Module
cc_library( # type: ignore
    name = "performance_monitor",
    srcs = ["src/effects/performance/performance_monitor.cpp"],
    hdrs = ["include/effects/performance/performance_monitor.h"],
    includes = [".", "include"],
    deps = [
        "//mediapipe/framework/port:opencv_core",
    ],
)

# Effects Configuration Module
cc_library( # type: ignore
    name = "effects_config",
    srcs = ["src/effects/config/effects_config.cpp"],
    hdrs = ["include/effects/config/effects_config.h"],
    includes = [".", "include"],
    deps = [
        ":app_state",
        ":presets",
        "//mediapipe/framework/port:opencv_core",
    ],
)

# Face Processor Module
cc_library( # type: ignore
    name = "face_processor",
    srcs = ["src/effects/face_processor.cpp"],
    hdrs = ["include/effects/face_processor.h"],
    includes = [".", "include", "include/effects"],
    deps = [
        ":app_state",
        ":face_regions",
        ":segmecam_face_effects",
        ":segmecam_composite",
        ":presets",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:face_mesh_processor",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:transform_calculator",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/tasks/cc/vision/face_landmarker:face_landmarks_connections",
    ],
    copts = ["-I/usr/include/opencv4"],
)

cc_binary( # pyright: ignore[reportUndefinedVariable]
    name = "segmecam",
    srcs = [
        "src/application/application.cpp",
        "include/application/application.h",
        "src/application/application_config.cpp",
        "include/application/application_config.h",
        "src/application/gpu_setup.cpp",
        "include/application/gpu_setup.h",
        "src/application/mediapipe_setup.cpp",
        "include/application/mediapipe_setup.h",
        "src/application/camera_setup.cpp",
        "include/application/camera_setup.h",
        "src/application/manager_coordination.cpp",
        "include/application/manager_coordination.h",
        "src/application/application_cleanup.cpp",
        "include/application/application_cleanup.h",
        "src/application/application_run.cpp",
        "include/application/application_run.h",
        "src/application/application_initialization.cpp",
        "include/application/application_initialization.h",
        "src/application/application_sync.cpp",
        "include/application/application_sync.h",
        "src/application/frame_processor.cpp",
        "include/application/frame_processor.h",
        "src/application/mediapipe_processor.cpp",
        "include/application/mediapipe_processor.h",
        "src/application/render_utils.cpp",
        "include/application/render_utils.h",
        "src/application/app_state.cpp",
        "include/application/app_state.h",
        "src/camera/vcam.cpp",
        "include/camera/vcam.h",
    ],
    includes = [".", "include"],
    data = [
        "//mediapipe/modules/face_detection:face_detection_short_range.tflite",
        "//mediapipe/modules/face_landmark:face_landmark_with_attention.tflite", 
        "//mediapipe/modules/selfie_segmentation:selfie_segmentation.tflite",
    ],
    deps = [
        ":gpu_detector",
        ":camera_manager",
        ":render_manager",
        ":effects_manager",
        "//mediapipe/examples/desktop/segmecam/src/config:config_manager",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:blendshape_processor",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:attachment_controller",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:opengl_renderer",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:ar_filter_manager",  # Phase 8: AR filter manager
        "//mediapipe/framework:calculator_graph",
        "//mediapipe/framework/port:file_helpers",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/framework/port:opencv_highgui",
        "//mediapipe/framework/port:parse_text_proto",
        "//mediapipe/framework/port:status",
        "//mediapipe/gpu:gpu_shared_data_internal",
        "//mediapipe/calculators/core:constant_side_packet_calculator",
        "//mediapipe/calculators/core:constant_side_packet_calculator_cc_proto",
        "//mediapipe/calculators/core:flow_limiter_calculator",
        "//mediapipe/calculators/core:split_proto_list_calculator",
        "//mediapipe/gpu:image_frame_to_gpu_buffer_calculator",
        "//mediapipe/gpu:gpu_buffer_to_image_frame_calculator",
        "//mediapipe/graphs/selfie_segmentation:selfie_segmentation_gpu_deps",
        "//mediapipe/modules/selfie_segmentation:selfie_segmentation_gpu",
        "//mediapipe/graphs/face_mesh:desktop_live_gpu_calculators",
        "//mediapipe/modules/face_landmark:face_landmark_front_gpu",
        "//mediapipe/util:resource_util",
        "@com_google_absl//absl/flags:flag",
        "//third_party/imgui:imgui",
        ":ui_manager_enhanced",
        ":portal_utils",
        "@glib//:glib",
        "@libportal//:libportal",
    ],
    copts = [
        "-I/usr/include/SDL2",
        "-I/usr/include/opencv4",
        "-I/usr/include/pipewire-0.3",
    ],
    linkopts = [
        "-lGL",
        "-lGLU",
        "-lSDL2",
        "-lopencv_photo",
    ],
)
