workspace(name = "ffvmaf")
# FFmpeg
load("//tools/workspace:default.bzl", "add_default_repositories")

add_default_repositories()

load("@com_github_mjbots_bazel_deps//tools/workspace:default.bzl",
     bazel_deps_add = "add_default_repositories")
bazel_deps_add()

# Webassembly bazel.
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
http_archive(
    name = "emsdk",
    sha256 = "",
    strip_prefix = "emsdk-8281708351554b8d1eda9dafdca851b0b9be1eeb/bazel",
    url = "https://github.com/emscripten-core/emsdk/archive/8281708351554b8d1eda9dafdca851b0b9be1eeb.tar.gz",
)
load("@emsdk//:deps.bzl", emsdk_deps = "deps")
emsdk_deps()

load("@emsdk//:emscripten_deps.bzl", emsdk_emscripten_deps = "emscripten_deps")
emsdk_emscripten_deps(emscripten_version = "3.1.15")


load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
# GoogleTest/GoogleMock framework.
git_repository(
    name = "com_google_googletest",
    remote = "https://github.com/google/googletest.git",
    tag = "release-1.10.0",
)

# BoringSSL which replaces OpenSSL in FFmpeg.
git_repository(
    name = "boringssl",
    commit = "348be81e950f86636cd51a8433510b64fd9064b7",
    remote = "https://boringssl.googlesource.com/boringssl",
)