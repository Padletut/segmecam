# pugixml (Fast XML Parser) BUILD file
# Used by Assimp for XML-based 3D model formats (COLLADA, GLTF2, etc.)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # MIT License

cc_library(
    name = "pugixml",
    srcs = ["src/pugixml.cpp"],
    hdrs = [
        "src/pugixml.hpp",
        "src/pugiconfig.hpp",
    ],
    includes = ["src"],
    copts = ["-Wno-unused-parameter"],
)
