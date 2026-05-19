// SPDX-License-Identifier: GPL-3.0-or-later
//
// Regression: the library scanner used to delete every track row whose file
// returned exists()=false, which wiped the library when a removable volume
// (e.g. /mnt/win) was unmounted at scan time. The sweep now consults the set
// of roots that were actually walked this run; tracks under unwalked roots
// are protected. This file tests the pure predicate directly.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "../../src/library/scanner.hpp"

#include <filesystem>
#include <vector>

namespace lib = fidelis::library;
using path = std::filesystem::path;

TEST_CASE("path under a walked root is sweepable") {
    std::vector<path> walked = {"/home/raj/Music"};
    CHECK(lib::path_under_any_root("/home/raj/Music/x/y.flac", walked));
    CHECK(lib::path_under_any_root("/home/raj/Music/y.flac", walked));
}

TEST_CASE("path under an UNwalked root is protected") {
    // /mnt/win was not present at scan time → not in walked.
    std::vector<path> walked = {"/home/raj/Music"};
    CHECK_FALSE(lib::path_under_any_root("/mnt/win/Music/x.flac", walked));
    CHECK_FALSE(lib::path_under_any_root("/mnt/win", walked));
}

TEST_CASE("an empty walked-roots list protects every path") {
    std::vector<path> walked;
    CHECK_FALSE(lib::path_under_any_root("/home/raj/Music/x.flac", walked));
    CHECK_FALSE(lib::path_under_any_root("/anything", walked));
}

TEST_CASE("prefix collision does not yield a false positive") {
    // "/home/raj/Music" is a prefix string of "/home/raj/Music2" but is
    // NOT an ancestor on the filesystem.
    std::vector<path> walked = {"/home/raj/Music"};
    CHECK_FALSE(lib::path_under_any_root("/home/raj/Music2/x.flac", walked));
}

TEST_CASE("an exact-root match is treated as under-root") {
    std::vector<path> walked = {"/srv/music"};
    CHECK(lib::path_under_any_root("/srv/music", walked));
}

TEST_CASE("multiple roots are honoured independently") {
    std::vector<path> walked = {"/a", "/b/c"};
    CHECK(lib::path_under_any_root("/a/song.flac", walked));
    CHECK(lib::path_under_any_root("/b/c/song.flac", walked));
    CHECK_FALSE(lib::path_under_any_root("/b/song.flac", walked));
}

TEST_CASE("trailing slash on a root is tolerated") {
    std::vector<path> walked = {"/srv/music/"};
    CHECK(lib::path_under_any_root("/srv/music/x.flac", walked));
}
