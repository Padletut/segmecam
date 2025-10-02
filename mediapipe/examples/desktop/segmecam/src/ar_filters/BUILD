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

