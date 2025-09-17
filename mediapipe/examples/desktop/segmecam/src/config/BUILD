cc_library(
    name = "config_manager",
    srcs = ["config_manager.cpp"],
    hdrs = ["config_manager.h"],
    visibility = ["//mediapipe/examples/desktop/segmecam:__subpackages__"],
    deps = [
        "//mediapipe/examples/desktop/segmecam:presets",
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