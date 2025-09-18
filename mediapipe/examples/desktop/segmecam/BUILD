load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library") # type: ignore

cc_library( # type: ignore
    name = "segmecam_composite",
    srcs = ["segmecam_composite.cc"],
    hdrs = ["segmecam_composite.h"],
    includes = ["."],
    deps = [
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
)

cc_library( # type: ignore
    name = "segmecam_face_effects",
    srcs = ["segmecam_face_effects.cc", "cam_enum.cc"],
    hdrs = ["segmecam_face_effects.h", "cam_enum.h"],
    includes = ["."],
    deps = [
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/framework/port:opencv_highgui",
    ],
    copts = ["-I/usr/include/opencv4"],
    linkopts = ["-lopencv_core", "-lopencv_imgproc", "-lopencv_highgui"],
)

cc_library( # type: ignore
    name = "presets",
    srcs = ["presets.cc"],
    hdrs = ["presets.h"],
    includes = ["."],
    deps = [],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
)

cc_library( # type: ignore
    name = "vcam",
    srcs = ["vcam.cc"],
    hdrs = ["vcam.h"],
    includes = ["."],
    deps = [
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = ["-I/usr/include/opencv4"],
    linkopts = ["-lopencv_core", "-lopencv_imgproc"],
)

cc_library( # type: ignore
    name = "app_state",
    srcs = ["app_state.cpp"],
    hdrs = ["app_state.h"],
    includes = ["."],
    deps = [
        ":vcam",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
    copts = ["-I/usr/include/opencv4"],
    linkopts = ["-lopencv_core", "-lopencv_imgproc"],
)

cc_library( # type: ignore
    name = "gpu_detector",
    srcs = ["gpu_detector.cpp"],
    hdrs = ["gpu_detector.h"],
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
        ":camera_manager",
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
    name = "camera_manager",
    srcs = ["src/camera/camera_manager.cpp"],
    hdrs = ["include/camera/camera_manager.h"],
    includes = [".", "include"],
    deps = [
        ":segmecam_face_effects",  # for cam_enum.h
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_highgui",
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

# Render Manager Library (Phase 3 Refactoring)
cc_library( # pyright: ignore[reportUndefinedVariable]
    name = "render_manager",
    srcs = ["src/render/render_manager.cpp"],
    hdrs = ["include/render/render_manager.h"],
    includes = [".", "include"],
    deps = [
        "//mediapipe/framework/port:opencv_core",
        "//third_party/imgui:imgui",
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

# UI Management System (Phase 6 Refactoring)
cc_library( # type: ignore
    name = "ui_panels",
    srcs = [
        "src/ui/camera_panel.cpp",
        "src/ui/background_panel.cpp", 
        "src/ui/beauty_panel.cpp",
        "src/ui/profile_debug_panels.cpp",
    ],
    hdrs = ["include/ui/ui_panels.h"],
    includes = [".", "include"],
    deps = [
        ":app_state",
        ":camera_manager",
        ":effects_manager",
        ":presets",
        ":segmecam_face_effects",  # for cam_enum.h
        ":vcam",
        "//mediapipe/examples/desktop/segmecam/src/config:config_manager",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/framework/port:opencv_highgui",
        "//third_party/imgui:imgui",
    ],
    copts = [
        "-I/usr/include/opencv4",
    ],
    linkopts = [
        "-lopencv_core",
        "-lopencv_imgproc", 
        "-lopencv_highgui",
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
    ],
    linkopts = [
        "-lSDL2",
        "-lGL",
        "-ldl",
    ],
)

# Effects Manager Library (Phase 5 Refactoring)
cc_library( # type: ignore
    name = "effects_manager",
    srcs = ["src/effects/effects_manager.cpp"],
    hdrs = ["include/effects/effects_manager.h"],
    includes = [".", "include"],
    deps = [
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
    ],
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
        "app_state.cpp",
        "app_state.h",
        "vcam.cc",
        "vcam.h",
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
    ],
    copts = [
        "-I/usr/include/SDL2",
        "-I/usr/include/opencv4",
    ],
    linkopts = [
        "-lGL",
        "-lGLU",
        "-lSDL2",
    ],
)