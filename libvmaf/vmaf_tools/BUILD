package(default_visibility=["//visibility:public"])


cc_library(
    name = "cli_parse",
    srcs = ["cli_parse.c"],
    hdrs = ["cli_parse.h"],
    deps = ["//libvmaf/src:libvmaf_header", ":vidinput", "//libvmaf/src:model", "//libvmaf/src:feature"],
)

cc_library(
    name = "spinner",
    hdrs = ["spinner.h"],
)

cc_library(
    name = "vidinput",
    hdrs = ["vidinput.h"],
    srcs = ["vidinput.c", "yuv_input.c", "y4m_input.c"],
    deps = ["//libvmaf/src:libvmaf_header"],
)

cc_binary(
    name = "vmaf",
    srcs = ["vmaf.c"],
    deps = ["//libvmaf/src:picture", "//libvmaf/src:libvmaf", ":cli_parse", ":spinner", ":vidinput"],
)