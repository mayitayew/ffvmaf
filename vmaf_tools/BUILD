package(default_visibility=["//visibility:public"])


cc_library(
    name = "cli_parse",
    srcs = ["cli_parse.c"],
    hdrs = ["cli_parse.h"],
    deps = ["//libvmaf:libvmaf_header", ":vidinput", "//libvmaf:model", "//libvmaf:feature"],
)

cc_library(
    name = "spinner",
    hdrs = ["spinner.h"],
)

cc_library(
    name = "vidinput",
    hdrs = ["vidinput.h"],
    srcs = ["vidinput.c", "yuv_input.c", "y4m_input.c"],
    deps = ["//libvmaf:libvmaf_header"],
)

cc_binary(
    name = "vmaf",
    srcs = ["vmaf.c"],
    deps = ["//libvmaf:picture", "//libvmaf:libvmaf", ":cli_parse", ":spinner", ":vidinput"],
)