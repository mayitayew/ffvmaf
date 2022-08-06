package(default_visibility=["//visibility:public"])

load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")


cc_library(
    name = "cpu",
    srcs = ["cpu.c"],
    hdrs = ["cpu.h"],
)

cc_library(
    name = "dict",
    srcs = ["dict.c"],
    hdrs = ["dict.h"],
    deps = [":feature"],
)

cc_library(
    name = "feature",
    hdrs = ["feature.h"],
)

cc_library(
    name = "feature_name",
    srcs = ["feature_name.c"],
    hdrs = ["feature_name.h"],
    deps = ["//libvmaf/feature:alias",
    ":dict",
    ":opt"],
)

cc_library(
    name = "feature_collector",
    srcs = ["feature_collector.c"],
    hdrs = ["feature_collector.h"],
    deps = [":feature_name",
    ":dict",
    ":log"],
)


cc_library(
    name = "fex_ctx_vector",
    srcs = ["fex_ctx_vector.c"],
    hdrs = ["fex_ctx_vector.h"],
    deps = [":feature_name",
    ":feature_extractor",
    ":log"],
)

cc_library(
    name = "feature_extractor",
    srcs = ["feature_extractor.c", "null.c", "cambi.c", "integer_vif.c", "float_ssim.c", "integer_ssim.c",
    "ciede.c", "integer_motion.c", "float_ms_ssim.c",
    "integer_adm.c",
    "integer_psnr.c", "psnr_hvs.c",],
    hdrs = ["feature_extractor.h"],
    deps = [":feature_name",
    ":feature_collector",
    "//libvmaf/feature:picture_copy",
    ":ms_ssim",
    ":dict",
    ":integer_adm_header",
    ":integer_motion_header",
    ":integer_vif_header",
    "//libvmaf/common:macros",
    "//libvmaf/common:alignment",
    ":cpu",
    ":picture_interface",
    ":opt",
    ":ssim",
    "//libvmaf/feature:mkdirp",
    ":luminance_tools",
    ":picture",
    ":log"],
)

cc_binary(
    name = "hello",
    srcs = ["hello.cc"],
    deps = [":runfiles_util", "@com_github_mjbots_bazel_deps//:everything"],
    data = ["//libvmaf/model:720p.mp4", "//libvmaf/model:ref.yuv"],
    #linkopts = ["-lavcodec", "-lavformat", "-lavutil",],
)

cc_library(
    name = "integer_adm_header",
    hdrs = ["integer_adm.h"],
)


cc_library(
    name = "integer_motion_header",
    hdrs = ["integer_motion.h"],
)

cc_library(
    name = "integer_vif_header",
    hdrs = ["integer_vif.h"],
)

cc_library(
    name = "libvmaf",
    hdrs = ["libvmaf.h"],
    srcs = ["libvmaf.c"],
    deps = [":cpu", ":feature", ":feature_extractor",
    ":feature_collector", ":fex_ctx_vector",
    ":model", ":log", ":picture", ":predict", ":thread_pool", ":output"],
)

WASM_LINKOPTS = [
 "--bind",
 "-sEXPORT_ALL=1",
 "-sFETCH",
 "-sEXPORT_ES6=1",
 "-sMODULARIZE=1",
 "-sPTHREAD_POOL_SIZE=12",
 "-sPROXY_TO_PTHREAD",
 "-sUSE_PTHREADS",
 "-sALLOW_MEMORY_GROWTH=1",
 "-sNO_EXIT_RUNTIME=1",
]

cc_binary(
    name = "libvmaf_wasm_wrapper",
    srcs = ["libvmaf_wasm.cc"],
    linkopts = WASM_LINKOPTS,
    deps = [":libvmaf", ":model_buffer",],
)

wasm_cc_binary(
    name = "libvmaf_wasm",
    cc_target = ":libvmaf_wasm_wrapper",
    simd = True,
)

cc_library(
    name = "libvmaf_header",
    hdrs = ["libvmaf.h"],
    deps = [":model_header", ":feature", ":picture_interface",],
)

