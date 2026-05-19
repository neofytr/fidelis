// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for the session snapshot module. JSON round-trip; file load + save
// (atomic rename); tolerant deserialise of partial / forward-compatible JSON.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <fidelis/session/session.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace s = fidelis::session;

namespace {
std::filesystem::path tmp_path(const char* name) {
    std::filesystem::path p = std::filesystem::temp_directory_path();
    p /= std::string("fidelis_session_") + name + ".json";
    std::error_code ec;
    std::filesystem::remove(p, ec);
    return p;
}
}

TEST_CASE("serialise/deserialise round-trip preserves every field") {
    s::Snapshot a;
    a.tracks = {"/m/a.flac", "/m/b.flac", "/m/c.flac"};
    a.current_index = 1;
    a.position_frames = 132300; // 3s @ 44.1k
    a.version = 1;

    const std::string text = s::serialize(a);
    REQUIRE_FALSE(text.empty());
    auto b = s::deserialize(text);
    REQUIRE(b.has_value());
    CHECK(b->tracks == a.tracks);
    CHECK(b->current_index == a.current_index);
    CHECK(b->position_frames == a.position_frames);
    CHECK(b->version == a.version);
}

TEST_CASE("deserialise tolerates a minimal { tracks: [] } payload") {
    auto b = s::deserialize("{\"tracks\":[]}");
    REQUIRE(b.has_value());
    CHECK(b->tracks.empty());
    CHECK(b->current_index == -1);
    CHECK(b->position_frames == 0u);
}

TEST_CASE("deserialise rejects malformed JSON") {
    CHECK_FALSE(s::deserialize("{").has_value());
    CHECK_FALSE(s::deserialize("not-json").has_value());
    CHECK_FALSE(s::deserialize("[1,2,3]").has_value()); // wrong root type
}

TEST_CASE("deserialise rejects a non-string entry in tracks") {
    CHECK_FALSE(s::deserialize("{\"tracks\":[42]}").has_value());
}

TEST_CASE("save() + load() round-trip on a real file") {
    const auto path = tmp_path("roundtrip");
    s::Snapshot a;
    a.tracks = {"/x/y.flac"};
    a.current_index = 0;
    a.position_frames = 99;
    REQUIRE(s::save(path, a));
    REQUIRE(std::filesystem::exists(path));
    auto b = s::load(path);
    REQUIRE(b.has_value());
    CHECK(b->tracks == a.tracks);
    CHECK(b->current_index == 0);
    CHECK(b->position_frames == 99u);
    std::filesystem::remove(path);
}

TEST_CASE("save() is atomic: tmp sibling is not left behind") {
    const auto path = tmp_path("atomic");
    s::Snapshot a;
    a.tracks = {"/q.flac"};
    REQUIRE(s::save(path, a));
    auto tmp = std::filesystem::path(path.string() + ".tmp");
    CHECK_FALSE(std::filesystem::exists(tmp));
    std::filesystem::remove(path);
}

TEST_CASE("load() returns nullopt on a missing file") {
    const auto path = tmp_path("missing"); // remove already done
    CHECK_FALSE(s::load(path).has_value());
}

TEST_CASE("load() returns nullopt on a corrupt file (never crashes)") {
    const auto path = tmp_path("corrupt");
    {
        std::ofstream o(path);
        o << "this is not json";
    }
    CHECK_FALSE(s::load(path).has_value());
    std::filesystem::remove(path);
}
