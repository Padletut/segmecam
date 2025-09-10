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

cc_binary(
    name = "segmecam_gui_gpu",
    srcs = [
        "segmecam_gui_gpu.cpp",
    ],
    data = [
        "//mediapipe/modules/selfie_segmentation:selfie_segmentation.tflite",
    ],
    deps = [
        ":segmecam_composite",
        "//mediapipe/framework:calculator_graph",
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/port:file_helpers",
        "//mediapipe/framework/port:parse_text_proto",
        "//mediapipe/framework/port:status",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/framework/port:opencv_highgui",
        "//mediapipe/graphs/selfie_segmentation:selfie_segmentation_gpu_deps",
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
    ],
    linkopts = [
        "-lSDL2",
        "-lGL",
        "-ldl",
        "-lpthread",
    ],
    visibility = ["//visibility:public"],
)
