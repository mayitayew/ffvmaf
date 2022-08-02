load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")


def bazel_deps_repository():
    commit = "4a2b308a03ccd20513c1aa7e413debbc0b8366e8"
    http_archive(
        name = "com_github_mjbots_bazel_deps",
        url = "https://github.com/mjbots/bazel_deps/archive/{}.zip".format(commit),
        # Try the following empty sha256 hash first, then replace with whatever
        # bazel says it is looking for once it complains.
        sha256 = "2304d22ac96bb3ab3a38821e018d7768d7f51d65f84103b94569513c3d66f775",
        strip_prefix = "bazel_deps-{}".format(commit),
    )