cc_library(
    name = "libvmaf_av",
    hdrs = ["libvmaf_av.h"],
    srcs = ["libvmaf_av.cc"],
    deps = [":libvmaf", "@ffmpeg//:avutil_lib", "@ffmpeg//:avcodec_lib", "@ffmpeg//:avformat_lib", "@zlib",],
)


cc_library(
    name = "log",
    srcs = ["log.c"],
    hdrs = ["log.h"],
    deps = [":libvmaf_header"],
)

cc_library(
    name = "luminance_tools",
    srcs = ["luminance_tools.c"],
    hdrs = ["luminance_tools.h"],
    deps = [":log"],
)

cc_library(
    name = "mem",
    srcs = ["mem.c"],
    hdrs = ["mem.h"],
)

cc_library(
    name = "ms_ssim",
    hdrs = ["ms_ssim.h"],
    srcs = ["ms_ssim.c"],
    deps = [":mem", "//libvmaf/iqa:decimate", "//libvmaf/iqa:math_utils", "//libvmaf/iqa:ssim_tools",],
)

cc_library(
    name = "model_buffer",
    hdrs = ["model_buffer.h"],
)

cc_library(
    name = "model_header",
    hdrs = ["model.h"],
    deps = [":model_interface", ":dict"],
)

# Was originally libvmaf/model.h
cc_library(
    name = "model_interface",
    hdrs = ["model_interface.h"],
)

cc_library(
    name = "model",
    hdrs = ["model.h"],
    srcs = ["model.c"],
    deps = [":model_interface",
        ":feature_extractor",
        ":log",
        ":runfiles_util",
        ":read_json_model",
        ":svm",
    ],
)

cc_library(
    name = "opt",
    hdrs = ["opt.h"],
    srcs = ["opt.c"],
)

cc_library(
    name = "output",
    hdrs = ["output.h"],
    srcs = ["output.c"],
    deps = [":feature_collector", "//libvmaf/feature:alias", ":libvmaf_header"]
)

cc_library(
    name = "picture",
    hdrs = ["picture.h"],
    srcs = ["picture.c"],
    deps = [":mem",
    ":picture_interface",
    ":ref"],
)

cc_library(
    name = "picture_interface",
    hdrs = ["picture_interface.h"],
)

cc_library(
    name = "predict",
    hdrs = ["predict.h"],
    srcs = ["predict.c"],
    deps = [":mem",
    ":model",
    ":dict",
    "//libvmaf/feature:alias",
    ":feature_extractor",
    ":feature_collector",
    ":feature_name",
    ":log",
    ":svm",
    ":ref"],
)

cc_library(
    name = "pdjson",
    hdrs = ["pdjson.h"],
    srcs = ["pdjson.c"],
)

cc_library(
    name = "read_json_model",
    hdrs = ["read_json_model.h"],
    srcs = ["read_json_model.c"],
    deps = [
        ":dict",
        ":model_interface",
        ":pdjson",
        ":svm",
        ":feature",
        ":model_header",
    ]
)

cc_library(
    name = "ref",
    srcs = ["ref.c"],
    hdrs = ["ref.h"],
)

cc_library(
    name = "runfiles_util",
    hdrs = ["runfiles_util.h"],
    deps = ["@bazel_tools//tools/cpp/runfiles"],
)

cc_library(
    name = "spinner",
    hdrs = ["spinner.h"],
)

cc_library(
    name = "ssim",
    srcs = ["ssim.c"],
    hdrs = ["ssim.h"],
    deps = ["//libvmaf/iqa:math_utils", "//libvmaf/iqa:decimate", "//libvmaf/iqa:ssim_tools", ":mem"],
)

cc_library(
    name = "svm",
    hdrs = ["svm.h"],
    srcs = ["svm.cpp"],
)

cc_library(
    name = "thread_pool",
    srcs = ["thread_pool.c"],
    hdrs = ["thread_pool.h"],
)