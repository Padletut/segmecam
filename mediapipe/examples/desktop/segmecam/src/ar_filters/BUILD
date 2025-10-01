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
