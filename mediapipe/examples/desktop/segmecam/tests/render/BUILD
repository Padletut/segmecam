# BUILD file for render tests
load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "fbo_manager_test",
    srcs = ["fbo_manager_test.cpp"],
    deps = [
        "//mediapipe/examples/desktop/segmecam:fbo_manager",
    ],
    copts = [
        "-I/usr/include",
    ],
    linkopts = [
        "-lSDL2",
        "-lepoxy",
        "-lGL",
    ],
    visibility = ["//visibility:public"],
)