load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")


def bazel_deps_repository():
    commit = "2e85722e47725691a1460d0509b63c794f5fea1a"
    http_archive(
        name = "com_github_mjbots_bazel_deps",
        url = "https://github.com/mayitayew/bazel_deps/archive/{}.zip".format(commit),
        # Try the following empty sha256 hash first, then replace with whatever
        # bazel says it is looking for once it complains.
        sha256 = "",
        strip_prefix = "bazel_deps-{}".format(commit),
    )