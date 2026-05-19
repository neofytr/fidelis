// SPDX-License-Identifier: GPL-3.0-or-later
//
// Regression tests for queue::resolve_loaded_index — the gapless-advance
// index logic. A gapless swap fires TrackLoaded for the staged next track
// while current_index_ still points at the previous one; failing to catch up
// re-preloads the same track and it repeats forever (the "queue won't move
// past song 2" bug).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <fidelis/queue/queue.hpp>

#include <filesystem>
#include <vector>

namespace q = fidelis::queue;
using std::filesystem::path;

namespace {
const std::vector<path> kAlbum = {"/m/a.flac", "/m/b.flac", "/m/c.flac"};
}

TEST_CASE("gapless swap into next track advances the index") {
    // Playing track 0; engine gapless-swaps to the staged track 1.
    CHECK(q::resolve_loaded_index(0, kAlbum, "/m/b.flac") == 1);
    // Then swaps to track 2.
    CHECK(q::resolve_loaded_index(1, kAlbum, "/m/c.flac") == 2);
}

TEST_CASE("explicit (re)load of the current track does not double-advance") {
    // append()/jump()/TrackEnded path: loaded file == tracks[current].
    CHECK(q::resolve_loaded_index(0, kAlbum, "/m/a.flac") == 0);
    CHECK(q::resolve_loaded_index(2, kAlbum, "/m/c.flac") == 2);
}

TEST_CASE("last track staying loaded does not run off the end") {
    CHECK(q::resolve_loaded_index(2, kAlbum, "/m/c.flac") == 2);
}

TEST_CASE("unrelated / jumped-to path leaves the index unchanged") {
    CHECK(q::resolve_loaded_index(0, kAlbum, "/m/elsewhere.flac") == 0);
}

TEST_CASE("path normalisation is applied to the match") {
    CHECK(q::resolve_loaded_index(0, kAlbum, "/m/./b.flac") == 1);
    CHECK(q::resolve_loaded_index(0, {"/m/x/../a.flac", "/m/b.flac"},
                                  "/m/b.flac") == 1);
}

TEST_CASE("empty queue / negative index are safe") {
    CHECK(q::resolve_loaded_index(-1, {}, "/m/a.flac") == -1);
    CHECK(q::resolve_loaded_index(-1, kAlbum, "/m/a.flac") == 0);
}
