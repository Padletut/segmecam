# GLM (OpenGL Mathematics) - Header-only library for 3D graphics math
# Provides vector, matrix, and quaternion types/operations for OpenGL applications

cc_library(
    name = "glm",
    hdrs = glob([
        "glm/**/*.hpp",
        "glm/**/*.h",
        "glm/**/*.inl",
    ]),
    includes = ["."],
    visibility = ["//visibility:public"],
    # GLM is header-only, no source files needed
)
