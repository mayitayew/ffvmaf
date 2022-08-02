package(default_visibility=["//visibility:public"])

cc_library(
    name = "convolve",
    srcs = ["convolve.c"],
    hdrs = ["convolve.h",],
    deps = [":iqa_options"],
)

cc_library(
    name = "decimate",
    srcs = ["decimate.c"],
    hdrs = ["decimate.h"],
    deps = [":convolve"],
)

cc_library(
    name = "math_utils",
    srcs = ["math_utils.c"],
    hdrs = ["math_utils.h"],
    deps = [":iqa_os"],
)

cc_library(
    name = "iqa",
    hdrs = ["iqa.h"],
    deps = [":iqa_os"],
)

cc_library(
    name = "iqa_os",
    hdrs = ["iqa_os.h"],
)

cc_library(
    name = "iqa_options",
    hdrs = ["iqa_options.h"],
)

cc_library(
    name = "ssim_tools",
    srcs = ["ssim_tools.c"],
    hdrs = ["ssim_tools.h"],
    deps = [":iqa", ":convolve"],
)