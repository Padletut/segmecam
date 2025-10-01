cc_library(
    name = "blendshape_processor",
    srcs = ["blendshape_processor.cpp"],
    hdrs = ["blendshape_processor.h"],
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
    hdrs = ["face_mesh_processor.h"],
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
    hdrs = ["//mediapipe/examples/desktop/segmecam/include/ar_filters:transform_calculator.h"],
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
