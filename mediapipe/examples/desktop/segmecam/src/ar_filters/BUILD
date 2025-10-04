cc_library(
    name = "blendshape_processor",
    srcs = ["blendshape_processor.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/blendshape_processor.h"],
    includes = ["../.."],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
    deps = [
        "//mediapipe/framework/formats:classification_cc_proto",
        "@com_google_absl//absl/log",
    ],
    copts = [
        "-std=c++17",
    ],
)

cc_library(
    name = "face_mesh_processor",
    srcs = ["face_mesh_processor.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/face_mesh_processor.h"],
    includes = ["../.."],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
    deps = [
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:opencv_core",
        "@com_google_absl//absl/log",
    ],
    copts = [
        "-std=c++17",
    ],
)

cc_library(
    name = "transform_calculator",
    srcs = ["transform_calculator.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/transform_calculator.h"],
    includes = ["../.."],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
    deps = [
        ":face_mesh_processor",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_calib3d",
        "@com_google_absl//absl/log",
    ],
    copts = [
        "-std=c++17",
    ],
)

cc_library(
    name = "model_loader",
    srcs = ["model_loader.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/model_loader.h"],
    includes = ["../.."],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
    deps = [
        "//mediapipe/framework/port:opencv_core",
        "@assimp//:assimp",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/log",
    ],
    copts = [
        "-std=c++17",
        "-DGL_GLEXT_PROTOTYPES",  # Enable OpenGL extension function prototypes
        "-I/usr/include/opencv4",  # For OpenCV headers
    ],
    linkopts = [
        "-lGL",
    ],
)

cc_library(
    name = "filter_object",
    srcs = ["filter_object.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/filter_object.h"],
    includes = ["../.."],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
    deps = [
        ":model_loader",
        "@com_google_absl//absl/log",
    ],
    copts = [
        "-std=c++17",
    ],
)

cc_library(
    name = "attachment_controller",
    srcs = ["attachment_controller.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/attachment_controller.h"],
    includes = ["../.."],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
    deps = [
        ":filter_object",
        ":transform_calculator",
    ],
    copts = [
        "-std=c++17",
    ],
)

cc_library(
    name = "filter_test_demo",
    srcs = ["filter_test_demo.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/filter_test_demo.h"],
    includes = ["../.."],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
    deps = [
        ":filter_object",
        ":attachment_controller",
    ],
    copts = [
        "-std=c++17",
    ],
)

cc_library(
    name = "filter_presets",
    srcs = ["filter_presets.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/filter_presets.h"],
    includes = ["../.."],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
    deps = [
        ":filter_object",
        ":attachment_controller",
    ],
    copts = [
        "-std=c++17",
    ],
)

# Phase 3 Step 8: OpenGL 3D Renderer (Option A - Isolated from MediaPipe)
# This library uses epoxy/gl.h and must never include MediaPipe GL headers
cc_library(
    name = "opengl_renderer",
    srcs = ["opengl_renderer.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/opengl_renderer.h"],
    includes = ["../.."],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
    deps = [
        ":model_loader",
        "//mediapipe/examples/desktop/segmecam:shader_program",
        "@glm//:glm",
        "@com_google_absl//absl/log",
    ],
    copts = [
        "-std=c++17",
        "-I/usr/include",  # For epoxy/gl.h
    ],
    linkopts = [
        "-lepoxy",  # Modern OpenGL function loader
    ],
)

# Phase 4 Step 1: TextureManager - Load and cache textures for 3D models
cc_library(
    name = "texture_manager",
    srcs = ["texture_manager.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/texture_manager.h"],
    includes = ["../.."],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
    deps = [
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgcodecs",
        "//mediapipe/framework/port:opencv_imgproc",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:absl_log",
    ],
    copts = [
        "-std=c++17",
        "-I/usr/include",  # For epoxy/gl.h
        "-I/usr/include/opencv4",  # For OpenCV headers
    ],
    linkopts = [
        "-lepoxy",  # Modern OpenGL function loader
        "-lopencv_core",
        "-lopencv_imgproc",
        "-lopencv_imgcodecs",
    ],
)

# Phase 4 Step 3: ARRenderer - Complete 3D model rendering integration
cc_library(
    name = "ar_renderer",
    srcs = ["ar_renderer.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/ar_renderer.h"],
    includes = ["../.."],
    visibility = ["//visibility:public"],
    deps = [
        ":model_loader",
        ":texture_manager",
        ":filter_asset",  # Phase 6: Filter asset integration
        "//mediapipe/examples/desktop/segmecam:fbo_manager",
        "//mediapipe/examples/desktop/segmecam:shader_program",  # PHASE 5: Shader support
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "@glm//:glm",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:absl_log",
    ],
    copts = [
        "-std=c++17",
        "-I/usr/include",
        "-I/usr/include/opencv4",  # For OpenCV headers
    ],
    linkopts = [
        "-lepoxy",
        "-lGL",
        "-lopencv_core",
        "-lopencv_imgproc",
    ],
)

# Phase 6: FilterAsset - AR filter JSON definition and parsing
cc_library(
    name = "filter_asset",
    srcs = ["filter_asset.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/filter_asset.h"],
    includes = ["../.."],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/strings:str_format",
        "@glm//:glm",
        "@com_github_nlohmann_json//:json",
    ],
    copts = [
        "-std=c++17",
        "-I/usr/include",
    ],
)

# FilterAsset unit tests
cc_binary(
    name = "filter_asset_test",
    srcs = ["filter_asset_test.cpp"],
    includes = ["../.."],
    deps = [
        ":filter_asset",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:absl_log",
    ],
    copts = [
        "-std=c++17",
        "-I/usr/include",
    ],
)

# Phase 6: ARRenderer + FilterAsset integration tests
cc_binary(
    name = "ar_renderer_filter_test",
    srcs = ["ar_renderer_filter_test.cpp"],
    includes = ["../.."],
    deps = [
        ":ar_renderer",
        ":filter_asset",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:absl_log",
    ],
    copts = [
        "-std=c++17",
        "-I/usr/include",
        "-I/usr/include/opencv4",  # For OpenCV headers
    ],
    linkopts = [
        "-lopencv_core",
        "-lopencv_imgproc",
        "-lopencv_imgcodecs",
    ],
)

# Phase 7: AR Filter Manager - Main coordinator for AR filter system
cc_library(
    name = "ar_filter_manager",
    srcs = ["ar_filter_manager.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/ar_filter_manager.h"],
    includes = ["../.."],
    visibility = ["//visibility:public"],
    deps = [
        ":filter_asset",
        ":ar_renderer",
        ":transform_calculator",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/strings:str_format",
        "@glm",
    ],
    copts = [
        "-std=c++17",
        "-I/usr/include/opencv4",
    ],
    linkopts = [
        "-lopencv_core",
        "-lopencv_imgproc",
    ],
)

# Phase 7 Day 3: AR Filter Visual Test Program
cc_binary(
    name = "ar_filter_test_visual",
    srcs = ["ar_filter_test_visual.cpp"],
    includes = ["../.."],
    deps = [
        ":ar_filter_manager",
        ":ar_renderer",
        ":filter_asset",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:absl_log",
    ],
    copts = [
        "-std=c++17",
        "-I/usr/include/opencv4",
    ],
    linkopts = [
        "-lopencv_core",
        "-lopencv_imgproc",
    ],
)
