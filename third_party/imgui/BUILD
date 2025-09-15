load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "imgui",
    srcs = [
        "imgui.cpp",
        "imgui_draw.cpp",
        "imgui_tables.cpp",
        "imgui_widgets.cpp",
        "backends/imgui_impl_sdl2.cpp",
        "backends/imgui_impl_opengl3.cpp",
    ],
    hdrs = [
        "imgui.h",
        "imconfig.h",
        "imgui_internal.h",
        "imstb_rectpack.h",
        "imstb_textedit.h",
        "imstb_truetype.h",
        "backends/imgui_impl_sdl2.h",
        "backends/imgui_impl_opengl3.h",
        "backends/imgui_impl_opengl3_loader.h",
    ],
    includes = ["."],
    copts = [
        "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS",
        # Ensure SDL2 headers are found (common distro path)
        "-I/usr/include/SDL2",
        "-D_REENTRANT",
    ],
    visibility = ["//visibility:public"],
)
