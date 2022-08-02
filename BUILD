package(default_visibility=["//visibility:public"])

load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")

cc_binary(
    name = "hello",
    srcs = ["hello.cc"],
    deps = ["@ffmpeg//:avutil_lib", "@ffmpeg//:avcodec_lib", "@ffmpeg//:avformat_lib", "@zlib", "//libvmaf:libvmaf"],
)

wasm_cc_binary(
    name = "hello_wasm",
    cc_target = ":hello",
)
