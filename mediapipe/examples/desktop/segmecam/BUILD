load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library") # type: ignore

# SegmeCam Modular Architecture - Phase 1-8 Complete! 🎉
# Note: Test targets may reference removed test files - this is expected after refactoring completion

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
    copts = ["-I/usr/include/opencv4"],
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
    ],
    linkopts = [
        "-lopencv_core",
        "-lopencv_imgproc",
        "-lopencv_highgui",
    ],
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

# Application Foundation - Main Executable (Phase 1 Modular Refactoring)
cc_binary( # pyright: ignore[reportUndefinedVariable]
    name = "application",
    srcs = [
        "src/application/application.cpp",
        "include/application/application.h",
    ],
    includes = [".", "include"],
    deps = [
        # Extracted application modules
        ":application_config",
        ":gpu_setup", 
        ":mediapipe_setup",
        ":camera_setup",
        ":manager_coordination",
        ":application_cleanup",
        ":application_run",
        ":application_initialization",
        # Phase 1-7 managers
        ":camera_manager",
        ":effects_manager", 
        ":render_manager",
        ":ui_manager_enhanced",
        ":mediapipe_manager",
        "//mediapipe/examples/desktop/segmecam/src/config:config_manager",
        # Core dependencies
        ":gpu_detector",
        ":segmecam_face_effects",  # for cam_enum.h
        "//mediapipe/framework:calculator_graph", 
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/port:file_helpers",
        "//mediapipe/framework/port:parse_text_proto",
        "//mediapipe/framework/port:status",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_highgui",
        "//mediapipe/gpu:gpu_shared_data_internal",
        # Required calculators for selfie segmentation
        "//mediapipe/graphs/selfie_segmentation:selfie_segmentation_gpu_deps",
        "//mediapipe/graphs/selfie_segmentation:selfie_segmentation_cpu_deps",
        "//mediapipe/calculators/util:to_image_calculator",
        "//mediapipe/calculators/core:flow_limiter_calculator",
        "//mediapipe/gpu:gl_calculator_helper",
        "//mediapipe/gpu:gpu_buffer",
        "//mediapipe/gpu:gpu_buffer_to_image_frame_calculator",
        "//mediapipe/gpu:image_frame_to_gpu_buffer_calculator",
        "@com_google_absl//absl/flags:flag",
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

cc_binary( # pyright: ignore[reportUndefinedVariable]
    name = "segmecam_gui_gpu",
    srcs = [
        "segmecam_gui_gpu.cpp",
    ],
    data = [
        "//mediapipe/modules/selfie_segmentation:selfie_segmentation.tflite",
        "//mediapipe/modules/face_landmark:face_landmark_with_attention.tflite",
        "//mediapipe/modules/face_detection:face_detection_short_range.tflite",
    ],
    deps = [
        ":segmecam_composite",
        ":segmecam_face_effects",
        ":presets",
        ":vcam",
        ":gpu_detector",
        "//mediapipe/framework:calculator_graph",
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/formats:rect_cc_proto",
        "//mediapipe/framework/port:file_helpers",
        "//mediapipe/framework/port:parse_text_proto",
        "//mediapipe/framework/port:status",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/framework/port:opencv_highgui",
        "//mediapipe/graphs/selfie_segmentation:selfie_segmentation_gpu_deps",
        "//mediapipe/graphs/face_mesh:desktop_live_gpu_calculators",
        # Tasks Face Landmarker graph + options protos
        "//mediapipe/tasks/cc/vision/face_landmarker:face_landmarker_graph",
        "//mediapipe/tasks/cc/vision/face_landmarker:face_landmarks_connections",
        "//mediapipe/tasks/cc/vision/face_landmarker/proto:face_landmarker_graph_options_cc_proto",
        "//mediapipe/tasks/cc/vision/face_landmarker/proto:face_landmarks_detector_graph_options_cc_proto",
        "//mediapipe/tasks/cc/vision/face_detector/proto:face_detector_graph_options_cc_proto",
        # Needed calculators referenced by our graphs
        "//mediapipe/calculators/util:to_image_calculator",
        "//mediapipe/gpu:gl_calculator_helper",
        "//mediapipe/gpu:gpu_buffer",
        "//mediapipe/gpu:gpu_shared_data_internal",
        "//mediapipe/gpu:gpu_buffer_to_image_frame_calculator",
        "//mediapipe/gpu:image_frame_to_gpu_buffer_calculator",
        "//third_party/imgui:imgui",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
    ],
    copts = [
        "-DABSL_USES_STD_ANY=1",
        "-I/usr/include/opencv4",
        "-I/usr/include/SDL2",
        "-I/usr/lib/gcc/x86_64-linux-gnu/13/include",
    ],
    linkopts = [
        "-lSDL2",
        "-lGL",
        "-ldl",
        "-lpthread",
    ],
    visibility = ["//visibility:public"],
)

# Test binary for Phase 1 Application Foundation 
cc_binary( # type: ignore
    name = "segmecam_gui_gpu_test",
    srcs = [
        "segmecam_gui_gpu_test.cpp",
    ],
    deps = [
        ":application",
    ],
    visibility = ["//visibility:public"],
)

# Test binary for Phase 2 MediaPipe Manager
cc_binary( # pyright: ignore[reportUndefinedVariable]
    name = "mediapipe_manager_test",
    srcs = [
        "mediapipe_manager_test.cpp",
    ],
    deps = [
        ":application",
        ":mediapipe_manager",
    ],
    visibility = ["//visibility:public"],
)

# Test binary for Phase 3 Render Manager
cc_binary( # pyright: ignore[reportUndefinedVariable]
    name = "render_manager_test",
    srcs = [
        "render_manager_test.cpp",
    ],
    deps = [
        ":render_manager",
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
        "-lopencv_core",
        "-lopencv_imgproc",
    ],
    visibility = ["//visibility:public"],
)

# Test binary for Phase 4 Camera Manager
cc_binary( # pyright: ignore[reportUndefinedVariable]
    name = "camera_manager_test",
    srcs = [
        "camera_manager_test.cpp",
    ],
    deps = [
        ":camera_manager",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_highgui",
    ],
    copts = [
        "-I/usr/include/opencv4",
    ],
    linkopts = [
        "-lopencv_core",
        "-lopencv_imgproc",
        "-lopencv_highgui",
    ],
    visibility = ["//visibility:public"],
)

# Test binary for Phase 6 UI Management
cc_binary( # pyright: ignore[reportUndefinedVariable]
    name = "ui_manager_test",
    srcs = [
        "ui_manager_test.cpp",
    ],
    deps = [
        ":ui_manager_enhanced",
        ":ui_panels",
        ":app_state",
        ":camera_manager",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/framework/port:opencv_highgui",
        "//third_party/imgui:imgui",
    ],
    copts = [
        "-DABSL_USES_STD_ANY=1",
        "-I/usr/include/SDL2",
        "-I/usr/include/opencv4",
        "-I/usr/lib/gcc/x86_64-linux-gnu/13/include",
    ],
    linkopts = [
        "-lSDL2",
        "-lGL",
        "-ldl",
        "-lpthread",
        "-lopencv_core",
        "-lopencv_imgproc",
        "-lopencv_highgui",
    ],
    visibility = ["//visibility:public"],
)

# Test binary for Phase 5 Effects Manager
cc_binary( # pyright: ignore[reportUndefinedVariable]
    name = "effects_manager_test",
    srcs = ["effects_manager_test.cpp"],
    deps = [
        ":effects_manager",
        ":app_state",
        "@mediapipe//mediapipe/framework:calculator_framework",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
)

# Test binary for Phase 7 Configuration Manager
cc_binary( # pyright: ignore[reportUndefinedVariable]
    name = "config_manager_test",
    srcs = ["config_manager_test.cpp"],
    deps = [
        "//mediapipe/examples/desktop/segmecam/src/config:config_manager",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_highgui",
    ],
    copts = [
        "-std=c++17",
        "-Wno-sign-compare",
        "-I/usr/include/opencv4",
    ],
    linkopts = [
        "-lopencv_core",
        "-lopencv_imgproc",
    ],
)

# Phase 8: Integrated Application using all managers
cc_binary( # pyright: ignore[reportUndefinedVariable]
    name = "segmecam_integrated_simple",
    srcs = ["segmecam_integrated_simple.cpp"],
    deps = [
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
    ],
    copts = [
        "-I/usr/include/SDL2",
    ],
    linkopts = [
        "-lGL",
        "-lGLU", 
        "-lSDL2",
    ],
)

# Phase 8: Simple Application Test (without MediaPipe dependencies)
cc_binary( # pyright: ignore[reportUndefinedVariable]
    name = "application_simple_test",
    srcs = ["application_simple_test.cpp"],
    deps = [
        ":camera_manager", 
        ":render_manager",
        "//mediapipe/examples/desktop/segmecam/src/config:config_manager",
    ],
    copts = [
        "-I/usr/include/SDL2",
        "-I/usr/include/opencv4",
    ],
    linkopts = [
        "-lGL",
        "-lGLU", 
        "-lSDL2",
        "-lopencv_core",
        "-lopencv_imgproc",
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