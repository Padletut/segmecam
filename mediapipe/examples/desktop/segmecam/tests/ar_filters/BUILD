# Copyright 2025 SegmeCam Contributors
# SPDX-License-Identifier: Apache-2.0
#
# AR Filters Unit Tests - Phase 3 Step 6

# ModelLoader Tests
cc_test(
    name = "model_loader_test",
    size = "medium",
    srcs = ["model_loader_test.cpp"],
    copts = [
        "-std=c++17",
    ],
    data = [
        "//assets/ar_filters/models:simple_cube.obj",
        "//assets/ar_filters/models:simple_cube.mtl",
        "//assets/ar_filters/models:simple_glasses.obj",
        "//assets/ar_filters/models:simple_glasses.mtl",
        "//assets/ar_filters/models:simple_hat.obj",
        "//assets/ar_filters/models:simple_hat.mtl",
    ],
    includes = ["../.."],
    deps = [
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:model_loader",
        "@com_google_googletest//:gtest_main",
    ],
)

# FilterObject Tests
cc_test(
    name = "filter_object_test",
    size = "medium",
    srcs = ["filter_object_test.cpp"],
    copts = [
        "-std=c++17",
    ],
    data = [
        "//assets/ar_filters/models:simple_cube.obj",
        "//assets/ar_filters/models:simple_cube.mtl",
        "//assets/ar_filters/models:simple_glasses.obj",
        "//assets/ar_filters/models:simple_glasses.mtl",
        "//assets/ar_filters/models:simple_hat.obj",
        "//assets/ar_filters/models:simple_hat.mtl",
    ],
    includes = ["../.."],
    deps = [
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:filter_object",
        "@com_google_googletest//:gtest_main",
    ],
)

# AttachmentController Tests
cc_test(
    name = "attachment_controller_test",
    size = "medium",
    srcs = ["attachment_controller_test.cpp"],
    copts = [
        "-std=c++17",
    ],
    data = [
        "//assets/ar_filters/models:simple_cube.obj",
        "//assets/ar_filters/models:simple_cube.mtl",
        "//assets/ar_filters/models:simple_glasses.obj",
        "//assets/ar_filters/models:simple_glasses.mtl",
    ],
    includes = ["../.."],
    deps = [
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:attachment_controller",
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:filter_object",
        "@com_google_googletest//:gtest_main",
    ],
)

# Phase 4 Step 1: TextureManager Test - Verify texture loading functionality
cc_binary(
    name = "texture_manager_test",
    srcs = ["texture_manager_test.cpp"],
    includes = ["../.."],
    deps = [
        "//mediapipe/examples/desktop/segmecam/src/ar_filters:texture_manager",
        "@com_google_absl//absl/log",
    ],
    copts = [
        "-std=c++17",
        "-I/usr/include/SDL2",
    ],
    linkopts = [
        "-lSDL2",
        "-lepoxy",
    ],
)

# Test suite to run all tests
test_suite(
    name = "ar_filters_tests",
    tests = [
        ":model_loader_test",
        ":filter_object_test",
        ":attachment_controller_test",
        # Note: texture_manager_test is a binary, not a test
    ],
)
