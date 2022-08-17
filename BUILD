package(default_visibility=["//visibility:public"])

load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")

FFMPEG_DEPS = [
    "@ffmpeg//:avutil_lib",
    "@ffmpeg//:avcodec_lib",
    "@ffmpeg//:avformat_lib",
    "@zlib",
    "@bzip2",
    "@boringssl//:ssl",
]

SYSTEM_FFMPEG_LINKOPTS = [
    "-lavcodec",
    "-lavformat",
    "-lavutil",
    "-lswresample",
    "-lswscale",
]

cc_binary(
    name = "basic_from_buffer",
    srcs = ["basic_from_buffer.cc"],
    data = ["//libvmaf/model:720p.mp4", "//libvmaf/model:bbb_trailer.webm", "//libvmaf/model:sample.mp4"],
    deps = ["//libvmaf/src:libvmaf", ":ffvmaf_lib"],
    linkopts = SYSTEM_FFMPEG_LINKOPTS,
)

wasm_cc_binary(
    name = "basic_from_buffer_wasm",
    cc_target = ":basic_from_buffer",
)

cc_binary(
    name = "basic_from_file",
    srcs = ["basic_from_file.cc"],
    data = ["//libvmaf/model:mux_modified.mp4", "//libvmaf/model:mux.mp4", "//libvmaf/model:vmaf_v0.6.1neg.json"],
    deps = ["//libvmaf/src:libvmaf", ":ffvmaf_lib",] + FFMPEG_DEPS,
)

cc_binary(
    name = "basic",
    srcs = ["basic.cc"],
    data = ["//libvmaf/model:720p.mp4", "//libvmaf/model:mux.mp4", "//libvmaf/model:bbb_trailer.webm", "//libvmaf/model:vmaf_v0.6.1neg.json"],
    deps = ["//libvmaf/src:libvmaf", ":ffvmaf_lib",] + FFMPEG_DEPS,
)

wasm_cc_binary(
    name = "basic_from_file_wasm",
    cc_target = ":basic_from_file",
)

WASM_LINKOPTS = [
 "--bind",
 "-sFETCH",
# "-sEXPORT_ES6=1",
# "-sMODULARIZE=1",
  "-sEXPORT_ALL=1",
  "-sPTHREAD_POOL_SIZE='navigator.hardwareConcurrency'",
 "-sALLOW_MEMORY_GROWTH=1",
 "-sNO_EXIT_RUNTIME=1",
 "-lworkerfs.js",  # To use WorkerFS filesystem
 "-sERROR_ON_UNDEFINED_SYMBOLS=0",
 "-sENVIRONMENT='web,worker'", # Exclude node environment. The frontend react app does not support it.
]

cc_library(
    name = "ffvmaf_lib",
    srcs = ["ffvmaf_lib.cc"],
    hdrs = ["ffvmaf_lib.h"],
    deps = ["//libvmaf/src:libvmaf"] + FFMPEG_DEPS,
)

cc_test(
    name = "ffvmaf_lib_test",
    srcs = ["ffvmaf_lib_test.cc"],
    deps = [":ffvmaf_lib", "@com_google_googletest//:gtest_main"],
)

cc_binary(
    name = "ffvmaf_wasm_lib",
    srcs = ["ffvmaf_wasm.cc"],
    deps = ["//libvmaf/src:libvmaf", "//libvmaf/src:model_buffer", ":ffvmaf_lib"],
    linkopts = WASM_LINKOPTS,
)

wasm_cc_binary(
    name = "ffvmaf_wasm",
    cc_target = ":ffvmaf_wasm_lib",
)