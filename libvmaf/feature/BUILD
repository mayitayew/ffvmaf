package(default_visibility=["//visibility:public"])


cc_library(
    name = "alias",
    srcs = ["alias.c"],
    hdrs = ["alias.h"],
)

cc_library(
    name = "mkdirp",
    srcs = ["mkdirp.c"],
    hdrs = ["mkdirp.h"],
)

cc_library(
    name = "ssim_header",
    hdrs = ["ssim.h"],
)

cc_library(
    name = "picture_copy",
    hdrs = ["picture_copy.h"],
    srcs = ["picture_copy.c"],
    deps = [":picture_interface"],
)

cc_library(
    name = "picture_interface",
    hdrs = ["picture_interface.h"],
)