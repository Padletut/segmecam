load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

# --- Custom C++ Toolchain registration (minimal example) ---
filegroup(name = "empty")

cc_library(
    name = "args_parser",
    srcs = ["args_parser.cpp"],
    hdrs = ["args_parser.h"],
    includes = ["."],
    deps = [
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:declare",
    ],
)

cc_library(
    name = "mediapipe_manager",
    srcs = ["mediapipe_manager.cpp"],
    hdrs = ["mediapipe_manager.h"],
    includes = ["."],
    deps = [
        "//mediapipe/framework:calculator_graph",
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/port:parse_text_proto",
        "//mediapipe/framework/port:file_helpers",
        "//mediapipe/framework/port:status",
        "//mediapipe/gpu:gpu_shared_data_internal",
    ],
)

cc_library(
    name = "camera_manager",
    srcs = ["camera_manager.cpp"],
    hdrs = ["camera_manager.h"],
    includes = ["."],
    deps = [
        ":segmecam_face_effects",  # for cam_enum.h
        "//mediapipe/framework/port:opencv_core",
    ],
)

cc_library(
    name = "ui_manager",
    srcs = ["ui_manager.cpp"],
    hdrs = ["ui_manager.h"],
    includes = ["."],
    deps = [
        "//mediapipe/framework/port:opencv_core",
        "//third_party/imgui:imgui",
    ],
    copts = [
        "-I/usr/include/SDL2",
    ],
    linkopts = [
        "-lSDL2",
        "-lGL",
    ],
)

cc_library(
    name = "app_state",
    srcs = ["app_state.cpp"],
    hdrs = ["app_state.h"],
    includes = ["."],
    deps = [
        ":vcam",
        "//mediapipe/framework/port:opencv_core",
    ],
)

cc_library(
    name = "app_loop",
    srcs = ["app_loop.cpp"],
    hdrs = ["app_loop.h"],
    includes = ["."],
    deps = [
        ":app_state",
        ":ui_manager",
        ":camera_manager",
        ":mediapipe_manager",
        ":segmecam_composite",
        ":segmecam_face_effects",
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/formats:rect_cc_proto",
        "//third_party/imgui:imgui",
    ],
    copts = [
        "-I/usr/include/SDL2",
    ],
    linkopts = [
        "-lSDL2",
    ],
)

cc_library(
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

cc_library(
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
)

cc_library(
    name = "presets",
    srcs = ["presets.cc"],
    hdrs = ["presets.h"],
    includes = ["."],
    deps = [],
)

cc_library(
    name = "vcam",
    srcs = ["vcam.cc"],
    hdrs = ["vcam.h"],
    includes = ["."],
    deps = [
        "//mediapipe/framework/port:opencv_core",
    ],
)

cc_binary(
    name = "segmecam_gui_gpu",
    srcs = [
        "segmecam_gui_gpu.cpp",
    ],
    data = [
        "//mediapipe/modules/selfie_segmentation:selfie_segmentation.tflite",
        "//mediapipe/modules/face_landmark:face_landmark_with_attention.tflite",
        "//mediapipe/modules/face_detection:face_detection_short_range.tflite",
        "@com_google_mediapipe_face_landmarker_task//file",
    ],
    deps = [
        ":args_parser",
        ":mediapipe_manager", 
        ":camera_manager",
        ":ui_manager",
        ":app_state",
        ":app_loop",
        ":segmecam_composite",
        ":segmecam_face_effects",
        ":presets",
        ":vcam",
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
