package(default_visibility=["//visibility:public"])

load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")

FFMPEG_DEPS = [
    "@ffmpeg//:avutil_lib",
    "@ffmpeg//:avcodec_lib",
    "@ffmpeg//:avformat_lib",
    "@zlib",
    "@boringssl//:ssl",
]

cc_binary(
    name = "hello",
    srcs = ["hello.cc"],
    deps = ["//libvmaf:libvmaf", ":ffvmaf_lib"] + FFMPEG_DEPS,
)

wasm_cc_binary(
    name = "hello_wasm",
    cc_target = ":hello",
)

WASM_LINKOPTS = [
 "--bind",
 "-sFETCH",
 "-sEXPORT_ES6=1",
 "-sMODULARIZE=1",
 "-sEXPORT_ALL=1",
 "-sALLOW_MEMORY_GROWTH=1",
 "-sNO_EXIT_RUNTIME=1",
 "-sENVIRONMENT='web,worker'", # Exclude node environment. The frontend react app does not support it.
]

cc_library(
    name = "ffvmaf_lib",
    srcs = ["ffvmaf_lib.cc"],
    hdrs = ["ffvmaf_lib.h"],
    deps = ["//libvmaf:libvmaf"] + FFMPEG_DEPS,
)

cc_test(
    name = "ffvmaf_lib_test",
    srcs = ["ffvmaf_lib_test.cc"],
    deps = [":ffvmaf_lib", "@com_google_googletest//:gtest_main"],
)

cc_binary(
    name = "ffvmaf_wasm_lib",
    srcs = ["ffvmaf_wasm.cc"],
    deps = ["//libvmaf:libvmaf", "//libvmaf:model_buffer", ":ffvmaf_lib"],
    linkopts = WASM_LINKOPTS,
)

wasm_cc_binary(
    name = "ffvmaf_wasm",
    cc_target = ":ffvmaf_wasm_lib",
)