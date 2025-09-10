# Bazel WORKSPACE file for SegmeCam
workspace(name = "segmecam")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Provide @rules_proto for MediaPipe's proto rules when building from this repo.
http_archive(
    name = "rules_proto",
    urls = ["https://github.com/bazelbuild/rules_proto/releases/download/5.3.0/rules_proto-5.3.0.tar.gz"],
)

# Stub rules_proto_grpc for JS defs required during load.
local_repository(
    name = "rules_proto_grpc",
    path = "third_party/rules_proto_grpc",
)

# Local MediaPipe checkout (required for segmecam_mp_cpu Bazel build)
local_repository(
    name = "mediapipe",
    path = "external/mediapipe",
)

# Stub @npm repository to bypass NodeJS requirements of MediaPipe's TS rules
local_repository(
    name = "npm",
    path = "third_party/npm",
)

# TODO: Add TensorFlow Lite external if you want C++ API via Bazel. Options:
# 1) Pull full TensorFlow and use //tensorflow/lite:tflite (heavy).
# 2) Create a local_repository pointing to a prebuilt TFLite C++ library.
# 3) Use the C API prebuilt (libtensorflowlite_c.so) and wrap it.

# Example (commented) for TensorFlow full repo (large, slow to fetch):
# http_archive(
#     name = "org_tensorflow",
#     urls = ["https://github.com/tensorflow/tensorflow/archive/refs/tags/v2.16.1.tar.gz"],
#     strip_prefix = "tensorflow-2.16.1",
# )

# TODO: Add external dependencies (e.g., rules_cc, rules_qt, rules_imgui, etc.)
